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
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "eccodes.h"
#include "eckit/filesystem/PathName.h"

#include "data_reader.h"

namespace nwp_emulator {

/**
 * @brief A structure for a custom deleter to manage codes handles.
 *
 * This deleter allows to use codes handles pointers as unique pointers for safer memory usage.
 */
struct codesHandleDeleter {
    void operator()(codes_handle* h);
};

/**
 * @brief A structure for a custom deleter to manage files.
 *
 * This deleter allows to use C-style file pointers as unique pointers for safer memory usage.
 * `std::fstream` is not compatible with eccodes, hence the need to stick to `FILE`.
 */
struct fileDeleter {
    void operator()(FILE* f);
};

/**
 * @class GRIBFileReader
 * @brief Handles reading of GRIB data and serves them as Atlas fields. Supports parallel environments.
 *
 * This class provides inputs for a NWP emulator sourced from historical data stored in GRIB format.
 * In parallel, only the root process is responsible for doing the reading to limit IO and unstability
 * due to concurrent access to source files. The main reader construction determines the emulator
 * parameters : folder from which to source the GRIB files (each file is considered as a single model step),
 * number of emulator steps (smallest between file count and default/user limit), parameters offered by the
 * model & grid (derived from the first file in the source folder content).
 */
class GRIBFileReader final : public DataReader {
private:
    std::vector<eckit::PathName> srcFilenames_;
    std::unique_ptr<FILE, fileDeleter> currentFile_         = nullptr;
    std::unique_ptr<codes_handle, codesHandleDeleter> grib_ = nullptr;

    size_t rank_;
    size_t root_;

    /**
     * @brief Opens the current file, and loads the first GRIB message.
     *
     * This method stores the number of messages in the currently open file in count_.
     *
     * @param path The path of the file to open.
     * @param exit Stops the execution if the to true and an error occurs.
     *
     * @return true if opening and decoding of first message successful, false otherwise.
     */
    bool openGribFile(const eckit::PathName& path, bool exit);

    /// Closes current file, and resets pointers.
    void closeGribFile();

    /// Gets the grib handle pointer.
    codes_handle* grib() { return grib_.get(); }

    /// Gets the file pointer.
    FILE* currentFile() { return currentFile_.get(); }

    /**
     * @brief Resets the GRIB handle. Decodes the next message in the current file.
     *
     * This method first resets the handle pointer using the deleter, making sure memory is properly deallocated.
     *
     * @return True if successful, false if there was an error loading the next message or if the current file
     * has had all its messages decoded already.
     */
    bool resetGribHandle();

    /// Called from the constructor by the main reader
    void setupReader(const eckit::PathName& path, bool isRoot);

    /**
     * @brief Checks that all the source files comply with emulator requirements.
     *
     * Checks that all source files contain the same grid, parameters, level types and levels.
     *
     * @param isRoot Only the root process can run the validation.
     *
     * @note Exits the execution if the input is not valid.
     */
    void validateSrcFiles(bool isRoot);

    /**
     * @brief Decodes GRIB message metadata relevant to the emulator.
     *
     * Parameters are represented by a string built as follow: <shortName>,<levtype>,<level>.
     * Each string must be unique as each file source is expected to represent a single time step.
     *
     * @param[out] gridName String to decode the grid name into.
     * @param[out] paramMd Metadata relevant to emulator options for the parameter stored in the message.
     */
    void readMsgMetadata(std::string& gridName, std::string& paramMd);

public:
    /**
     * @brief Constructs a GRIBFileReader object and sets up emulator params if it is the main reader.
     *
     * The secondary readers, which only receive their share of the data from the provider,
     * initialise the emulator options to empty defaults and wait for the main reader to broadcast
     * the necessary params.
     *
     * @param path The path to the source folder (or any file within it).
     * @param rank The rank of the current process.
     * @param root The rank of the root process.
     * @param stepCountLimit The number of maximum steps to run in the emulator. Defaults to 100.
     *                       Any source file beyond that number will be skipped.
     *
     * @note The constructor aborts the execution in case the path passed does not allow to properly
     *       perform the emulator setup.
     */
    GRIBFileReader(const eckit::PathName& inputPath, size_t rank, size_t root, int stepCountLimit = 100);

    /// Makes sure open files have been closed, and pointers not dereferenced already before destroying.
    ~GRIBFileReader();

    /**
     * @brief Decodes the GRIB data from the next message in the current source file.
     *
     * This method reads the next GRIB file in the source folder if there isn't one currently open,
     * The step_ and index_ counters are updated for all processes (main and secondary) to ensure termination.
     *
     * @param[out] shortName The name of the parameter decoded in the message for the caller use.
     * @param[out] levtype The levtype of the message for the caller use.
     * @param[out] level The level of the message for the caller use.
     * @param[out] data The vector that contains the raw values for the decoded field.
     *
     * @return true if the decoding is successful, false otherwise.
     */
    bool nextMessage(std::string& shortName, std::string& levtype, std::string& level,
                     std::vector<FIELD_TYPE_REAL>& data) override;

    /// Returns true when the GRIB file reader is done or has reached its limit
    bool done() override;
};

}  // namespace nwp_emulator
