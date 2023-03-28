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


Configurable::~Configurable() {
}


const eckit::LocalConfiguration& Configurable::config() const {
    return config_;
}


std::ostream& operator<<(std::ostream& oss, const Configurable& obj) {
    oss << obj.config();
    return oss;
}
// -------------------------------------------------------------------------------------------



CheckedConfigurable::CheckedConfigurable(const eckit::Configuration& config, const std::vector<std::string> essentialKeys, const eckit::Configuration& options) : Configurable{config} {
    if (!isValid(config, essentialKeys, options)) {
        throw eckit::BadValue("Data type configuration not valid!");
    }
}

CheckedConfigurable::~CheckedConfigurable() {

}

bool CheckedConfigurable::isValid(const eckit::Configuration& config, const std::vector<std::string> essentialKeys, const eckit::Configuration& options) {

    // 1) check that the configuration contains all the essential keys
    for (const auto& key: essentialKeys) {
        if (!config.has(key)) {
            return false;
        }
    }

    // 2) check that all the keys in configuration are valid
    for (const auto& key: config.keys()) {

        // Check if key is listed in the options
        if (!options.has(key)) {
            return false;
        }

        // Check if the value is within acceptable options
        std::string value = config.getString(key);
        std::vector<std::string> keyOptions = options.getStringVector(key);

        // an empty list of options, means all values allowed!
        if (keyOptions.size() > 0) {
            if (find(keyOptions.begin(), keyOptions.end(), value) == keyOptions.end()) {
                return false;
            }
        }
    }
    return true;
}



} // namespace plume
