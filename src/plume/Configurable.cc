/*
 * (C) Copyright 2023- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <algorithm>
#include "eckit/log/Log.h"
#include "eckit/exception/Exceptions.h"

#include "plume/Configurable.h"


namespace plume {


// Setup from config
Configurable::Configurable(const eckit::Configuration& config) : config_{config} {
}


const eckit::LocalConfiguration& Configurable::config() const {
    return config_;
}


std::ostream& operator<<(std::ostream& oss, const Configurable& obj) {
    oss << obj.config();
    return oss;
}
// -------------------------------------------------------------------------------------------



CheckedConfigurable::CheckedConfigurable(const eckit::Configuration& config,
                                         const std::unordered_set<std::string>& essentialKeys,
                                         const std::unordered_set<std::string>& optionalKeys) : Configurable{config} {

    eckit::Log::debug() << "checking configuration: " << config << std::endl;
    if (!isValid(config, essentialKeys, optionalKeys)) {
        throw eckit::BadValue("Data type configuration not valid!");
    }
}


bool CheckedConfigurable::isValid(const eckit::Configuration& config,
                                  const std::unordered_set<std::string>& essentialKeys,
                                  const std::unordered_set<std::string>& optionalKeys) {

    // 1) check that the configuration contains all the essential keys
    // 2) check that the configuration contains only valid keys (essential and/or optional)
    return hasEssentialKeys(config, essentialKeys) && hasAllValidKeys(config, essentialKeys, optionalKeys);
}


// helper function to check for optional keys
bool CheckedConfigurable::hasEssentialKeys(const eckit::Configuration& config,
                                           const std::unordered_set<std::string>& essentialKeys) {
    bool allKeys = true;
    for (const auto& key: essentialKeys) {
        if (!config.has(key)) {
            eckit::Log::error() << "Missing essential key: " << key << std::endl;
            allKeys = false;
        }
    }
    return allKeys;
}


// helper function to check for optional keys
bool CheckedConfigurable::hasAllValidKeys(const eckit::Configuration& config,
                                          const std::unordered_set<std::string>& essentialKeys,
                                          const std::unordered_set<std::string>& optionalKeys) {
    
    for (const auto& key: config.keys()) {
        if (essentialKeys.find(key) == essentialKeys.end() && optionalKeys.find(key) == optionalKeys.end()) {
            eckit::Log::error() << "Invalid key: " << key << std::endl;
            return false;
        }
    }
    return true;
}




} // namespace plume
