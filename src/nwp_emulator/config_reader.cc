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
#include <regex>
#include <sstream>
#include <unordered_set>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"

#include "config_reader.h"
#include "nwp_definitions.h"

namespace nwp_emulator {
ConfigReader::ConfigReader(const eckit::PathName& inputPath, size_t rank, size_t root, int stepCountLimit) :
    rank_(rank), root_(root), DataReader(stepCountLimit) {
    if (inputPath.extension() != ".yml" && inputPath.extension() != ".yaml") {
        eckit::Log::error() << "Source '" << inputPath << "'should be a YAML file, exit..." << std::endl;
        eckit::mpi::comm().abort(1);
    }
    eckit::YAMLConfiguration config(inputPath);
    config.get("emulator", readerConfig_);
    gridName_    = readerConfig_.getString("grid_identifier");
    modelLevels_ = 1;
    try {
        stepCountLimit_ = readerConfig_.getInt("n_steps");
    }
    catch (eckit::Exception&) {
        eckit::Log::warning() << "No steps found in config, will use default (" << stepCountLimit_ << ")..."
                              << std::endl;
    }
    try {
        modelLevels_ = readerConfig_.getInt("vertical_levels");
    }
    catch (eckit::Exception&) {
        eckit::Log::warning() << "No vertical levels number found in config, will use 1 as default..." << std::endl;
    }

    if (!readerConfig_.has("fields") || !readerConfig_.isSubConfiguration("fields")) {
        eckit::Log::error() << "No fields configured, exit..." << std::endl;
        eckit::mpi::comm().abort(1);
    }
    eckit::LocalConfiguration fields = readerConfig_.getSubConfiguration("fields");
    for (const std::string& fieldName : fields.keys()) {
        eckit::LocalConfiguration fieldConfig;
        if (fields.isSubConfiguration(fieldName)) {
            fields.get(fieldName, fieldConfig);
        }
        else if (fields.isString(fieldName) && fields.isSubConfiguration(fields.getString(fieldName))) {
            // The field uses the same configuration as the field specified by its key (short name)
            fields.get(fields.getString(fieldName), fieldConfig);
        }
        else {
            eckit::Log::error() << "Field '" << fieldName << "' must be either a subconfiguration or a valid "
                                << "field with subconfiguration, exit..." << std::endl;
            eckit::mpi::comm().abort(1);
        }
        if (!fieldConfig.has("levtype")) {
            for (int level = 0; level < modelLevels_; level++) {
                params_.push_back(fieldName + ",ml," + std::to_string(level + 1));
            }
        }
        else {
            params_.push_back(fieldName + ",sfc,0");
        }
    }
    if (rank_ == root_ && !validateConfig()) {  // validation only done by root
        eckit::mpi::comm().abort(1);
    }
}

void ConfigReader::setReaderArea(const atlas::Field& lonlat) {
    readerArea_ = lonlat.clone();
}

bool ConfigReader::done() {
    return step_ >= stepCountLimit_;
}

bool ConfigReader::nextMessage(std::string& shortName, std::string& levtype, std::string& level,
                               std::vector<FIELD_TYPE_REAL>& data) {
    if (!readerArea_) {
        eckit::Log::error() << "The reader " << rank_ << " area is not set, please set before requesting data"
                            << ", exit..." << std::endl;
        eckit::mpi::comm().abort(1);
    }
    if (index_ == params_.size() || done()) {
        index_ = 0;
        ++step_;
        return false;
    }
    std::stringstream ss(params_[index_]);
    std::getline(ss, shortName, ',');
    std::getline(ss, levtype, ',');
    std::getline(ss, level, ',');

    data.resize(0);
    data.resize(readerArea_.shape(0));

    std::string fieldConfigStr = "fields.";
    eckit::LocalConfiguration fieldConfig;
    if (readerConfig_.isString("fields." + shortName)) {
        fieldConfigStr += readerConfig_.getString("fields." + shortName) + ".apply";
    }
    else {
        fieldConfigStr += shortName + ".apply";
    }
    if (readerConfig_.has(fieldConfigStr + ".levels")) {
        std::string levelKey = getLevelConfigKey(level, fieldConfigStr + ".levels");
        fieldConfigStr += ".levels." + levelKey;
    }
    readerConfig_.get(fieldConfigStr, fieldConfig);
    for (const std::string& func : fieldConfig.keys()) {
        // no check for key existence as func should be validated at this point
        functionTable[func](fieldConfig.getSubConfiguration(func), data);
    }

    ++index_;
    return true;
}

std::string ConfigReader::getLevelConfigKey(const std::string& level, const std::string& levelConfigStr) {
    // level keys can be as follow: "1", "1,3,5", "1:3", "3:", ":4"
    int nlevel = std::stoi(level);
    eckit::LocalConfiguration levelConfig;
    readerConfig_.get(levelConfigStr, levelConfig);
    for (const std::string& key : levelConfig.keys()) {
        if (key.find(',') != std::string::npos) {  // key is a sequence of levels
            std::stringstream ss(key);
            std::string keyLevel;
            while (std::getline(ss, keyLevel, ',')) {
                if (std::stoi(keyLevel) == nlevel) {
                    return key;
                }
            }
        }
        else if (key.find(':') != std::string::npos) {  // key is a range
            size_t colonPos = key.find(':');
            int lowerBound  = 1;  // level 0 is reserved for surface
            if (colonPos != 0) {  // range is not open at the beginning
                lowerBound = std::stoi(key.substr(0, colonPos));
            }
            if (nlevel >= lowerBound) {
                if (colonPos == key.length() - 1) {
                    return key;  // key is an open range at the end
                }
                int upperBound = std::stoi(key.substr(colonPos + 1));
                if (nlevel <= upperBound) {
                    return key;  // key is a closed range
                }
            }
        }
        else if (nlevel == std::stoi(key)) {  // key represents a single level
            return key;
        }
    }
    return "";  // should never happen if the config has been properly validated
}

//----------------------------------------------------------------------------------------------------------------------
// Validation methods & utils
//----------------------------------------------------------------------------------------------------------------------
bool ConfigReader::validateConfig() {
    if (readerConfig_.has("area") && !validateArea(readerConfig_.getDoubleVector("area"))) {
        eckit::Log::error() << "Provided focus area must respect the format [lat, lon, lat, lon] "
                            << " representing its Northwest and Southeast corners, exit..." << std::endl;
        return false;
    }

    eckit::LocalConfiguration fields = readerConfig_.getSubConfiguration("fields");
    for (const std::string& fieldName : fields.keys()) {
        if (!fields.isSubConfiguration(fieldName)) {
            continue;  // only validate actual subconfigurations
        }
        eckit::LocalConfiguration fieldConfig = fields.getSubConfiguration(fieldName);
        // 1. General checks
        if (!fieldConfig.has("apply") || !fieldConfig.isSubConfiguration("apply")) {
            eckit::Log::error() << "Field '" << fieldName << "' has no 'apply' configuration, exit..." << std::endl;
            return false;
        }
        if (fieldConfig.has("levtype") && fieldConfig.has("apply.levels")) {
            eckit::Log::error() << "Field '" << fieldName
                                << " is a surface field, it cannot have a 'levels' key, exit..." << std::endl;
            return false;
        }
        if (fieldConfig.has("apply.levels") && modelLevels_ == 1) {
            eckit::Log::error() << "Field '" << fieldName << " cannot use the levels key because the emulator "
                                << "has a single level, exit..." << std::endl;
            return false;
        }
        std::vector<eckit::LocalConfiguration> functionConfigs;
        if (!fieldConfig.has("apply.levels")) {
            functionConfigs.push_back(fieldConfig.getSubConfiguration("apply"));
        }
        else {
            if (!fieldConfig.isSubConfiguration("apply.levels")) {
                eckit::Log::error() << "Field '" << fieldName << "' does not have a configuration for levels, exit..."
                                    << std::endl;
                return false;
            }
            if (!validateLevelKeys(fieldConfig.getSubConfiguration("apply.levels"))) {
                eckit::Log::error() << "Field '" << fieldName << "' has invalid level keys."
                                    << " Make sure to use single level, sequence or range, "
                                    << "and that each level is covered by a single key, exit..." << std::endl;
                return false;
            }
            for (const std::string& level : fieldConfig.getSubConfiguration("apply.levels").keys()) {
                functionConfigs.push_back(fieldConfig.getSubConfiguration("apply.levels." + level));
            }
        }

        // 2. Function specific checks
        for (const auto& functionConfig : functionConfigs) {
            for (const std::string& functionName : functionConfig.keys()) {
                // Cross functions
                if (functionConfig.has(functionName + ".area") &&
                    !validateArea(functionConfig.getDoubleVector(functionName + ".area"))) {
                    eckit::Log::error() << "Area provided for '" << functionName << "' for field '" << fieldName
                                        << "' is invalid, exit..." << std::endl;
                    return false;
                }
                if (functionConfig.has(functionName + ".translation")) {
                    if (!functionConfig.isFloatingPointList(functionName + ".translation") ||
                        functionConfig.getDoubleVector(functionName + ".translation").size() != 2) {
                        eckit::Log::error() << "The 'translation' key in function '" << functionName << "' of  field '"
                                            << fieldName << "' does not respect the format [lat_trans, lon_trans], "
                                            << "exit..." << std::endl;
                        return false;
                    }
                }
                // Function specific
                if (functionTable.find(functionName) == functionTable.end() ||
                    validationTable.find(functionName) == validationTable.end()) {
                    eckit::Log::error() << "Function name '" << functionName << "' is invalid, valid names are "
                                        << "[vortex_rollup, random, step, sinc, gaussian], exit..." << std::endl;
                    return false;
                }
                if (!validationTable[functionName](functionConfig.getSubConfiguration(functionName))) {
                    eckit::Log::error() << "Options provided for '" << functionName << "' for field '" << fieldName
                                        << "' are invalid, exit..." << std::endl;
                    return false;
                }
            }
        }
    }
    eckit::Log::info() << "The emulator has accepted the provided configuration, proceeding with run..." << std::endl;
    return true;
}

bool ConfigReader::validateArea(const std::vector<double>& area) {
    if (area.size() != 4) {
        return false;
    }
    // The lats and lons are in the expected ranges of [-90,90] and [-180,180]
    for (int lon = 1; lon < 4; lon += 2) {
        if (area[lon] < -180.0 || area[lon] > 180.0) {
            return false;
        }
    }
    for (int lat = 0; lat < 4; lat += 2) {
        if (area[lat] < -90.0 || area[lat] > 90.0) {
            return false;
        }
    }
    // The points are provided from North to South
    if (area[0] < area[2]) {
        return false;
    }
    return true;
}

bool ConfigReader::validateLevelKeys(const eckit::LocalConfiguration& config) {
    std::unordered_set<int> levelsCovered;              // Keep track of levels represented by the level keys
    std::regex singleLevelRegex("^(\\d+)$");            // Keys: "1" "12"
    std::regex levelSequenceRegex("^\\d+(?:,\\d+)+$");  // Keys: "1,2,10"
    std::regex levelRangeRegex("^(\\d+):(\\d+)$|^(\\d+):$|^:(\\d+)$");  // Keys: ":5" "2:5" "5:"
    for (const std::string& level : config.keys()) {
        std::smatch matches;
        if (std::regex_search(level, matches, singleLevelRegex)) {
            int nlevel = std::stoi(matches.str(0));
            if (nlevel > modelLevels_ || nlevel <= 0 || !levelsCovered.insert(nlevel).second) {
                return false;
            }
        }
        else if (std::regex_search(level, matches, levelSequenceRegex)) {
            std::stringstream ss(level);
            std::string number;
            while (std::getline(ss, number, ',')) {
                int nlevel = std::stoi(number);
                if (nlevel > modelLevels_ || nlevel <= 0 || !levelsCovered.insert(nlevel).second) {
                    return false;
                }
            }
        }
        else if (std::regex_search(level, matches, levelRangeRegex)) {
            if (!matches.str(1).empty()) {  // it is a closed range
                int lowerBound = std::stoi(matches.str(1));
                int upperBound = std::stoi(matches.str(2));
                if (lowerBound >= upperBound || lowerBound <= 0 || upperBound > modelLevels_) {
                    return false;
                }
                for (size_t i = lowerBound; i <= upperBound; i++) {
                    if (!levelsCovered.insert(i).second) {
                        return false;
                    }
                }
            }
            else {  // it is an open range
                int bound = matches.str(3).empty() ? std::stoi(matches.str(4)) : std::stoi(matches.str(3));
                if (bound > modelLevels_ || bound <= 0) {
                    return false;
                }
                if (matches.str(3).empty()) {  // it is a begin open range
                    for (int i = 1; i <= bound; i++) {
                        if (!levelsCovered.insert(i).second) {
                            return false;
                        }
                    }
                }
                else {
                    for (int i = bound; i <= modelLevels_; i++) {
                        if (!levelsCovered.insert(i).second) {
                            return false;
                        }
                    }
                }
            }
        }
        else {  // the level key should match at least one of the three regular expressions
            return false;
        }
    }
    return true;
}

bool ConfigReader::validateVortexRollupOpts(const eckit::LocalConfiguration& options) {
    if (!options.isFloatingPoint("time_variation")) {
        return false;
    }
    return true;
}

bool ConfigReader::validateRandomOpts(const eckit::LocalConfiguration& options) {
    if (options.empty()) {  // Random has only optional options
        return true;
    }
    if (options.has("distribution") && !options.isString("distribution")) {
        return false;
    }
    auto distribution = options.getString("distribution", "uniform");
    if (distribution == "uniform") {
        if ((options.has("min") && !options.isFloatingPoint("min")) ||
            (options.has("max") && !options.isFloatingPoint("max"))) {
            return false;
        }
        auto min = options.getDouble("min", 0.0);
        auto max = options.getDouble("max", 1.0);
        if (max < min) {  // min should be inferior to max
            return false;
        }
    }
    else if (distribution == "bernoulli") {
        if ((options.has("probability") && !options.isFloatingPoint("probability")) ||
            (options.has("value") && !options.isFloatingPoint("value"))) {
            return false;
        }
        auto probability = options.getDouble("probability", 0.5);
        if (probability < 1e-6 || probability > 1.0) {  // probability must belong to [0,1]
            return false;
        }
    }
    else if (distribution == "normal") {
        if ((options.has("mean") && !options.isFloatingPoint("mean")) ||
            (options.has("stddev") && !options.isFloatingPoint("stddev"))) {
            return false;
        }
        auto stddev = options.getDouble("stddev", 1.0);
        if (stddev < 1e-6) {  // standard deviation must be positive
            return false;
        }
    }
    else {
        eckit::Log::error() << "Distribution '" << distribution << "' is not supported, pick between 'bernoulli', "
                            << "'uniform', or 'normal', exit..." << std::endl;
        return false;
    }
    return true;
}

bool ConfigReader::validateStepOpts(const eckit::LocalConfiguration& options) {
    if (!options.isFloatingPoint("value")) {  // checks both for existence and type
        return false;
    }
    if (options.has("probability") && !options.isFloatingPoint("probability")) {
        return false;
    }
    if (options.has("probability") &&
        (options.getDouble("probability") < 1e-6 || options.getDouble("probability") > 1.0)) {
        return false;  // probability must belong to [0,1]
    }
    if (options.has("variation") && !options.isFloatingPoint("variation")) {
        return false;
    }
    return true;
}

bool ConfigReader::validateCardinalSineOpts(const eckit::LocalConfiguration& options) {
    if (!options.isIntegral("modes") || options.getInt("modes") < 1) {
        return false;
    }
    if (options.has("sink") && !options.isBoolean("sink")) {
        return false;
    }
    if ((options.has("spread") && !options.isFloatingPoint("spread")) ||
        (options.has("min") && !options.isFloatingPoint("min")) ||
        (options.has("max") && !options.isFloatingPoint("max"))) {
        return false;
    }
    auto spread = options.getDouble("spread", 10.0);
    auto min    = options.getDouble("min", 0.0);
    auto max    = options.getDouble("max", 1.0);
    if (max < min || spread < 1e-6) {
        return false;
    }
    return true;
}

bool ConfigReader::validateGaussianOpts(const eckit::LocalConfiguration& options) {
    if ((options.has("max_stddev") && !options.isFloatingPoint("max_stddev"))) {
        return false;
    }
    auto stddev = options.getDouble("max_stddev", 1.0);
    eckit::LocalConfiguration tmpConfig(options);
    tmpConfig.set("spread", stddev);
    return validateCardinalSineOpts(tmpConfig);
}
//----------------------------------------------------------------------------------------------------------------------
}  // namespace nwp_emulator