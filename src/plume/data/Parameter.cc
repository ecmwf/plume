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
#include "plume/data/Parameter.h"
#include "eckit/config/LocalConfiguration.h"

namespace plume {
namespace data {


namespace {

eckit::LocalConfiguration makeParameterOptions() {
    eckit::LocalConfiguration options;
    options.set("name", std::vector<std::string>{} ); // empty means all accepted
    options.set("type", std::vector<std::string>{ ParameterTypeConverter::toStringVector() } );
    options.set("available", std::vector<std::string>{"", "always", "on-request"} );
    options.set("comment", std::vector<std::string>{} );
    return options;
}

eckit::LocalConfiguration parameterOptions() {
    static eckit::LocalConfiguration options = makeParameterOptions();
    return options;
}

}
 

Parameter::Parameter(const eckit::Configuration& config) : CheckedConfigurable(config, std::vector<std::string>{"name", "type"}, parameterOptions() ) {

    // setup from config
    name_ = config.getString("name");
    dataType_ = ParameterTypeConverter::fromString(config.getString("type"));
    available_ = config.getString("available", "");
    comment_ = config.getString("comment", "");
}

// construct from params
Parameter::Parameter(
    const std::string& name,  
    const std::string& type, 
    const std::string& available, 
    const std::string& comment) : Parameter{params2config(name, type, available, comment)}{
}

Parameter::Parameter(
    const std::string& name, 
    const ParameterType& type, 
    const std::string& available, 
    const std::string& comment) : Parameter{params2config(name, ParameterTypeConverter::toString(type), available, comment)} {}


Parameter::~Parameter() {

}


const std::string& Parameter::name() const {
    return name_;
}


const plume::data::ParameterType& Parameter::type() const {
    return dataType_;
}


const std::string& Parameter::available() const {
    return available_;
}


const std::string& Parameter::comment() const {
    return comment_;
}

// helper constructing function
eckit::LocalConfiguration Parameter::params2config(const std::string& name,  const std::string& type, const std::string& available, const std::string& comment) {
    eckit::LocalConfiguration config;
    config.set("name", name);
    config.set("type", type);
    config.set("available", available);
    config.set("comment", comment);
    return config;
}

}  // namespace data
}  // namespace plume