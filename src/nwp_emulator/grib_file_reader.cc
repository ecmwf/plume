/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <typeinfo>

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"
#include "eckit/mpi/Comm.h"

#include "grib_file_reader.h"
#include "nwp_definitions.h"

namespace nwp_emulator {
void codesHandleDeleter::operator()(codes_handle* h) {
    if (h) {
        codes_handle_delete(h);
    }
}

void fileDeleter::operator()(FILE* f) {
    if (f) {
        fclose(f);
    }
}

GRIBFileReader::GRIBFileReader(const eckit::PathName& inputPath, size_t rank, size_t root, int stepCountLimit) :
    rank_(rank), root_(root), DataReader(stepCountLimit) {
    if (rank_ != root_) {
        gridName_     = "";
        params_       = {};
        srcFilenames_ = {};
    }
    else {
        setupReader(inputPath, rank_ == root_);
        validateSrcFiles(rank_ == root_);
    }
    // Broadcast emulator source params from main reader (root)
    // 1. Grid identifier
    size_t gridNameSize = gridName_.size();
    eckit::mpi::comm().broadcast(gridNameSize, root_);
    if (rank_ != root_) {
        gridName_.resize(gridNameSize);
    }
    eckit::mpi::comm().broadcast(gridName_.begin(), gridName_.end(), root_);
    // 2. Field names & metadata (parameters)
    std::string paramBuffer;
    if (rank_ == root_) {
        for (const auto& param : params_) {
            paramBuffer += param + ";";
        }
        paramBuffer.pop_back();  // remove last ';' separator
    }
    size_t paramSize = paramBuffer.size();
    eckit::mpi::comm().broadcast(paramSize, root_);
    if (rank_ != root_) {
        paramBuffer.resize(paramSize);
    }
    // broadcast a single string to limit communication
    eckit::mpi::comm().broadcast(paramBuffer.begin(), paramBuffer.end(), root_);
    if (rank_ != root_) {
        std::stringstream ss(paramBuffer);
        std::string param;
        while (std::getline(ss, param, ';')) {
            params_.push_back(param);
        }
    }
    // 3. Number of model steps (files in the source folder)
    size_t fileCount = srcFilenames_.size();
    eckit::mpi::comm().broadcast(fileCount, root_);
    if (rank_ != root_) {
        srcFilenames_.resize(fileCount);
    }
}

void GRIBFileReader::setupReader(const eckit::PathName& inputPath, bool isRoot) {
    if (!isRoot) {
        eckit::Log::warning() << "Only root process is allowed to read, skipping..." << std::endl;
        return;
    }
    eckit::PathName srcPath;
    std::vector<eckit::PathName> skippedDirs;
    if (!inputPath.exists()) {
        eckit::Log::error() << inputPath << " does not exist" << std::endl;
        eckit::mpi::comm().abort(1);
    }
    std::vector<eckit::PathName> listFilenames;
    // Open source directory and store source file names
    // Each file is assumed to store data for a single model step
    // (regardless of the actual time in the messages)
    if (!inputPath.isDir()) {
        inputPath.dirName().children(listFilenames, skippedDirs);  // Use the dirname as dir and file as source
    }
    else {
        inputPath.children(listFilenames, skippedDirs);  // input path is a dir
    }
    // Remove any file that is not GRIB
    for (size_t i = 0; i < listFilenames.size(); ++i) {
        if ((listFilenames[i].extension() != ".grib") && (listFilenames[i].extension() != ".grib1") &&
            (listFilenames[i].extension() != ".grib2")) {
            continue;
        }
        srcFilenames_.push_back(listFilenames[i]);
    }
    if (srcFilenames_.empty()) {
        eckit::Log::error() << "No GRIB files remained in the source folder after filtering, exit..." << std::endl;
        eckit::mpi::comm().abort(1);
    }
    // Steps should be stored in alphabetical order
    std::sort(srcFilenames_.begin(), srcFilenames_.end());
    srcPath = eckit::PathName(srcFilenames_[0]);
    if (srcFilenames_.size() > stepCountLimit_) {
        eckit::Log::warning() << "The emulator is limited to " << stepCountLimit_ << " steps, "
                              << srcFilenames_.size() - stepCountLimit_
                              << " steps from the source directory will be skipped" << std::endl;
    }
    else {
        eckit::Log::info() << "The emulator will attempt to run " << srcFilenames_.size() << " steps" << std::endl;
    }
    if (!skippedDirs.empty()) {
        eckit::Log::info() << "Any file in children directories will be ignored..." << std::endl;
    }
    // For later consistency checks & emulator setup :
    openGribFile(srcPath, true);
    eckit::Log::info() << "Number of messages : " << std::to_string(count_) << std::endl;
    char buffer[64];
    size_t size = sizeof(buffer);
    // 1. Lock the grid name from the passed GRIB file
    if (codes_get_string(grib(), "gridName", buffer, &size) == GRIB_NOT_FOUND) {
        eckit::Log::error() << "Grid type unsupported at the moment, exit" << std::endl;
        eckit::mpi::comm().abort(1);
    }
    eckit::Log::info() << "Grid identifier : " << std::string(buffer) << std::endl;
    gridName_ = std::string(buffer);
    // 2. Lock the parameter names
    std::string paramMd, _;
    eckit::Log::info() << "Params (name,levtype,level) : ";
    for (int i = 0; i < count_ - 1; ++i) {
        readMsgMetadata(_, paramMd);
        params_.push_back(paramMd);
        eckit::Log::info() << params_[i] << "; ";
        if (!resetGribHandle()) {
            eckit::Log::error() << " Check source file " << srcPath << std::endl;
            eckit::mpi::comm().abort(1);
        }
    }
    readMsgMetadata(_, paramMd);
    params_.push_back(paramMd);
    eckit::Log::info() << paramMd;
    std::set<std::string> paramSet(params_.begin(), params_.end());
    if (paramSet.size() != params_.size()) {
        eckit::Log::error() << "(param, levtype, level) combinations must be unique in each source file as they "
                            << " represent a single time step, exit..." << std::endl;
        eckit::mpi::comm().abort(1);
    }
    eckit::Log::info() << std::endl;
    closeGribFile();
}

void GRIBFileReader::validateSrcFiles(bool isRoot) {
    if (!isRoot) {
        eckit::Log::warning() << "Only root process is allowed to read, skipping..." << std::endl;
        return;
    }
    std::string gridName, paramMd;
    // Do not sort the `params_` directly as levels should be provided as they are encoded in the source file
    std::vector<std::string> sortedParams = params_;
    std::sort(sortedParams.begin(), sortedParams.end());
    std::vector<std::string> fileParams;
    for (const auto& filename : srcFilenames_) {
        fileParams.clear();
        openGribFile(filename, true);
        for (int i = 0; i < count_ - 1; ++i) {
            readMsgMetadata(gridName, paramMd);
            if (gridName != gridName_) {
                eckit::Log::error() << "Grid in " << filename << " is different from setup (" << gridName_
                                    << "), exit..." << std::endl;
                eckit::mpi::comm().abort(1);
            }
            fileParams.push_back(paramMd);
            if (!resetGribHandle()) {
                eckit::Log::error() << " Check source file " << filename << std::endl;
                eckit::mpi::comm().abort(1);
            }
        }
        readMsgMetadata(gridName, paramMd);  // read the last message in the file
        if (gridName != gridName_) {
            eckit::Log::error() << "Grid in " << filename << " is different from setup (" << gridName_ << "), exit..."
                                << std::endl;
            eckit::mpi::comm().abort(1);
        }
        fileParams.push_back(paramMd);

        std::sort(fileParams.begin(), fileParams.end());
        if (fileParams != sortedParams) {
            eckit::Log::error() << "Parameters and levels in " << filename
                                << " are inconsistent with other source files, exit..." << std::endl;
            eckit::mpi::comm().abort(1);
        }
        closeGribFile();
    }
}

void GRIBFileReader::readMsgMetadata(std::string& gridName, std::string& paramMd) {
    char buffer[64];
    size_t size    = sizeof(buffer);
    gridName       = "";
    int gridStatus = codes_get_string(grib(), "gridName", buffer, &size);
    if (gridStatus != GRIB_NOT_FOUND) {
        gridName = std::string(buffer);
    }
    paramMd                           = "";
    std::vector<const char*> keywords = {"shortName", "levtype", "level"};
    for (const auto& keyword : keywords) {
        std::memset(buffer, 0, 64);
        size = sizeof(buffer);
        codes_get_string(grib(), keyword, buffer, &size);
        paramMd.append(buffer);
        paramMd.append(",");  // separator
    }
    paramMd.pop_back();  // remove last separator
}

GRIBFileReader::~GRIBFileReader() {
    closeGribFile();
}

bool GRIBFileReader::openGribFile(const eckit::PathName& path, bool exit) {
    // No check for existence here because the paths passed belong to the src dir
    // It is assumed they will not be deleted during runtime
    // No check for file format as checked during construction
    currentFile_.reset(fopen(path.asString().c_str(), "rb"));
    if (!currentFile_) {
        eckit::Log::error() << "Could not open file " << path << std::endl;
        if (exit) {
            eckit::mpi::comm().abort(1);
        }
        return false;  // defer error handling
    }
    index_ = 0;
    codes_count_in_file(nullptr, currentFile(), &count_);
    if (!resetGribHandle()) {
        if (exit) {
            eckit::mpi::comm().abort(1);
        }
        return false;
    }
    return true;
}

bool GRIBFileReader::resetGribHandle() {
    int err;
    // This calls the deleter of the previous handle first
    grib_.reset(codes_handle_new_from_file(nullptr, currentFile(), PRODUCT_GRIB, &err));
    if (!grib_) {
        eckit::Log::error() << "Could not create grib handle. Error: " << err << std::endl;
        return false;
    }
    return true;
}

void GRIBFileReader::closeGribFile() {
    count_ = 0;
    index_ = 0;
    // Calls the custom destructors
    currentFile_.reset(nullptr);
    grib_.reset(nullptr);
}

bool GRIBFileReader::done() {
    return (step_ >= srcFilenames_.size() || step_ >= stepCountLimit_);
}

bool GRIBFileReader::nextMessage(std::string& shortName, std::string& levtype, std::string& level,
                                 std::vector<FIELD_TYPE_REAL>& data) {
    if ((index_ == count_ && index_ != 0) || done()) {
        // All messages in the current file have been decoded, increase step
        // or all files in source dir have been read, return
        closeGribFile();
        ++step_;
        return false;
    }
    // Open next file if beginning of the step
    int result = 1;
    if (rank_ == root_ && !currentFile_) {
        if (!openGribFile(srcFilenames_[step_], false)) {
            eckit::Log::error() << "An error occurred while reading: " << srcFilenames_[step_] << std::endl;
            result = 0;
        }
    }
    eckit::mpi::comm().broadcast(result, root_);  // use int as bool can't be broadcast by eckit mpi
    if (!result) {
        closeGribFile();
        ++step_;
        return false;
    }
    // Decode the next message metadata and raw values
    if (rank_ == root_) {
        std::string _, paramMd;
        readMsgMetadata(_, paramMd);
        std::stringstream ss(paramMd);
        std::getline(ss, shortName, ',');
        std::getline(ss, levtype, ',');
        std::getline(ss, level, ',');

        size_t nbOfValues;
        codes_get_size(grib(), "values", &nbOfValues);
        data.resize(nbOfValues);
        if constexpr (std::is_same_v<FIELD_TYPE_REAL, double>) {
            ASSERT((std::is_same_v<FIELD_TYPE_REAL, double>));
            codes_get_double_array(grib(), "values", reinterpret_cast<double*>(data.data()), &nbOfValues);
        }
        else {
            ASSERT((std::is_same_v<FIELD_TYPE_REAL, float>));
            codes_get_float_array(grib(), "values", reinterpret_cast<float*>(data.data()), &nbOfValues);
        }
    }
    ++index_;
    eckit::mpi::comm().broadcast(count_, root_);
    if (rank_ == root_ && index_ < count_) {
        return resetGribHandle();  // Load the next message
    }
    return true;
}
}  // namespace nwp_emulator