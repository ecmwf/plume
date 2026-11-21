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

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "eckit/config/LocalConfiguration.h"

#include "Hook.h"
#include "PluginDecision.h"
#include "Protocol.h"
#include "coupling/WriteAuthorisation.h"
#include "coupling/WriteBackPolicy.h"
#include "data/ParameterCatalogue.h"


namespace plume {

class Negotiator {
private:
    WriteBackPolicy policy_;

    // Cross-plugin state, accumulated across negotiate() calls (config order = run order within a hook point).
    // Claims are per (param, hook point): the access mode of a param may differ from one hook point to another, and
    // conflicts are only evaluated between claims at the same hook point.
    using ParamHook = std::pair<std::string, std::string>;               // (param, hook point)
    std::map<ParamHook, std::vector<std::string>> writableClaimants_;   // (param, hook)  -> writers
    std::map<ParamHook, std::vector<std::string>> readonlyClaimants_;   // (param, hook)  -> readers
    std::map<ParamHook, std::vector<std::string>> derivedClaimants_;    // (source, hook) -> derived-readers

    // Accepted plugins in config order, with their decision (params, hook points, per-hook access).
    struct AcceptedPlugin {
        std::string name;
        PluginDecision decision;
    };
    std::vector<AcceptedPlugin> executionOrder_;

    /**
     * @brief Is a required param offered by the model at every hook point it is needed at, with the access mode
     *        it is needed with there?
     *
     * The param is needed at its hook scope, or at all agreedHooks if it is unscoped; a scope naming a hook point
     * the plugin is not bound to rejects it. A param offered as is must be offered at those hook points; a derived
     * param needs its source and all its dependencies offered there. At each hook point where the param is
     * required writable, the write-back policy must not be disabled and the model must offer it writable there.
     */
    bool isParamOffered(const Protocol& offers, const data::ParameterDefinition& param,
                        const std::set<std::string>& agreedHooks);

    // the writable access requested for a (base) param is offered at each of its hook points
    bool isAccessOffered(const Protocol& offers, const data::ParameterDefinition& param,
                         const std::set<std::string>& hooks);

    // the offered param "offered" (needed by the required param "required") is offered at all of "hooks"
    static bool isOfferedAtHooks(const Protocol& offers, const std::string& offered, const std::string& required,
                                 const std::set<std::string>& hooks);

    // hook points a required param is needed at: its scope, or all the hook points the plugin is bound to
    static std::set<std::string> effectiveHooks(const data::ParameterDefinition& param,
                                                const std::set<std::string>& agreedHooks);

    /**
     * @brief Work out the hook points a plugin is asking for.
     *
     * Hook points requested through the configuration *replace* the ones the plugin declared, so
     * that a deployment can re-target a plugin without recompiling it. A plugin that asks for no
     * hook point at all is bound to plume::DEFAULT_HOOK.
     */
    std::set<std::string> resolveHooks(const Protocol& requires,
                                       const std::optional<std::set<std::string>>& config_hooks);

    /**
     * @brief Are all the requested hook points offered by the model?
     *
     * All-or-nothing, consistently with the parameters declared by the plugin.
     */
    bool areHooksOffered(const Protocol& offers, const std::set<std::string>& hooks);

    /**
     * @brief Check cross-plugin conflicts for an accepted single-plugin decision, per (param, hook point).
     *
     * Write-write, write-read and write-derived-read conflicts are evaluated between claims at the same hook point,
     * following the write-back policy. Interactions between claims at different hook points are always allowed and
     * logged, as their order is set by the order in which the model calls the hook points.
     *
     * @return true if the plugin should be activated; false if it must be rejected.
     */
    bool checkConflicts(const std::string& pluginName, const PluginDecision& decision);

    // log the interactions of a claim with the claims on the same param at other hook points
    void logCrossHookInteractions(const std::string& pluginName, const std::string& param, const std::string& hook,
                                  const std::string& mode) const;

    // Record the accepted plugin's (param, hook point) claims into the tracking maps.
    void recordClaims(const std::string& pluginName, const PluginDecision& decision);

public:
    /**
     * @brief Construct a Negotiator with the given write-back policy.
     *
     * @param policy  Controls whether plugins may request writable parameters.
     *                Defaults to WriteBackPolicy::disabled — no write-back allowed.
     */
    explicit Negotiator(WriteBackPolicy policy = WriteBackPolicy::disabled);

    /**
     * @brief Negotiate with a single plugin (per-plugin checks + cross-plugin conflict detection).
     *
     * Per-plugin checks (stateless): version compatibility, hook points, parameter availability and access mode at
     * each hook point, writable policy gate.
     * Cross-plugin checks (stateful): write-write, write-read and write-derived-read conflicts per (param, hook).
     * If accepted, the plugin's claims are recorded for future calls.
     *
     * @param pluginName    Name of the plugin being negotiated (used in log messages and claim maps).
     * @param offers        Parameters and hook points offered by the model.
     * @param requires      Parameters and hook points required by the plugin.
     * @param config_params Optional parameter groups from manager configuration.
     * @param config_hooks  Hook points from the plugin configuration (nullopt if the key is absent).
     * @return PluginDecision with accepted=true if the plugin passes all checks.
     */
    PluginDecision negotiate(
        const std::string& pluginName, const Protocol& offers, const Protocol& requires,
        const std::vector<eckit::LocalConfiguration>& config_params = std::vector<eckit::LocalConfiguration>{},
        const std::optional<std::set<std::string>>& config_hooks    = std::nullopt);

    /**
     * @brief Emit a post-negotiation summary of the accepted plugins, per hook point.
     *
     * For each hook point, logs the accepted plugins bound to it in run order, and each parameter they use there
     * with its access mode at that hook point: "write-back", "read-only", or "derived (source: X)". It also logs the
     * mapping of accepted (parameter, hook point) pairs to their claimants. The order across hook points is set by
     * the model. Should be called once after all plugins have been negotiated.
     */
    void logSummary() const;

    /**
     * @brief Build and return the write authorisation table for the completed negotiation.
     *
     * Grants each accepted plugin write access to a parameter at each hook point where it requested it writable.
     * Must be called after all negotiate() calls are complete.
     */
    WriteAuthorisation writeAuthorisation() const;
};


}  // namespace plume
