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

    // optional hook scope: when present it must be a non-empty list of hook point names (an empty list is not
    // the same as the key being absent, and conflating the two would hide a configuration mistake)
    if (config.has("hooks")) {
        if (!config.isStringList("hooks") || config.getStringVector("hooks").empty()) {
            throw eckit::BadValue("Param '" + name_ + "': hooks must be a non-empty list of hook point names", Here());
        }
        auto hookNames = config.getStringVector("hooks");
        hooks_.insert(hookNames.begin(), hookNames.end());
    }

    // optional access mode: a boolean (writable at every hook point of the scope), or a non-empty list of the hook
    // points the param is writable at (read-only at the other hook points of its scope)
    if (config.has("writable")) {
        if (config.isBoolean("writable")) {
            writable_ = config.getBool("writable");
        }
        else if (config.isStringList("writable") && !config.getStringVector("writable").empty()) {
            auto hookNames = config.getStringVector("writable");
            writableHooks_.insert(hookNames.begin(), hookNames.end());
            writable_ = true;
            for (const auto& hook : writableHooks_) {
                if (!hooks_.empty() && hooks_.count(hook) == 0) {
                    throw eckit::BadValue("Param '" + name_ + "': writable at hook point '" + hook +
                                              "', which is not in its hook scope",
                                          Here());
                }
            }
        }
        else {
            throw eckit::BadValue("Param '" + name_ +
                                      "': writable must be a boolean or a non-empty list of hook point names",
                                  Here());
        }
    }

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

        if (writable_) {
            throw eckit::BadParameter(
                "Derived parameter '" + name_ + "' cannot be declared writable. "
                "Only base (non-derived) parameters support write-back.",
                Here());
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
                                         const std::string& comment, bool writable, const std::set<std::string>& hooks,
                                         const std::set<std::string>& writableHooks) :
    ParameterDefinition{params2config(name, type, available, comment, writable, hooks, writableHooks)} {}

ParameterDefinition::ParameterDefinition(const std::string& name, const ParameterType& type,
                                         const std::string& available, const std::string& comment, bool writable,
                                         const std::set<std::string>& hooks,
                                         const std::set<std::string>& writableHooks) :
    ParameterDefinition{params2config(name, typeToString(type), available, comment, writable, hooks, writableHooks)} {}

ParameterDefinition::ParameterDefinition(const std::string& name, const ParameterType& type,
                                         const std::unordered_map<std::string, std::string>& options,
                                         const std::set<std::string>& hooks) :
    ParameterDefinition([this, &name, &type, &options, &hooks]() {
        eckit::LocalConfiguration config = params2config(name, typeToString(type), "", "", false, hooks);
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

bool ParameterDefinition::writable() const {
    return writable_;
}

const std::set<std::string>& ParameterDefinition::hooks() const {
    return hooks_;
}

const std::set<std::string>& ParameterDefinition::writableHooks() const {
    return writableHooks_;
}

bool ParameterDefinition::isWritableAt(const std::string& hook) const {
    return writable_ && (writableHooks_.empty() || writableHooks_.count(hook) > 0);
}

void ParameterDefinition::pinWritableHooks() {
    if (writable_ && writableHooks_.empty() && !hooks_.empty()) {
        writableHooks_ = hooks_;
    }
}

namespace {

// writable at every hook point, whatever the scope ends up being
bool writableEverywhere(const ParameterDefinition& param) {
    return param.writable() && param.writableHooks().empty() && param.hooks().empty();
}

// the definition expresses per-hook-point intent: a hook scope or an explicit list of writable hook points
bool hasPerHookIntent(const ParameterDefinition& param) {
    return !param.hooks().empty() || !param.writableHooks().empty();
}

// read-only at (at least) one hook point of its scope (to be called on a pinned definition)
bool hasReadOnlyHooks(const ParameterDefinition& param) {
    if (!param.writable()) {
        return true;
    }
    if (param.writableHooks().empty()) {
        return false;
    }
    return param.hooks().empty() || param.writableHooks() != param.hooks();
}

}  // namespace

bool ParameterDefinition::mergeAccess(const ParameterDefinition& other) {

    ParameterDefinition rhs(other);

    // 1. normalise: pin the writes of scoped definitions to their scope before the scopes are merged
    pinWritableHooks();
    rhs.pinWritableHooks();

    const bool lhsEverywhere = writableEverywhere(*this);
    const bool rhsEverywhere = writableEverywhere(rhs);
    // only worth a warning when the overridden definition asked for read-only access at specific hook points: two
    // unscoped definitions (one read-only, one writable) are the plain "writable wins" case
    const bool widened = (lhsEverywhere && hasPerHookIntent(rhs) && hasReadOnlyHooks(rhs)) ||
                         (rhsEverywhere && hasPerHookIntent(*this) && hasReadOnlyHooks(*this));

    // 2. scope: unscoped wins, otherwise union
    if (hooks_.empty() || rhs.hooks_.empty()) {
        hooks_.clear();
    }
    else {
        hooks_.insert(rhs.hooks_.begin(), rhs.hooks_.end());
    }

    // 3. access: writable wins, per hook point
    if (lhsEverywhere || rhsEverywhere) {
        writable_ = true;
        writableHooks_.clear();
    }
    else {
        std::set<std::string> writes = writable_ ? writableHooks_ : std::set<std::string>{};
        if (rhs.writable_) {
            writes.insert(rhs.writableHooks_.begin(), rhs.writableHooks_.end());
        }
        writable_      = !writes.empty();
        writableHooks_ = writes;
    }

    if (writable_ && (!strategy_.empty() || !rhs.strategy_.empty())) {
        throw eckit::BadParameter("Derived parameter '" + name_ + "' cannot be declared writable. "
                                  "Only base (non-derived) parameters support write-back.",
                                  Here());
    }

    // 4. normalise back: writable at the whole (scoped) scope is written as "writable: true"
    if (writable_ && !hooks_.empty() && writableHooks_ == hooks_) {
        writableHooks_.clear();
    }

    // 5. keep the config in sync
    syncAccessConfig();

    return widened;
}

void ParameterDefinition::syncAccessConfig() {
    if (hooks_.empty()) {
        if (config_.has("hooks")) {
            config_.remove("hooks");
        }
    }
    else {
        config_.set("hooks", std::vector<std::string>(hooks_.begin(), hooks_.end()));
    }

    if (writable_ && !writableHooks_.empty()) {
        config_.set("writable", std::vector<std::string>(writableHooks_.begin(), writableHooks_.end()));
    }
    else {
        config_.set("writable", writable_);
    }
}

// helper constructing function
eckit::LocalConfiguration ParameterDefinition::params2config(const std::string& name, const std::string& type,
                                                             const std::string& available, const std::string& comment,
                                                             bool writable, const std::set<std::string>& hooks,
                                                             const std::set<std::string>& writableHooks) {
    eckit::LocalConfiguration config;
    config.set("name", name);
    config.set("type", type);
    config.set("available", available);
    config.set("comment", comment);
    // an empty set means unscoped: leave the key out rather than writing an (invalid) empty list
    if (!hooks.empty()) {
        config.set("hooks", std::vector<std::string>(hooks.begin(), hooks.end()));
    }
    // writable at some hook points only: list them, otherwise a plain boolean
    if (writable && !writableHooks.empty()) {
        config.set("writable", std::vector<std::string>(writableHooks.begin(), writableHooks.end()));
    }
    else {
        config.set("writable", writable);
    }
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

bool ParameterCatalogue::mergeParam(const ParameterDefinition& param) {
    const auto& name = param.name();
    auto it          = std::find_if(parameters_.begin(), parameters_.end(),
                                    [&name](const ParameterDefinition& obj) { return obj.name() == name; });
    if (it == parameters_.end()) {
        throw eckit::BadValue("Param [" + name + "] not in Catalogue, cannot merge its definition", Here());
    }
    return it->mergeAccess(param);
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