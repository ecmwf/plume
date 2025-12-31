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

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterValue.h"


namespace plume {
namespace data {

ParameterDefinition::ParameterDefinition(const eckit::Configuration& config) :
    CheckedConfigurable(config, essentialKeys_, optionalKeys_) {

    // setup from config
    name_      = config.getString("name");
    dataType_  = typeFromString(config.getString("type").c_str());
    available_ = config.getString("available", "on-request");
    comment_   = config.getString("comment", "");

    // determine strategy, dependencies & derived param name based on the config, if applicable
    bool hasOptions = false;
    for (const auto& option : field_provider::UpdateStrategyTraits<int>::allConfigArgs) {
        if (config.has(std::string(option))) {
            hasOptions = true;
            break;  // no need to continue, found a key that is an option to request a derived param
        }
    }

    if (hasOptions) {
        auto [strategy, dependencies, levtype, levelKey] = field_provider::findMatchingStrategy(name_, config);

        if (strategy.empty()) {
            throw eckit::BadParameter("No strategy matches the options passed for param '" + name_ + "'!", Here());
        }

        sourceParam_  = name_;
        strategy_     = strategy;
        dependencies_ = dependencies;
        levtype_      = levtype;
        config.get(levelKey, level_);

        config_.set("levtype", levtype_);
        config_.set("level", level_);

        // Do not change the name in the underlying config as it is going to be used for creating the strategy
        name_ = IParameterObserver::deriveParamName(sourceParam_, levtype_, level_);
    }
}

// construct from params
ParameterDefinition::ParameterDefinition(const std::string& name, const std::string& type, const std::string& available,
                                         const std::string& comment) :
    ParameterDefinition{params2config(name, type, available, comment)} {}

ParameterDefinition::ParameterDefinition(const std::string& name, const ParameterType& type,
                                         const std::string& available, const std::string& comment) :
    ParameterDefinition{params2config(name, typeToString(type), available, comment)} {}

ParameterDefinition::ParameterDefinition(const std::string& name, const ParameterType& type,
                                         const std::unordered_map<std::string, std::string>& options) :
    ParameterDefinition([this, &name, &type, &options]() {
        eckit::LocalConfiguration config = params2config(name, typeToString(type));
        for (const auto& option : options) {
            config.set(option.first, option.second);
        }
        return config;
    }()) {}

const std::string& ParameterDefinition::name() const {
    return name_;
}

const ParameterType& ParameterDefinition::type() const {
    return dataType_;
}

const std::string& ParameterDefinition::available() const {
    return available_;
}

const std::string& ParameterDefinition::comment() const {
    return comment_;
}

const std::string& ParameterDefinition::levtype() const {
    return levtype_;
}

const std::string& ParameterDefinition::level() const {
    return level_;
}

const std::string& ParameterDefinition::strategy() const {
    return strategy_;
}

const std::string& ParameterDefinition::sourceParam() const {
    return sourceParam_;
}

const std::vector<std::string>& ParameterDefinition::dependencies() const {
    return dependencies_;
}

// helper constructing function
eckit::LocalConfiguration ParameterDefinition::params2config(const std::string& name, const std::string& type,
                                                             const std::string& available, const std::string& comment) {
    eckit::LocalConfiguration config;
    config.set("name", name);
    config.set("type", type);
    config.set("available", available);
    config.set("comment", comment);
    return config;
}

// =====================================================================================================================
// Parameter Catalogue
// =====================================================================================================================
ParameterCatalogue::ParameterCatalogue() : parameters_{} {}

ParameterCatalogue::ParameterCatalogue(const eckit::Configuration& config) {

    // fill in parameters
    std::vector<eckit::LocalConfiguration> paramsConfigs = config.getSubConfigurations("params");
    for (const auto& parConf : paramsConfigs) {
        parameters_.push_back(ParameterDefinition(parConf));
    }
}

ParameterCatalogue::ParameterCatalogue(const ParameterCatalogue& other) {
    parameters_ = other.parameters_;
}

ParameterCatalogue& ParameterCatalogue::operator=(const ParameterCatalogue& other) {
    parameters_ = other.parameters_;
    return *this;
}

const ParameterDefinition& ParameterCatalogue::getParam(const std::string& name) const {
    auto it = std::find_if(parameters_.begin(), parameters_.end(),
                           [&name](const ParameterDefinition& obj) { return obj.name() == name; });
    if (it == parameters_.end()) {
        auto err = "Param [" + name + "] not in Catalogue! something went wrong..";
        eckit::Log::error() << err << std::endl;
        throw eckit::BadValue(err, Here());
    }
    else {
        return *it;
    }
}

// insert a parameter (just a string, for now..)
ParameterCatalogue& ParameterCatalogue::insertParam(const ParameterDefinition& param) {
    parameters_.push_back(param);
    return *this;
}

const std::vector<ParameterDefinition>& ParameterCatalogue::getParams() const {
    return parameters_;
}

std::set<std::string> ParameterCatalogue::getParamNames() const {
    std::set<std::string> names;
    for (const auto& paramEntry : parameters_) {
        names.insert(paramEntry.name());
    }
    return names;
}

eckit::LocalConfiguration ParameterCatalogue::getConfig() const {

    eckit::LocalConfiguration config;

    std::vector<eckit::LocalConfiguration> params;
    for (const auto& param : parameters_) {
        params.push_back(param.config());
    }
    config.set("params", params);

    return config;
}

bool ParameterCatalogue::hasParam(const std::string& name) const {
    auto it = std::find_if(parameters_.begin(), parameters_.end(),
                           [&name](const ParameterDefinition& obj) { return obj.name() == name; });
    if (it != parameters_.end()) {
        return true;
    }
    else {
        return false;
    }
}

ParameterCatalogue ParameterCatalogue::filter(const std::unordered_set<std::string>& params) const {

    ParameterCatalogue filtered;

    // insert only the requested params
    for (const auto& name : params) {
        if (!hasParam(name)) {
            throw eckit::BadValue("Param " + name + " not found!", Here());
        }
        else {
            filtered.parameters_.push_back(getParam(name));
        }
    }

    return filtered;
}

}  // namespace data
}  // namespace plume