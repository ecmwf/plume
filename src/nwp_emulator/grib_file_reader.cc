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
#include <typeinfo>
#include <variant>

#include "atlas/array/ArrayView.h"
#include "atlas/field/Field.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/log/Log.h"

#include "grib_file_reader.h"
#include "nwp_definitions.h"

namespace nwp_emulator {
GRIBFileReader::GRIBFileReader(const std::string& path, size_t rank, size_t root, int stepCountLimit) :
    rank_(rank), root_(root), stepCountLimit_(stepCountLimit) {
    if (rank_ != root_) {
        gridName_ = "";
        params_ = {};
        srcFilenames_ = {};
    }
    else {
        setupReader(path, rank_ == root_);
    }
    // Broadcast emulator source params from main reader (root)
    // 1. Grid identifier
    std::vector<char> gridNameBuffer(10);  // typical grid names are 4-6 chars
    if (rank_ == root_) {
        gridNameBuffer.assign(gridName_.begin(), gridName_.end());
    }
    eckit::mpi::comm().broadcast(gridNameBuffer, root_);
    if (rank_ != root_) {
        gridName_ = std::string(gridNameBuffer.begin(), gridNameBuffer.end());
        gridName_.resize(strlen(gridName_.c_str()));
    }
    // 2. Field names (parameters)
    size_t paramCount = params_.size();
    eckit::mpi::comm().broadcast(paramCount, root_);
    std::vector<char> paramBuffer(10);  // Params short name are typically 1-4 chars
    for (size_t i = 0; i < paramCount; ++i) {
        paramBuffer.resize(10, '\0');  // clean param buffer
        if (rank_ == root_) {
            paramBuffer.assign(params_[i].begin(), params_[i].end());
        }
        eckit::mpi::comm().broadcast(paramBuffer, root_);
        if (rank_ != root_) {
            std::string param(paramBuffer.begin(), paramBuffer.end());
            param.resize(strlen(param.c_str()));  // Trim trailing '\0's
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

void GRIBFileReader::setupReader(const std::string& path, bool isRoot) {
    if (!isRoot) {
        eckit::Log::warning() << "Only root process is allowed to read, skipping..." << std::endl;
        return;
    }
    eckit::PathName inputPath{path};
    eckit::PathName srcPath;
    std::vector<eckit::PathName> skippedDirs;
    if (!inputPath.exists()) {
        eckit::Log::error() << path << " does not exist" << std::endl;
        std::exit(1);
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
        if ((listFilenames[i].extension() != ".grib") && (listFilenames[i].extension() != ".grib1")
            && (listFilenames[i].extension() != ".grib2")) {
            continue;
        }
        srcFilenames_.push_back(listFilenames[i]);
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
    openGribFile(srcPath);
    eckit::Log::info() << "Number of messages : " << std::to_string(count_) << std::endl;
    char buffer[64];
    size_t size = sizeof(buffer);
    // 1. Lock the grid name from the passed GRIB file
    codes_get_string(grib_, "gridName", buffer, &size);
    eckit::Log::info() << "Grid identifier : " << std::string(buffer) << std::endl;
    gridName_ = std::string(buffer);
    // 2. Lock the parameter names
    int err;
    eckit::Log::info() << "Params : ";
    for (int i = 0; i < count_; ++i) {
        std::memset(buffer, 0, size);
        size = sizeof(buffer);
        codes_get_string(grib_, "shortName", buffer, &size);
        params_.push_back(std::string(buffer));
        eckit::Log::info() << params_[i] << "; ";
        if (i == count_ - 1) {
            // Skip reading next message if we reached the end of the count
            continue;
        }
        codes_handle_delete(grib_);
        grib_ = codes_handle_new_from_file(nullptr, currentFile_, PRODUCT_GRIB, &err);
        if (!grib_) {
            eckit::Log::error() << "Could not create grib handle. Error: " << err << " Skipping... " << std::endl;
        }
    }
    eckit::Log::info() << std::endl;
    closeGribFile();
}

GRIBFileReader::~GRIBFileReader() {
    closeGribFile();
}

bool GRIBFileReader::openGribFile(const eckit::PathName& path) {
    // No check for existence here because the paths passed belong to the src dir
    // It is assumed they will not be deleted during runtime
    // No check for file format as checked during construction
    currentFile_ = fopen(path.asString().c_str(), "rb");
    if (!currentFile_) {
        eckit::Log::error() << "Could not open file " << path << std::endl;
        if (gridName_.empty()) {
            // The file passed for setup cannot be opened, crash
            std::exit(1);
        }
        // The file passed at runtime cannot be opened, defer error handling
        return false;
    }
    codes_count_in_file(nullptr, currentFile_, &count_);
    int err;
    grib_ = codes_handle_new_from_file(nullptr, currentFile_, PRODUCT_GRIB, &err);
    if (!grib_) {
        eckit::Log::error() << "Could not create grib handle. Error: " << err << std::endl;
        if (gridName_.empty()) {
            // The file passed for setup cannot be decoded, crash
            std::exit(1);
        }
        // The file passed at runtime cannot be decoded, defer error handling
        return false;
    }
    return true;
}

void GRIBFileReader::closeGribFile() {
    if (grib_) {
        codes_handle_delete(grib_);
    }
    if (currentFile_) {
        fclose(currentFile_);
        count_ = 0;
    }
    // To avoid segfault at destruction if they have already been dereferenced
    currentFile_ = nullptr;
    grib_ = nullptr;
}

bool GRIBFileReader::hasReadAll() {
    return (step_ >= srcFilenames_.size() || step_ >= stepCountLimit_);
}

bool GRIBFileReader::openNextFile() {
    if (rank_ != root_) {
        eckit::Log::warning() << "Only root reader is allowed to read files, skipping..." << std::endl;
        return false;
    }
    bool result = true;
    // Open the GRIB file corresponding to the current step
    if (!openGribFile(srcFilenames_[step_])) {
        // Skip this file
        eckit::Log::error() << "An error occurred while reading: " << srcFilenames_[step_] << std::endl;
        closeGribFile();
        return false;
    }
    // Run some validity checks on the file
    char buffer[64];
    size_t size = sizeof(buffer);
    codes_get_string(grib_, "gridName", buffer, &size);
    if (std::string(buffer) != gridName_) {
        eckit::Log::error() << "Data for step " << step_ << "are on grid " << buffer << " instead of " << gridName_
                            << ", skipping step..." << std::endl;
        result = false;
    }
    if (!result) {
        // Note: extracting this here in case more checks are run in the future
        closeGribFile();
        return result;
    }
}

bool GRIBFileReader::readNextStep(std::vector<atlas::Field>& fields, std::vector<atlas::Field>& glb_fields) {
    int result = 1;
    // Open the current file or skip
    if (rank_ == root_) {
        result = static_cast<int>(openNextFile());
    }
    eckit::mpi::comm().broadcast(result, root_); // use int as bool can't be broadcast by eckit mpi
    if (!result) {
        ++step_;
        return false;
    }
    // Broadcast the number of messages in the current file for looping
    eckit::mpi::comm().broadcast(count_, root_);
    char buffer[10];
    size_t size = sizeof(buffer);
    std::vector<char> vecBuffer(size);
    for (int msg_id = 0; msg_id < count_; ++msg_id) {
        std::memset(buffer, 0, 10);
        size = sizeof(buffer);
        vecBuffer.resize(size, '\0');
        if (rank_ == root_) {
            codes_get_string(grib_, "shortName", buffer, &size);
            vecBuffer.assign(buffer, buffer + size);
        }
        eckit::mpi::comm().broadcast(vecBuffer, root_);
        std::string param(vecBuffer.begin(), vecBuffer.end());
        param.resize(strlen(param.c_str()));
        // Find the field name corresponding to the current message and populate it with its data
        auto it = std::find(params_.begin(), params_.end(), param);
        if (it != params_.end()) {
            std::variant<std::vector<float>, std::vector<double>> values; // support single and double precision
            if (rank_ == root_) {
                size_t nbOfValues;
                codes_get_size(grib_, "values", &nbOfValues);
                if (std::is_same<FIELD_TYPE_REAL, double>::value) {
                    values = std::vector<double>(nbOfValues);
                    codes_get_double_array(grib_, "values", std::get<std::vector<double>>(values).data(), &nbOfValues);
                }
                else {
                    values = std::vector<float>(nbOfValues);
                    codes_get_float_array(grib_, "values", std::get<std::vector<float>>(values).data(), &nbOfValues);
                }
            }
            // If the data source is the same for all steps, i and fieldIdx should match
            int fieldIdx = std::distance(params_.begin(), it);
            glb_fields[fieldIdx].functionspace().gather(fields[fieldIdx], glb_fields[fieldIdx]);
            if (rank_ == root_) {
                auto source = atlas::array::make_view<FIELD_TYPE_REAL, 2>(glb_fields[fieldIdx]);
                for (atlas::idx_t i_pt = 0; i_pt < glb_fields[fieldIdx].size(); ++i_pt) {
                    source(i_pt, 0) = std::get<std::vector<FIELD_TYPE_REAL>>(values)[i_pt - 1];  // TODO: levels ?
                }
            }
            glb_fields[fieldIdx].functionspace().scatter(glb_fields[fieldIdx], fields[fieldIdx]);
        }
        // Else the current field was not in the initial setup, skip
        if (rank_ == root_) {
            int err;
            if (msg_id != count_ - 1) {
                // Keep reading the next message
                codes_handle_delete(grib_);
                grib_ = codes_handle_new_from_file(nullptr, currentFile_, PRODUCT_GRIB, &err);
            }
            if (!grib_) {
                eckit::Log::error() << "Could not create grib handle. Error: " << err << " Skipping..." << std::endl;
                result = false;
                break;
            }
        }
    }
    if (rank_ == root_) {
        closeGribFile();
    }
    ++step_;
    return static_cast<bool>(result);
}
}  // namespace nwp_emulator