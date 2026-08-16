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
    offeredAtlasVersion_{atlas::library::version()} {

    // The default hook point is always offered, so that a model that knows nothing about hook
    // points still satisfies the plugins that know nothing about them either.
    offerHook(DEFAULT_HOOK, "implicit default hook point");
}


Protocol::Protocol(const eckit::Configuration& config) {
    offerHook(DEFAULT_HOOK, "implicit default hook point");
    setParamsFromConfig(config);
    setHooksFromConfig(config);
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

void Protocol::requireHook(const std::string& name) {
    requiredHooks_.insert(name);
}

std::set<std::string> Protocol::requiredParamNames() const {
    return requiredParams_.getParamNames();
}

const std::set<std::string>& Protocol::requiredHooks() const {
    return requiredHooks_;
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

bool Protocol::isHookRequired(const std::string& name) const {
    return requiredHooks_.find(name) != requiredHooks_.end();
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

void Protocol::offerHook(const std::string& name, const std::string& comment) {
    // idempotent: re-offering an existing hook point only updates its comment
    offeredHooks_[name] = comment;
}

std::set<std::string> Protocol::offeredParamNames() const {
    return offeredParams_.getParamNames();
}

std::set<std::string> Protocol::offeredHookNames() const {
    std::set<std::string> names;
    for (const auto& hook : offeredHooks_) {
        names.insert(hook.first);
    }
    return names;
}

const std::map<std::string, std::string>& Protocol::offeredHooks() const {
    return offeredHooks_;
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

bool Protocol::isHookOffered(const std::string& name) const {
    return offeredHooks_.find(name) != offeredHooks_.end();
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


void Protocol::setHooksFromConfig(const eckit::Configuration& config) {

    if (config.has("requiredHooks")) {
        for (const auto& hook : config.getStringVector("requiredHooks")) {
            requireHook(hook);
        }
    }

    if (config.has("offeredHooks")) {
        for (const auto& hook : config.getSubConfigurations("offeredHooks")) {
            if (!hook.has("name")) {
                throw eckit::BadParameter("Protocol configuration: each offeredHooks entry needs a 'name' key",
                                          Here());
            }
            offerHook(hook.getString("name"), hook.getString("comment", ""));
        }
    }
}


}  // namespace plume