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
#include "plume/Protocol.h"



namespace plume {


Protocol::Protocol()  : requestedPlumeVersion_{"0.0.0"}, 
                        requestedAtlasVersion_{"0.0.0"},
                        offeredPlumeVersion_{"0.0.0"}, 
                        offeredAtlasVersion_{"0.0.0"} {
}


Protocol::Protocol(const eckit::Configuration& config) {
    setParamsFromConfig(config);
    requestedPlumeVersion_ = config.getString("requestedPlumeVersion", "0.0.0");
    requestedAtlasVersion_ = config.getString("requestedAtlasVersion", "0.0.0");
    offeredPlumeVersion_ = config.getString("offeredPlumeVersion", "0.0.0");
    offeredAtlasVersion_ = config.getString("offeredAtlasVersion", "0.0.0");
}


Protocol::~Protocol() {}



// ------- required
void Protocol::requireInt(const std::string& name) {
    insertParam(data::Parameter(name, data::ParameterType::INT), requiredParams_);
}

void Protocol::requireBool(const std::string& name) {
    insertParam(data::Parameter(name, data::ParameterType::BOOL), requiredParams_);
}

void Protocol::requireFloat(const std::string& name) {
    insertParam(data::Parameter(name, data::ParameterType::FLOAT), requiredParams_);
}

void Protocol::requireDouble(const std::string& name) {
    insertParam(data::Parameter(name, data::ParameterType::DOUBLE), requiredParams_);
}

void Protocol::requireAtlasField(const std::string& name) {
    insertParam(data::Parameter(name, data::ParameterType::ATLAS_FIELD), requiredParams_);
}

void Protocol::requirePlumeVersion(const std::string& version) {
    requestedPlumeVersion_ = version;
}

void Protocol::requireAtlasVersion(const std::string& version) {
    requestedAtlasVersion_ = version;
}

std::vector<std::string> Protocol::requiredParamNames() const {
    return requiredParams_.getParamNames();;
}

const std::string& Protocol::requiredPlumeVersion() const {
    return requestedPlumeVersion_;
}

const std::string& Protocol::requiredAtlasVersion() const {
    return requestedAtlasVersion_;
}

bool Protocol::isParamRequired(const std::string& name) const {
    return requiredParams_.hasParam(name);
}

const data::ParameterCatalogue& Protocol::requires() const {
    return requiredParams_;
}



// ------- offered
void Protocol::offerPlumeVersion(const std::string& version) {
    offeredPlumeVersion_ = version;
}
void Protocol::offerAtlasVersion(const std::string& version) {
    offeredAtlasVersion_ = version;
}
void Protocol::offerInt(const std::string& name, const std::string& avail, const std::string& comment) {
    insertParam(data::Parameter(name, data::ParameterType::INT, avail, comment), offeredParams_);
}
void Protocol::offerBool(const std::string& name, const std::string& avail, const std::string& comment) {
    insertParam(data::Parameter(name, data::ParameterType::BOOL, avail, comment), offeredParams_);
}
void Protocol::offerFloat(const std::string& name, const std::string& avail, const std::string& comment) {
    insertParam(data::Parameter(name, data::ParameterType::FLOAT, avail, comment), offeredParams_);
}
void Protocol::offerDouble(const std::string& name, const std::string& avail, const std::string& comment) {
    insertParam(data::Parameter(name, data::ParameterType::DOUBLE, avail, comment), offeredParams_);    
}
void Protocol::offerAtlasField(const std::string& name, const std::string& avail, const std::string& comment) {
    insertParam(data::Parameter(name, data::ParameterType::ATLAS_FIELD, avail, comment), offeredParams_);
}

std::vector<std::string> Protocol::offeredParamNames() const {
    return offeredParams_.getParamNames();
}
const std::string& Protocol::offeredPlumeVersion() const {
    return offeredPlumeVersion_;
}
const std::string& Protocol::offeredAtlasVersion() const {
    return offeredAtlasVersion_;
}


bool Protocol::isParamOffered(const std::string& name) const {
    return offeredParams_.hasParam(name);
}

const data::ParameterCatalogue& Protocol::offers() const {
    return offeredParams_;
}
// -------------------------------------------------------------------------------------------------



void Protocol::insertParam(const data::Parameter& param, data::ParameterCatalogue& catalogue) {
    if ( ! catalogue.hasParam(param.name())){
            catalogue.insertParam(param);
    } else {
        // eckit::Log::warning() << "Parameter " << param.name() << " already requested!" << std::endl;
    }
}

void Protocol::setParamsFromConfig(const eckit::Configuration& config) {

    if (config.has("required")){
        std::vector<eckit::LocalConfiguration> reqParams = config.getSubConfigurations("required");
        for(const auto& p: reqParams) {
            requiredParams_.insertParam(p);
        }
    }
    if (config.has("offered")){
        std::vector<eckit::LocalConfiguration> offeredParams = config.getSubConfigurations("offered");
        for(const auto& p: offeredParams) {
            offeredParams_.insertParam(p);
        }
    }
}


}  // namespace plume