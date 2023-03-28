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
#include "plume/data/ParameterCatalogue.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"


namespace plume {
namespace data {


ParameterCatalogue::ParameterCatalogue() :
    parameters_{} {
}


ParameterCatalogue::ParameterCatalogue(const eckit::Configuration& config) {

    // fill in parameters
    std::vector<eckit::LocalConfiguration> paramsConfigs = config.getSubConfigurations("params");
    for (const auto& parConf: paramsConfigs) {
        parameters_.push_back(Parameter(parConf));
    }
}


ParameterCatalogue::ParameterCatalogue(const ParameterCatalogue& other) {
    parameters_ = other.parameters_;
}



ParameterCatalogue::~ParameterCatalogue() {}


ParameterCatalogue& ParameterCatalogue::operator=(const ParameterCatalogue& other) {
    parameters_ = other.parameters_;
    return *this;
}


const Parameter& ParameterCatalogue::getParam(const std::string& name) const {
    auto it = std::find_if(parameters_.begin(), parameters_.end(), [&name](const Parameter& obj) {return obj.name() == name;});
        if (it == parameters_.end()) {
        auto err = "Param [" + name + "] not in Catalogue! something went wrong..";
        eckit::Log::error() << err << std::endl;
        throw eckit::BadValue(err, Here());
    } else {
        return *it;
    }
}

// insert a parameter (just a string, for now..)
ParameterCatalogue& ParameterCatalogue::insertParam(const Parameter& param) {
    parameters_.push_back(param);
    return *this;
}


const std::vector<Parameter>& ParameterCatalogue::getParams() const {
    return parameters_;
}

std::vector<std::string> ParameterCatalogue::getParamNames() const {
    std::vector<std::string> names;
    for (const auto& paramEntry: parameters_) {
        names.push_back(paramEntry.name());
    }
    return names;
}


eckit::LocalConfiguration ParameterCatalogue::getConfig() const {

    eckit::LocalConfiguration config;

    std::vector<eckit::LocalConfiguration> params;
    for (const auto& param: parameters_) {
        params.push_back(param.config());
    }
    config.set("params", params);
    
    return config;
}


bool ParameterCatalogue::hasParam(const std::string& name) const {
    auto it = std::find_if(parameters_.begin(), parameters_.end(), [&name](const Parameter& obj) {return obj.name() == name;});
    if (it != parameters_.end()) {
        return true;
    } else {
        return false;
    }
}


ParameterCatalogue ParameterCatalogue::filter(const std::vector<std::string>& params) const {

    ParameterCatalogue filtered;

    // insert only the requested params
    for (const auto& name : params) {
        if (!hasParam(name)) {
            throw eckit::BadValue("Param " + name + " not found!", Here());
        } else {
            filtered.parameters_.push_back(getParam(name));
        }
    }

    return filtered;
}

}  // namespace data
}  // namespace plume