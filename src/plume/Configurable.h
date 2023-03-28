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
#include <map>
#include <string>
#include <vector>
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

    virtual ~Configurable();

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

    CheckedConfigurable(const eckit::Configuration& config, const std::vector<std::string> essentialKeys, const eckit::Configuration& options);
    virtual ~CheckedConfigurable() ;

    static bool isValid(const eckit::Configuration& config, const std::vector<std::string> essentialKeys, const eckit::Configuration& options);
};

} // namespace plume