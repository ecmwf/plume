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

#include <string>
#include <vector>

#include "eccodes.h"
#include "eckit/filesystem/PathName.h"

#include "atlas/field/Field.h"

namespace nwp_emulator {

/**
 * @class GRIBFileReader
 * @brief Handles reading of GRIB data and serves them as Atlas fields. Supports parallel environments.
 *
 * This class provides inputs for a NWP emulator sourced from historical data stored in GRIB format.
 * In parallel, only the root process is responsible for doing the reading to limit IO and unstability
 * due to concurrent access to source files. Atlas is used to then scatter the data across the partitions,
 * each responsible for its own share of the fields. The main reader construction determines the emulator
 * parameters : folder from which to source the GRIB files (each file is considered as a  single model step),
 * number of emulator steps (smallest between file count and default/user limit), parameters offered by the
 * model & grid (derived from the first file in the source folder content).
 */
class GRIBFileReader {
private:
    std::string gridName_;
    std::vector<std::string> params_;
    std::vector<eckit::PathName> srcFilenames_;
    int stepCountLimit_;
    int count_{0};
    int step_{0};
    FILE* currentFile_ = nullptr;
    codes_handle* grib_ = nullptr;

    size_t rank_;
    size_t root_;

    /**
     * @brief Opens the current file, and loads the first GRIB message.
     *
     * This method stores the number of messages in the currently open file in count_.
     *
     * @param path The path of the file to open.
     *
     * @return true if opening and decoding of first message successful, false otherwise.
     */
    bool openGribFile(const eckit::PathName& path);

    // Opens file for the current step and performs some sanity checks on it
    bool openNextFile();

    // Closes current file, and resets pointers.
    void closeGribFile();

    // Called from the constructor by the main reader
    void setupReader(const std::string& path, bool isRoot);

public:
    /**
     * @brief Constructs a GRIBFileReader object and sets up emulator params if it is the main reader.
     *
     * The secondary readers, which only receive their share of the data through Atlas fields,
     * initialise the emulator options to empty defaults and wait for the main reader to broadcast
     * the necessary params.
     *
     * @param path The path to the source folder (or any file within it).
     * @param rank The rank of the current process.
     * @param root The rank of the root process.
     * @param stepCountLimit The number of maximum steps to run in the emulator. Defaults to 100.
     *                       Any source file beyond that number will be skipped.
     *
     * @note The constructor uses std::exit() in case the path passed does not allow to properly
     *       perform the emulator setup.
     */
    GRIBFileReader(const std::string& path, size_t rank, size_t root, int stepCountLimit = 100);

    // Makes sure open files have been closed, and pointers not dereferenced already before destroying.
    ~GRIBFileReader();

    // Getters
    const std::string& getGridName() const { return gridName_; }
    int getStep() const { return step_; }
    const std::vector<std::string>& getParams() const { return params_; }

    /**
     * @brief Decodes the GRIB data from the next source file, and scatters it across partitionned fields.
     *
     * This method reads the next GRIB file in the source folder, performs some sanity checks on it
     * to ensure its consistency with the emulator setup (presence of required parameters, same grid),
     * and populates the Atlas fields with the data. The step_ counter is updated for all processes
     * (main and secondary) to ensure emulator termination.
     *
     * @param fields The Atlas fields to populate with the data read from the current source file.
     * @param glb_fields The global Atlas fields used to correctly scatter data across partitions.
     *
     * @return true if opening and decoding of first message successful, false otherwise.
     */
    bool readNextStep(std::vector<atlas::Field>& fields, std::vector<atlas::Field>& glb_fields);

    // Returns true when the GRIB file reader is done or has reached its limit
    bool hasReadAll();
};

}  // namespace nwp_emulator
