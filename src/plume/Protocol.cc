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

#include "atlas/library/version.h"

#include "plume/plume_version.h"
#include "plume/Protocol.h"

namespace plume {


Protocol::Protocol() :
    requestedPlumeVersion_{"0.0.0"},
    requestedAtlasVersion_{"0.0.0"},
    offeredPlumeVersion_{plume_VERSION},
    offeredAtlasVersion_{atlas::library::version()} {}


Protocol::Protocol(const eckit::Configuration& config) {
    setParamsFromConfig(config);
    requestedPlumeVersion_ = config.getString("requestedPlumeVersion", "0.0.0");
    requestedAtlasVersion_ = config.getString("requestedAtlasVersion", "0.0.0");
    offeredPlumeVersion_   = config.getString("offeredPlumeVersion", plume_VERSION);
    offeredAtlasVersion_   = config.getString("offeredAtlasVersion", atlas::library::version());
}


Protocol::~Protocol() {}


// ------- required
void Protocol::requirePlumeVersion(const std::string& version) {
    requestedPlumeVersion_ = version;
}

void Protocol::requireAtlasVersion(const std::string& version) {
    requestedAtlasVersion_ = version;
}

std::set<std::string> Protocol::requiredParamNames() const {
    return requiredParams_.getParamNames();
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

std::set<std::string> Protocol::offeredParamNames() const {
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


void Protocol::insertParam(const data::ParameterDefinition& param, data::ParameterCatalogue& catalogue) {
    if (!catalogue.hasParam(param.name())) {
        catalogue.insertParam(param);
    }
    else {
        // eckit::Log::warning() << "Parameter " << param.name() << " already requested!" << std::endl;
    }
}

void Protocol::setParamsFromConfig(const eckit::Configuration& config) {

    if (config.has("required")) {
        std::vector<eckit::LocalConfiguration> reqParams = config.getSubConfigurations("required");
        for (const auto& p : reqParams) {
            requiredParams_.insertParam(data::ParameterDefinition(p));
        }
    }
    if (config.has("offered")) {
        std::vector<eckit::LocalConfiguration> offeredParams = config.getSubConfigurations("offered");
        for (const auto& p : offeredParams) {
            offeredParams_.insertParam(data::ParameterDefinition(p));
        }
    }

    // if it's neither requesting nor offering, then throw an error
    if (!config.has("required") && !config.has("offered")) {
        throw eckit::BadParameter("Protocol configuration must have either 'required' or 'offered' keys", Here());
    }
}


}  // namespace plume