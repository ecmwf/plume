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

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atlas/field/Field.h"
#include "atlas/util/Point.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/log/Log.h"
#include "eckit/mpi/Comm.h"

#include "base_data_reader.h"

namespace nwp_emulator {

/**
 * @brief A structure that represents a rectangular area on the world map.
 *
 * This struct holds the coordinates of 2 points lon,lat points. It assumes the values passed describe a
 * valid rectangle.
 */
struct FocusArea {
    double west, east, south, north;

    /**
     * @brief Constructs a focus area.
     *
     * The list initialisation is incompatible with the use of unique pointers, hence the need for a constructor.
     *
     * @param west The west longitude of the area box.
     * @param east The east longitude of the area box.
     * @param south The south latitude of the area box.
     * @param north The north latitude of the area box.
     */
    FocusArea(double west, double east, double south, double north) :
        west(west), east(east), south(south), north(north) {}

    /**
     * @brief Determines whether a certain point belongs to the rectangle described by the area.
     *
     * This method supports an area containing the longitude 0/360 so it is possible that the west longitude
     * is not smaller than the east longitude.
     *
     * @param p The atlas point. It is assumed that latitudes range between [-90,90], and longitudes [0,360].
     */
    bool contains(const atlas::PointLonLat& p) {
        if (west < east) {
            return (p.lat() >= south && p.lat() <= north && p.lon() >= west && p.lon() <= east);
        }
        return (p.lat() >= south && p.lat() <= north && ((p.lon() >= west && p.lon() <= 360.0) || (p.lon() <= east)));
    }
};

/**
 * @class ConfigReader
 * @brief Reads a configuration and generates data based on it that it serves as Atlas fields.
 *
 * Supports parallel environments. Each process is responsible for generating its own share of the data.
 * This class provides inputs for a NWP emulator generated from a set of functions that can be broaden as needed.
 * It currently supports five generation functions: vortex rollup, random (uniform or normal), step, cardinal sine,
 * and gaussian. The configuration file is used at construction to determine the emulator parameters :
 * number of emulator steps, grid, vertical levels, parameters offered by the model, and how each field should
 * be populated. An example of configuration file can be found in the README.
 */
class ConfigReader final : public BaseDataReader {
private:
    eckit::LocalConfiguration readerConfig_;
    atlas::Field readerArea_;
    int modelLevels_;

    size_t rank_;
    size_t root_;

    /**
     * @brief Finds the subconfiguration key that matches a particular level.
     *
     * This method is used by the reader to resolve in the configuration where the instructions are to populate
     * a given level for a given field.
     *
     * @param level The model level to find the key for.
     * @param levelConfigStr The path in the configuration where the possible level keys are.
     *
     * @return The configuration key covering to the level.
     */
    std::string getLevelConfigKey(const std::string& level, const std::string& levelConfigStr);

    /**
     * @brief Randomly sample (lon,lat) points based on provided options.
     *
     * Only the root reader does the sampling, and broadcasts the result so every process uses a consistent
     * set of centres for a given time step. Options contain a number of centres, and an area.
     *
     * @param options The configuration containing information for the sampling.
     * @param[out] centres The vector of sampled lon,lat points.
     */
    void sampleNCentres(const eckit::LocalConfiguration& options, std::vector<atlas::PointLonLat>& centres);

    /**
     * @brief Sets a focus area on the map based on the provided options.
     *
     * Options can contain an area which is assumed to be formatted as [NW_lat, NW_lon, SE_lat, SE_lon],
     * and a translation vector if the area should be displaced at every time step. The struct returned has
     * a convenience method to determine belonging.
     *
     * @param options The configuration containing information about the area.
     *
     * @return The rectangular area of the map or a null pointer if the whole globe should be considered.
     */
    std::unique_ptr<FocusArea> setFocusArea(const eckit::LocalConfiguration& options);

    /// Fills the values vector with a vortex rollup parameterised by the given options.
    void applyVortexRollup(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values);

    /// Fills the values vector with random values parameterised by the given options.
    void applyRandom(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values);

    /// Fills the values vector with a step value parameterised by the given options.
    void applyStep(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values);

    /// Fills the values vector with a cardinal sine parameterised by the given options.
    void applyCardinalSine(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values);

    /// Fills the values vector with a gaussian parameterised by the given options.
    void applyGaussian(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values);

    /**
     * @brief Validates the passed emulator configuration.
     *
     * Runs a deeper validation than the one run directly in the constructor. Checks the the fields are configured
     * properly. Some checks include (but are not limited too, see the code for detailed comments):
     *  - 'apply' has at least one key and all keys are valid functions.
     *  - All levels do not require to have an 'apply' key, but at least one should (other levels are 0s).
     *  - All level keys are valid (0 is invalid, ml keys should cover 1-<n_ml> at most).
     *  - The 'levels' key usage is correct (incompatible with surface fields or emulators with one model level).
     *  - Each function has the options it needs & they are valid (value/types).
     *  - Distributions asked are all supported.
     *  - Areas are well defined (NW,SE and lat [-90,90] and lon [-180,180])
     *  - ...
     *
     * @return True if the configuration can be used by the reader to setup and generate data for the emulator.
     *         False otherwise.
     */
    bool validateConfig();

    /// Validates that areas configured respect the [NW_lat, NW_lon, SE_lat, SE_lon] format and ranges.
    bool validateArea(const std::vector<double>& area);

    /// Validates that level keys are either single levels ("1"), sequences ("1,2,5"), or ranges (":3", "2:5", "2:")
    bool validateLevelKeys(const eckit::LocalConfiguration& config);

    /// Validates options provided for 'vortex_rollup' apply function.
    bool validateVortexRollupOpts(const eckit::LocalConfiguration& options);

    /// Validates options provided for 'random' apply function.
    bool validateRandomOpts(const eckit::LocalConfiguration& options);

    /// Validates options provided for 'step' apply function.
    bool validateStepOpts(const eckit::LocalConfiguration& options);

    /// Validates options provided for 'sinc' apply function.
    bool validateCardinalSineOpts(const eckit::LocalConfiguration& options);

    /// Validates options provided for 'gaussian' apply function.
    bool validateGaussianOpts(const eckit::LocalConfiguration& options);

    /// Lookup tables for functions and validation functions
    std::unordered_map<std::string,
                       std::function<void(const eckit::LocalConfiguration&, std::vector<FIELD_TYPE_REAL>&)>>
        functionTable = {
            {"step", std::bind(&ConfigReader::applyStep, this, std::placeholders::_1, std::placeholders::_2)},
            {"random", std::bind(&ConfigReader::applyRandom, this, std::placeholders::_1, std::placeholders::_2)},
            {"vortex_rollup",
             std::bind(&ConfigReader::applyVortexRollup, this, std::placeholders::_1, std::placeholders::_2)},
            {"sinc", std::bind(&ConfigReader::applyCardinalSine, this, std::placeholders::_1, std::placeholders::_2)},
            {"gaussian", std::bind(&ConfigReader::applyGaussian, this, std::placeholders::_1, std::placeholders::_2)}};

    std::unordered_map<std::string, std::function<bool(const eckit::LocalConfiguration&)>> validationTable = {
        {"step", std::bind(&ConfigReader::validateStepOpts, this, std::placeholders::_1)},
        {"random", std::bind(&ConfigReader::validateRandomOpts, this, std::placeholders::_1)},
        {"vortex_rollup", std::bind(&ConfigReader::validateVortexRollupOpts, this, std::placeholders::_1)},
        {"sinc", std::bind(&ConfigReader::validateCardinalSineOpts, this, std::placeholders::_1)},
        {"gaussian", std::bind(&ConfigReader::validateGaussianOpts, this, std::placeholders::_1)}};

public:
    /**
     * @brief Constructs a ConfigReader object and sets up emulator params.
     *
     * Reads or derives emulator options from the configuration, which should be a YAML file.
     * All readers follow a parameter representation as follows: "<shortName>,<levtype>,<level>".
     *
     * @param inputPath The path to the emulator configuration file.
     * @param rank The rank of the current process.
     * @param root The rank of the root process.
     * @param stepCountLimit The number of maximum steps to run in the emulator. Defaults to 100.
     *
     * @note The constructor aborts the execution in case the configuration is invalid. Relevant information
     *       to correct the configuration should be output.
     */
    ConfigReader(const eckit::PathName& inputPath, size_t rank, size_t root, int stepCountLimit = 100);

    /**
     * @brief Generates data for the next field and level based on its configuration.
     *
     * This method generates data for the next parameter in the `params_` vector and increases the index.
     * Once all the parameters have had data generated, it increases the model step and resets the index.
     * Depending on the generation function, limited communication may be required between the processes,
     * but they are all responsible for generating data for their own partition.
     *
     * @param[out] shortName The name of the parameter that data is generated for for the caller use.
     * @param[out] levtype The levtype of the parameter for the caller use.
     * @param[out] level The level of the parameter for the caller use.
     * @param[out] data The vector that contains the raw values for the emulator field.
     *
     * @return true if the generation is successful, false otherwise or when the model step is complete.
     */
    bool nextMessage(std::string& shortName, std::string& levtype, std::string& level,
                     std::vector<FIELD_TYPE_REAL>& data) override;

    /// Returns true when the reader has generated data for all the steps.
    bool done() override;

    /**
     * @brief Stores the lon,lat field for which the current partition is responsible. (Setter)
     *
     * This field is used by the generation functions to generate the expected number of data points,
     * and for direct use if functions image depend on the longitude and latitude.
     *
     * @param lonlat The lonlat field to clone (partitions keep the same area throughout the execution).
     */
    void setReaderArea(const atlas::Field& lonlat) override;
};
}  // namespace nwp_emulator