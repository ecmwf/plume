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
#include <iostream>
#include <iterator>
#include <map>
#include <set>

#include "eckit/exception/Exceptions.h"

#include "plume/Negotiator.h"
#include "plume/plume.h"
#include "plume/utils.h"

namespace plume {

namespace {

std::string hooksToString(const std::set<std::string>& hooks) {
    std::string str;
    for (const auto& hook : hooks) {
        str += (str.empty() ? "" : ", ") + hook;
    }
    return "[" + str + "]";
}

// hook points in display order: by name, with the default hook point last
std::vector<std::string> displayOrder(const std::set<std::string>& hooks) {
    std::vector<std::string> ordered;
    for (const auto& hook : hooks) {
        if (hook != DEFAULT_HOOK) {
            ordered.push_back(hook);
        }
    }
    if (hooks.count(DEFAULT_HOOK) > 0) {
        ordered.push_back(DEFAULT_HOOK);
    }
    return ordered;
}

}  // namespace


Negotiator::Negotiator(WriteBackPolicy policy) : policy_{policy} {}


std::set<std::string> Negotiator::effectiveHooks(const data::ParameterDefinition& param,
                                                 const std::set<std::string>& agreedHooks) {
    return param.hooks().empty() ? agreedHooks : param.hooks();
}


bool Negotiator::isOfferedAtHooks(const Protocol& offers, const std::string& offered, const std::string& required,
                                  const std::set<std::string>& hooks) {
    const auto offeredHooks = offers.offeredHooksFor(offered);
    std::set<std::string> missing;
    std::set_difference(hooks.begin(), hooks.end(), offeredHooks.begin(), offeredHooks.end(),
                        std::inserter(missing, missing.begin()));
    if (!missing.empty()) {
        eckit::Log::warning() << "Parameter " << offered
                              << (offered == required ? "" : " (needed by parameter " + required + ")")
                              << " is not offered by the model at hook point(s) " << hooksToString(missing) << "!"
                              << std::endl;
        return false;
    }
    return true;
}


bool Negotiator::isAccessOffered(const Protocol& offers, const data::ParameterDefinition& param,
                                 const std::set<std::string>& hooks) {

    if (!param.writable()) {
        return true;
    }

    // If the plugin requests write access (at some hook points):
    //   1. The write-back policy must permit it (not disabled).
    //   2. The hook points it is requested writable at must be ones it is needed at.
    //   3. The model must have offered the parameter writable at each of these hook points.
    // Derived parameters cannot be writable (enforced at ParameterDefinition construction), so param.name()
    // is always the name the model offered directly.
    if (policy_ == WriteBackPolicy::disabled) {
        eckit::Log::warning() << "Parameter \"" << param.name()
                              << "\" requested as writable but write-back is disabled "
                                 "(set \"write-back-policy\" in manager config)."
                              << std::endl;
        return false;
    }

    for (const auto& hook : param.writableHooks()) {
        if (hooks.find(hook) == hooks.end()) {
            eckit::Log::warning() << "Parameter \"" << param.name() << "\" requested as writable at hook point "
                                  << hook << ", where it is not needed (needed at " << hooksToString(hooks) << ")!"
                                  << std::endl;
            return false;
        }
    }

    const auto& offered = offers.offers().getParam(param.name());
    for (const auto& hook : hooks) {
        if (param.isWritableAt(hook) && !offered.isWritableAt(hook)) {
            eckit::Log::warning() << "Parameter \"" << param.name() << "\" requested as writable at hook point "
                                  << hook << " but not offered as writable there!" << std::endl;
            return false;
        }
    }

    return true;
}


bool Negotiator::isParamOffered(const Protocol& offers, const data::ParameterDefinition& param,
                                const std::set<std::string>& agreedHooks) {
    eckit::Log::info() << " - Considering Parameter: " << param.name() << std::endl;

    // a param can only be required at hook points the plugin is bound to
    const auto hooks = effectiveHooks(param, agreedHooks);
    for (const auto& hook : hooks) {
        if (agreedHooks.find(hook) == agreedHooks.end()) {
            eckit::Log::warning() << "Parameter " << param.name() << " is required at hook point " << hook
                                  << ", but the plugin is not bound to it (bound to " << hooksToString(agreedHooks)
                                  << ")!" << std::endl;
            return false;
        }
    }

    if (offers.isParamOffered(param.name())) {
        // offered as is: it must be offered at every hook point it is needed at, with the requested access mode
        return isOfferedAtHooks(offers, param.name(), param.name(), hooks) && isAccessOffered(offers, param, hooks);
    }

    if (offers.isParamOffered(param.sourceParam())) {
        // it is a derived param whose source is offered: the source and all the dependencies must be offered at
        // every hook point the derived param is needed at, for it to be recomputed from current data there
        if (!isOfferedAtHooks(offers, param.sourceParam(), param.name(), hooks)) {
            return false;
        }
        for (const auto& dependency : param.dependencies()) {
            if (!offers.isParamOffered(dependency)) {
                eckit::Log::warning() << "Dependency " << dependency << " of parameter " << param.name()
                                      << " not found!" << std::endl;
                return false;
            }
            if (!isOfferedAtHooks(offers, dependency, param.name(), hooks)) {
                return false;
            }
        }
        return true;
    }

    eckit::Log::warning() << "Parameter " << param.name() << " not found or source not found!" << std::endl;
    return false;
}


std::set<std::string> Negotiator::resolveHooks(const Protocol& requires,
                                               const std::optional<std::set<std::string>>& config_hooks) {

    // hook points from the configuration replace the ones declared by the plugin
    std::set<std::string> hooks = config_hooks.has_value() ? config_hooks.value() : requires.requiredHooks();

    // a plugin that asks for no hook point runs at the default one
    if (hooks.empty()) {
        hooks.insert(DEFAULT_HOOK);
    }

    return hooks;
}


bool Negotiator::areHooksOffered(const Protocol& offers, const std::set<std::string>& hooks) {

    eckit::Log::info() << "Requesting Hook Points: " << hooksToString(hooks) << std::endl;

    // all-or-nothing: one hook point not offered by the model rejects the plugin
    for (const auto& hook : hooks) {
        if (!offers.isHookOffered(hook)) {
            eckit::Log::warning() << "Hook point " << hook << " not offered by the model!" << std::endl;
            return false;
        }
    }

    return true;
}


bool Negotiator::checkConflicts(const std::string& pluginName, const PluginDecision& decision) {

    auto first = [](const std::map<ParamHook, std::vector<std::string>>& claimants, const ParamHook& key) {
        auto it = claimants.find(key);
        return (it == claimants.end() || it->second.empty()) ? std::string{} : it->second.front();
    };

    for (const auto& param : decision.offeredParams()) {
        const std::string& pname  = param.name();
        const std::string& source = param.sourceParam();  // non-empty only for derived params
        const bool isDerived      = !source.empty();

        for (const auto& hook : decision.hooksOf(pname)) {
            const ParamHook key{pname, hook};
            const std::string at = "\" at hook point \"" + hook + "\"";

            if (param.isWritableAt(hook)) {
                // Write-Write conflict at the same hook point
                const auto writer = first(writableClaimants_, key);
                if (!writer.empty() && !isMultiWriter(policy_)) {
                    eckit::Log::error() << "Plume negotiation: plugin \"" << pluginName
                                        << "\" rejected — write-write conflict: param \"" << pname << at
                                        << " already written by \"" << writer << "\"." << std::endl;
                    return false;
                }

                if (isStrictPolicy(policy_)) {
                    // Write-Read conflict: existing readers at the same hook point
                    if (!first(readonlyClaimants_, key).empty()) {
                        eckit::Log::error() << "Plume negotiation: plugin \"" << pluginName
                                            << "\" rejected under strict policy — write-read conflict: param \""
                                            << pname << at << " is already read (read-only) by one or more plugins."
                                            << std::endl;
                        return false;
                    }
                    // Write-DerivedRead conflict: existing derived readers at the same hook point
                    if (!first(derivedClaimants_, key).empty()) {
                        eckit::Log::error()
                            << "Plume negotiation: plugin \"" << pluginName
                            << "\" rejected under strict policy — write-derived-read conflict: a derived version of "
                               "param \""
                            << pname << at << " is already consumed by one or more plugins." << std::endl;
                        return false;
                    }
                }
            }
            else if (!isDerived) {
                // Read-Write conflict: existing writer at the same hook point
                const auto writer = first(writableClaimants_, key);
                if (!writer.empty() && isStrictPolicy(policy_)) {
                    eckit::Log::error() << "Plume negotiation: plugin \"" << pluginName
                                        << "\" rejected under strict policy — read-write conflict: param \"" << pname
                                        << at << " is already written by \"" << writer << "\"." << std::endl;
                    return false;
                }
            }
            else {
                // DerivedRead-Write conflict: existing writer of the source at the same hook point
                const auto writer = first(writableClaimants_, ParamHook{source, hook});
                if (!writer.empty() && isStrictPolicy(policy_)) {
                    eckit::Log::error() << "Plume negotiation: plugin \"" << pluginName
                                        << "\" rejected under strict policy — derived-read-write conflict: "
                                        << "source param \"" << source << "\" of derived param \"" << pname
                                        << "\" is already written at hook point \"" << hook << "\" by \"" << writer
                                        << "\"." << std::endl;
                    return false;
                }
            }
        }
    }
    return true;
}


void Negotiator::logCrossHookInteractions(const std::string& pluginName, const std::string& param,
                                          const std::string& hook, const std::string& mode) const {

    // only interactions that involve a write are of interest
    auto logClaims = [&](const std::map<ParamHook, std::vector<std::string>>& claimants, const std::string& other) {
        for (auto it = claimants.lower_bound(ParamHook{param, ""}); it != claimants.end() && it->first.first == param;
             ++it) {
            if (it->first.second == hook) {
                continue;
            }
            for (const auto& otherPlugin : it->second) {
                eckit::Log::info() << "Plume negotiation: cross-hook interaction on param \"" << param << "\": \""
                                   << pluginName << "\" (" << mode << " at \"" << hook << "\") and \"" << otherPlugin
                                   << "\" (" << other << " at \"" << it->first.second
                                   << "\"). Their order is set by the order in which the model calls the hook points."
                                   << std::endl;
            }
        }
    };

    logClaims(writableClaimants_, "write");
    if (mode == "write") {
        logClaims(readonlyClaimants_, "read");
        logClaims(derivedClaimants_, "derived-read");
    }
}


void Negotiator::recordClaims(const std::string& pluginName, const PluginDecision& decision) {
    executionOrder_.push_back({pluginName, decision});

    for (const auto& param : decision.offeredParams()) {
        const std::string& pname  = param.name();
        const std::string& source = param.sourceParam();
        for (const auto& hook : decision.hooksOf(pname)) {
            if (param.isWritableAt(hook)) {
                logCrossHookInteractions(pluginName, pname, hook, "write");
                writableClaimants_[{pname, hook}].push_back(pluginName);
            }
            else if (source.empty()) {
                logCrossHookInteractions(pluginName, pname, hook, "read");
                readonlyClaimants_[{pname, hook}].push_back(pluginName);
            }
            else {
                logCrossHookInteractions(pluginName, source, hook, "derived-read");
                derivedClaimants_[{source, hook}].push_back(pluginName);
            }
        }
    }
}


void Negotiator::logSummary() const {
    if (executionOrder_.empty()) {
        eckit::Log::info() << "Plume negotiation summary: no plugins accepted." << std::endl;
        return;
    }

    std::set<std::string> allHooks;
    for (const auto& plugin : executionOrder_) {
        allHooks.insert(plugin.decision.agreedHooks().begin(), plugin.decision.agreedHooks().end());
    }

    eckit::Log::info() << "Plume negotiation summary — accepted plugin execution order, per hook point:" << std::endl;
    for (const auto& hook : displayOrder(allHooks)) {
        eckit::Log::info() << "  hook point \"" << hook << "\":" << std::endl;
        size_t rank = 0;
        for (const auto& plugin : executionOrder_) {
            const auto& decision = plugin.decision;
            if (decision.agreedHooks().count(hook) == 0) {
                continue;
            }
            eckit::Log::info() << "    [" << (++rank) << "] " << plugin.name << std::endl;
            for (const auto& param : decision.offeredParams()) {
                if (decision.hooksOf(param.name()).count(hook) == 0) {
                    continue;
                }
                std::string mode;
                if (param.isWritableAt(hook)) {
                    mode = "write-back";
                }
                else if (!param.sourceParam().empty()) {
                    mode = "derived  (source: " + param.sourceParam() + ")";
                }
                else {
                    mode = "read-only";
                }
                eckit::Log::info() << "         - " << param.name() << "  (" << mode << ")" << std::endl;
            }
        }
    }
    if (allHooks.size() > 1) {
        eckit::Log::info() << "  The order across hook points is set by the order in which the model calls them."
                           << std::endl;
    }

    eckit::Log::info() << "Plume negotiation summary — accepted params claimants, per hook point:" << std::endl;
    std::map<ParamHook, std::vector<std::pair<std::string, std::string>>> claimantsByParam;
    auto mergeClaimants = [&claimantsByParam](const std::map<ParamHook, std::vector<std::string>>& claimants,
                                              const std::string& mode) {
        for (const auto& [key, plugins] : claimants) {
            for (const auto& pluginName : plugins) {
                claimantsByParam[key].emplace_back(pluginName, mode);
            }
        }
    };
    mergeClaimants(writableClaimants_, "write");
    mergeClaimants(readonlyClaimants_, "read");
    mergeClaimants(derivedClaimants_, "derived");

    for (const auto& [key, claimants] : claimantsByParam) {
        eckit::Log::info() << "  " << key.first << " @ " << key.second << ":" << std::endl;
        for (const auto& [pluginName, mode] : claimants) {
            eckit::Log::info() << "       - " << pluginName << "  (" << mode << ")" << std::endl;
        }
    }
}


WriteAuthorisation Negotiator::writeAuthorisation() const {
    WriteAuthorisation auth;
    for (const auto& [key, plugins] : writableClaimants_) {
        for (const auto& pluginName : plugins) {
            auth.grant(pluginName, key.first, key.second);
        }
    }
    return auth;
}


PluginDecision Negotiator::negotiate(const std::string& pluginName, const Protocol& offers,
                                     const Protocol& requires,
                                     const std::vector<eckit::LocalConfiguration>& config_params,
                                     const std::optional<std::set<std::string>>& config_hooks) {

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

    // Check the requested hook points (before the parameters: this is the cheapest check and it
    // gives the clearest rejection reason)
    std::set<std::string> agreedHooks = resolveHooks(requires, config_hooks);
    if (!areHooksOffered(offers, agreedHooks)) {
        return PluginDecision{false};
    }

    // Accumulate requested params in a map keyed by name.
    // The same param may be requested several times (from code and/or from config groups), with different hook
    // scopes and access modes: the definitions are merged per hook point, "writable wins" at each hook point
    // (see data::ParameterDefinition::mergeAccess). Using std::set<ParameterDefinition> directly would drop the
    // second insert because operator< compares by name only.
    std::map<std::string, data::ParameterDefinition> paramMap;
    auto insertOrMerge = [&paramMap, &pluginName](const data::ParameterDefinition& p) {
        auto it = paramMap.find(p.name());
        if (it == paramMap.end()) {
            paramMap.emplace(p.name(), p);
        }
        else if (it->second.mergeAccess(p)) {
            eckit::Log::warning() << "Plume negotiation: plugin \"" << pluginName << "\" — param \"" << p.name()
                                  << "\" is writable at every hook point: an unscoped writable definition overrides "
                                     "the read-only access requested at some hook points."
                                  << std::endl;
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
        if (!isParamOffered(offers, param, agreedHooks)) {
            return PluginDecision{false};
        }
    }

    try {
        for (const auto& p : requested_params) {
            insertOrMerge(p);
        }

        // 2) Check requested parameters (from the configuration)
        // Here we check "groups" of requested parameters. A plugin can run if AT LEAST ONE of the "groups" is
        // satisfied
        if (!config_params.empty()) {

            bool any_group_accepted = false;

            for (const auto& param_group : config_params) {

                eckit::Log::info() << "Considering Parameter Group..." << std::endl;

                const std::vector<eckit::LocalConfiguration>& params = param_group.getSubConfigurations();
                requested_params.clear();

                // loop over parameters in the group and check if they are all offered
                bool group_satisfied = std::all_of(
                    params.begin(), params.end(),
                    [this, &offers, &agreedHooks, &requested_params](const auto& paramConfig) {
                        data::ParameterDefinition param(paramConfig);
                        if (!isParamOffered(offers, param, agreedHooks)) {
                            return false;
                        }
                        requested_params.push_back(param);
                        return true;
                    });

                if (group_satisfied) {
                    eckit::Log::info() << " ---> Parameter Group Accepted!" << std::endl;
                    any_group_accepted = true;

                    // add all parameters in the group to paramMap, merged per hook point
                    for (const auto& p : requested_params) {
                        insertOrMerge(p);
                    }
                }
                else {
                    eckit::Log::warning() << "---> Group rejected!" << std::endl;
                }
            }

            if (!any_group_accepted) {
                return PluginDecision{false};
            }
        }
    }
    catch (const eckit::BadParameter& e) {
        // e.g. merging a writable definition into a derived param
        eckit::Log::error() << "Plume negotiation: plugin \"" << pluginName << "\" rejected — " << e.what()
                            << std::endl;
        return PluginDecision{false};
    }

    // Convert paramMap to the set expected by PluginDecision (unique by name, merged per hook point), and record the
    // hook points at which each param is needed (all the agreed hook points for unscoped ones).
    std::set<data::ParameterDefinition> allRequestedParams;
    std::map<std::string, std::set<std::string>> paramHooks;
    for (const auto& [name, param] : paramMap) {
        allRequestedParams.insert(param);
        paramHooks[name] = effectiveHooks(param, agreedHooks);
    }

    // 3) Cross-plugin conflict detection (stateful: checks against previously accepted plugins)
    PluginDecision decision(true, allRequestedParams, agreedHooks, paramHooks);
    if (!checkConflicts(pluginName, decision)) {
        return PluginDecision{false};
    }

    recordClaims(pluginName, decision);
    return decision;
}


}  // namespace plume
