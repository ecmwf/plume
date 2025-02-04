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
#pragma once

#include <map>
#include <string>
#include <unordered_set>

#include "eckit/config/LocalConfiguration.h"


namespace plume {

/**
 * @brief A flat list of {key: value}
 * that can be validated against value options
 * 
 */
class Configurable {

public:

    Configurable(const eckit::Configuration& config);

    virtual ~Configurable() = default;

    const eckit::LocalConfiguration& config() const;

    friend std::ostream& operator<<(std::ostream& oss, const Configurable& obj);

private:

    // internal copy of config
    eckit::LocalConfiguration config_;

};


/**
 * @brief Check config against options
 * 
 */
class CheckedConfigurable : public Configurable {

public:

    CheckedConfigurable(const eckit::Configuration& config,
                        const std::unordered_set<std::string>& essentialKeys,
                        const std::unordered_set<std::string>& optionalKeys = std::unordered_set<std::string>{});

    static bool isValid(const eckit::Configuration& config,
                        const std::unordered_set<std::string>& essentialKeys,
                        const std::unordered_set<std::string>& optionalKeys = std::unordered_set<std::string>{});

    bool has(const std::string& key) const {
        return config().has(key);
    }

private:

    // function that checks for essential keys
    static bool hasEssentialKeys(const eckit::Configuration& config,
                                 const std::unordered_set<std::string>& essentialKeys);

    // function that checks for optional keys
    static bool hasAllValidKeys(const eckit::Configuration& config,
                                const std::unordered_set<std::string>& essentialKeys,
                                const std::unordered_set<std::string>& optionalKeys);

};

} // namespace plume