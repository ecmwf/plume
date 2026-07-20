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

#include <iostream>
#include <map>
#include <set>

#include "plume/Negotiator.h"
#include "plume/plume.h"
#include "plume/utils.h"

namespace plume {

Negotiator::Negotiator(WriteBackPolicy policy) : policy_{policy} {}

bool Negotiator::isParamOffered(const Protocol& offers, const data::ParameterDefinition& param) {
    eckit::Log::info() << " - Considering Parameter: " << param.name() << std::endl;
    if (!offers.isParamOffered(param.name())) {
        if (offers.isParamOffered(param.sourceParam())) {
            // it is a derived param whose source is offered
            for (const auto& dependency : param.dependencies()) {
                if (!offers.isParamOffered(dependency)) {
                    eckit::Log::warning() << "Dependency " << dependency << " of parameter " << param.name()
                                          << " not found!" << std::endl;
                    return false;
                }
            }
        }
        else {
            eckit::Log::warning() << "Parameter " << param.name() << " not found or source not found!" << std::endl;
            return false;
        }
    }

    // If the plugin requests write access:
    //   1. The write-back policy must permit it (not disabled).
    //   2. The model must have explicitly offered the parameter as writable.
    // Derived parameters cannot be writable (enforced at ParameterDefinition construction), so param.name()
    // is always the name the model offered directly.
    if (param.writable()) {
        if (policy_ == WriteBackPolicy::disabled) {
            eckit::Log::warning() << "Parameter \"" << param.name()
                                  << "\" requested as writable but write-back is disabled "
                                     "(set \"write-back-policy\" in manager config)."
                                  << std::endl;
            return false;
        }
        if (!offers.offers().getParam(param.name()).writable()) {
            eckit::Log::warning() << "Parameter \"" << param.name()
                                  << "\" requested as writable but not offered as writable!" << std::endl;
            return false;
        }
    }

    return true;
}


bool Negotiator::checkConflicts(const std::string& pluginName, const PluginDecision& decision) {
    for (const auto& param : decision.offeredParams()) {
        const std::string& pname  = param.name();
        const std::string& source = param.sourceParam();  // non-empty only for derived params
        const bool isDerived      = !source.empty();

        if (param.writable()) {
            // Write-Write conflict
            if (writableClaimants_.count(pname) && !isMultiWriter(policy_)) {
                eckit::Log::error()
                    << "Plume negotiation: plugin \"" << pluginName
                    << "\" rejected — write-write conflict: param \"" << pname
                    << "\" already written by \"" << writableClaimants_[pname].front() << "\"." << std::endl;
                return false;
            }

            if (isStrictPolicy(policy_)) {
                // Write-Read conflict: existing readers
                if (readonlyClaimants_.count(pname)) {
                    eckit::Log::error()
                        << "Plume negotiation: plugin \"" << pluginName
                        << "\" rejected under strict policy — write-read conflict: "
                        << "param \"" << pname << "\" is already read (read-only) by one or more plugins." << std::endl;
                    return false;
                }
                // Write-DerivedRead conflict: existing derived readers
                if (derivedClaimants_.count(pname)) {
                    eckit::Log::error()
                        << "Plume negotiation: plugin \"" << pluginName
                        << "\" rejected under strict policy — write-derived-read conflict: "
                        << "a derived version of param \"" << pname
                        << "\" is already consumed by one or more plugins." << std::endl;
                    return false;
                }
            }
        }
        else if (!isDerived) {
            // Read-Write conflict: existing writer
            if (writableClaimants_.count(pname) && isStrictPolicy(policy_)) {
                eckit::Log::error()
                    << "Plume negotiation: plugin \"" << pluginName
                    << "\" rejected under strict policy — read-write conflict: "
                    << "param \"" << pname << "\" is already written by \""
                    << writableClaimants_[pname].front() << "\"." << std::endl;
                return false;
            }
        }
        else {
            // DerivedRead-Write conflict: existing writer on source
            if (writableClaimants_.count(source) && isStrictPolicy(policy_)) {
                eckit::Log::error()
                    << "Plume negotiation: plugin \"" << pluginName
                    << "\" rejected under strict policy — derived-read-write conflict: "
                    << "source param \"" << source << "\" of derived param \"" << pname
                    << "\" is already written by \"" << writableClaimants_[source].front() << "\"." << std::endl;
                return false;
            }
        }
    }
    return true;
}


void Negotiator::recordClaims(const std::string& pluginName, const PluginDecision& decision) {
    executionOrder_.push_back({pluginName, {}});
    auto& entry = executionOrder_.back();

    for (const auto& param : decision.offeredParams()) {
        const std::string& pname  = param.name();
        const std::string& source = param.sourceParam();
        if (param.writable()) {
            writableClaimants_[pname].push_back(pluginName);
        }
        else if (source.empty()) {
            readonlyClaimants_[pname].push_back(pluginName);
        }
        else {
            derivedClaimants_[source].push_back(pluginName);
        }
        entry.params.push_back(param);
    }
}


void Negotiator::logSummary() const {
    if (executionOrder_.empty()) {
        eckit::Log::info() << "Plume negotiation summary: no plugins accepted." << std::endl;
        return;
    }

    eckit::Log::info() << "Plume negotiation summary — accepted plugin execution order:" << std::endl;
    for (size_t i = 0; i < executionOrder_.size(); ++i) {
        const auto& plugin = executionOrder_[i];
        eckit::Log::info() << "  [" << (i + 1) << "] " << plugin.name << std::endl;
        for (const auto& param : plugin.params) {
            std::string mode;
            if (param.writable()) {
                mode = "write-back";
            }
            else if (!param.sourceParam().empty()) {
                mode = "derived  (source: " + param.sourceParam() + ")";
            }
            else {
                mode = "read-only";
            }
            eckit::Log::info() << "       - " << param.name() << "  (" << mode << ")" << std::endl;
        }
    }
}


WriteAuthorisation Negotiator::writeAuthorisation() const {
    WriteAuthorisation auth;
    for (const auto& [paramName, plugins] : writableClaimants_) {
        for (const auto& pluginName : plugins) {
            auth.grant(pluginName, paramName);
        }
    }
    return auth;
}


PluginDecision Negotiator::negotiate(const std::string& pluginName, const Protocol& offers,
                                     const Protocol& requires,
                                     const std::vector<eckit::LocalConfiguration>& config_params) {

    eckit::Log::info() << "Requesting Plume Version: " << LibVersion(requires.requiredPlumeVersion()).asString()
                       << " VS Actual Plume version " << plume_VERSION << std::endl;

    eckit::Log::info() << "Requesting Atlas Version: " << LibVersion(requires.requiredAtlasVersion()).asString()
                       << " VS Actual Atlas version " << offers.offeredAtlasVersion() << std::endl;

    // Check Plume version
    if (LibVersion(requires.requiredPlumeVersion()) > LibVersion(plume_VERSION)) {
        return PluginDecision{false};
    }

    // Check Atlas version
    if (LibVersion(requires.requiredAtlasVersion()) > LibVersion(offers.offeredAtlasVersion())) {
        return PluginDecision{false};
    }

    // Accumulate requested params in a map keyed by name.
    // "Writable wins": if the same param is requested read-only from code and writable from a
    // config group (or vice versa), the writable version takes precedence so that write-back
    // intent is never silently lost.  Using std::set<ParameterDefinition> directly would drop
    // the second insert because operator< compares by name only, causing the writable flag to be
    // determined solely by whichever version was inserted first.
    std::map<std::string, data::ParameterDefinition> paramMap;
    auto insertOrUpgrade = [&paramMap](const data::ParameterDefinition& p) {
        auto it = paramMap.find(p.name());
        if (it == paramMap.end()) {
            paramMap.emplace(p.name(), p);
        }
        else if (p.writable() && !it->second.writable()) {
            it->second = p;  // upgrade to writable
        }
    };

    std::set<std::string> requested_param_names = requires.requiredParamNames();
    std::vector<data::ParameterDefinition> requested_params = requires.requires().getParams();

    // 1) Check requested parameters (from the plugin)
    // if ANY one of these parameters is not offered, the plugin is rejected
    eckit::Log::info() << "Requesting Parameters: [";
    for (auto it = requested_param_names.begin(); it != requested_param_names.end(); ++it) {
        if (it != requested_param_names.begin())
            eckit::Log::info() << ", ";
        eckit::Log::info() << *it;
    }
    eckit::Log::info() << "]" << std::endl;

    for (const auto& param : requested_params) {
        if (!isParamOffered(offers, param)) {
            return PluginDecision{false};
        }
    }

    if (!requested_params.empty()) {
        for (const auto& p : requested_params) { insertOrUpgrade(p); }
    }

    // 2) Check requested parameters (from the configuration)
    // Here we check "groups" of requested parameters. A plugin can run if AT LEAST ONE of the "groups" is satisfied
    if (!config_params.empty()) {

        bool any_group_accepted = false;

        for (const auto& param_group : config_params) {

            eckit::Log::info() << "Considering Parameter Group..." << std::endl;

            const std::vector<eckit::LocalConfiguration>& params = param_group.getSubConfigurations();
            requested_params.clear();

            // loop over parameters in the group and check if they are all offered
            bool group_satisfied =
                std::all_of(params.begin(), params.end(), [this, &offers, &requested_params](const auto& paramConfig) {
                    data::ParameterDefinition param(paramConfig);
                    if (!isParamOffered(offers, param)) {
                        return false;
                    }
                    requested_params.push_back(param);
                    return true;
                });

            if (group_satisfied) {
                eckit::Log::info() << " ---> Parameter Group Accepted!" << std::endl;
                any_group_accepted = true;

                // add all parameters in the group to paramMap with writable-wins semantics
                for (const auto& p : requested_params) { insertOrUpgrade(p); }
            }
            else {
                eckit::Log::warning() << "---> Group rejected!" << std::endl;
            }
        }

        if (!any_group_accepted) {
            return PluginDecision{false};
        }
    }

    // Convert paramMap to the set expected by PluginDecision (unique by name, writable-wins applied).
    std::set<data::ParameterDefinition> allRequestedParams;
    for (const auto& [name, param] : paramMap) { allRequestedParams.insert(param); }

    // 3) Cross-plugin conflict detection (stateful: checks against previously accepted plugins)
    PluginDecision decision(true, allRequestedParams);
    if (!checkConflicts(pluginName, decision)) {
        return PluginDecision{false};
    }

    recordClaims(pluginName, decision);
    return decision;
}


}  // namespace plume
