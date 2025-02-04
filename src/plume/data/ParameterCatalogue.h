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

#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/Parameter.h"

namespace plume {
namespace data {


/**
 * @brief A catalogue of Parameters
 * 
 */
class ParameterCatalogue {
    ///
    /// {
    ///   "params": [
    ///     {
    ///       "name": "param-1",
    ///       "type": "INT",
    ///       "available": "always",
    ///       "comment": "Param 1"
    ///     },
    ///     {
    ///       "name": "param-2",
    ///       "type": "FLOAT",
    ///       "available": "on-request",
    ///       "comment": "Param 2"
    ///     }
    ///   ]
    /// }
    ///

public:

    ParameterCatalogue();

    ParameterCatalogue(const eckit::Configuration& config);

    ParameterCatalogue(const ParameterCatalogue& other);

    ~ParameterCatalogue();    

    ParameterCatalogue& operator=(const ParameterCatalogue& other);

    const Parameter& getParam(const std::string& key) const;

    ParameterCatalogue& insertParam(const Parameter& param);

    const std::vector<Parameter>& getParams() const;

    std::vector<std::string> getParamNames() const;

    eckit::LocalConfiguration getConfig() const;

    bool hasParam(const std::string& name) const;

    ParameterCatalogue filter(const std::unordered_set<std::string>& params) const;

private:

    // internal keys
    constexpr static const char* paramsKey() {return "params";}

private:

    std::vector<Parameter> parameters_;
};


}  // namespace data
}  // namespace plume