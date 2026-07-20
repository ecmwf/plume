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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"

#include "PluginDecision.h"
#include "Protocol.h"
#include "coupling/WriteAuthorisation.h"
#include "coupling/WriteBackPolicy.h"
#include "data/ParameterCatalogue.h"


namespace plume {

class Negotiator {
private:
    WriteBackPolicy policy_;

    // Cross-plugin state: accumulated across negotiate() calls (config order = run order).
    std::map<std::string, std::vector<std::string>> writableClaimants_;   // param -> writers
    std::map<std::string, std::vector<std::string>> readonlyClaimants_;   // param -> readers
    std::map<std::string, std::vector<std::string>> derivedClaimants_;    // source param -> derived-readers

    // Execution order: accepted plugins in config order, each with their accepted params.
    struct AcceptedPlugin {
        std::string name;
        std::vector<data::ParameterDefinition> params;
    };
    std::vector<AcceptedPlugin> executionOrder_;

    bool isParamOffered(const Protocol& offers, const data::ParameterDefinition& param);

    // Check cross-plugin conflicts for an accepted single-plugin decision.
    // Returns true if the plugin should be activated; false if it must be rejected.
    bool checkConflicts(const std::string& pluginName, const PluginDecision& decision);

    // Record the accepted plugin's parameter claims into the tracking maps.
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
     * Per-plugin checks (stateless): version compatibility, parameter availability, writable policy gate.
     * Cross-plugin checks (stateful): write-write, write-read and write-derived-read conflicts.
     * If accepted, the plugin's claims are recorded for future calls.
     *
     * @param pluginName   Name of the plugin being negotiated (used in log messages and claim maps).
     * @param offers       Parameters offered by the model.
     * @param requires     Parameters required by the plugin.
     * @param config_params Optional parameter groups from manager configuration.
     * @return PluginDecision with accepted=true if the plugin passes all checks.
     */
    PluginDecision negotiate(
        const std::string& pluginName,
        const Protocol& offers, const Protocol& requires,
        const std::vector<eckit::LocalConfiguration>& config_params = std::vector<eckit::LocalConfiguration>{});

    /**
     * @brief Emit a post-negotiation summary of the accepted plugin execution order.
     *
     * For each accepted plugin in run order, logs each parameter it will use and
     * its access mode: "write-back", "read-only", or "derived (source: X)".
     * Should be called once after all plugins have been negotiated.
     */
    void logSummary() const;

    /**
     * @brief Build and return the write authorisation table for the completed negotiation.
     *
     * Returns a WriteAuthorisation mapping each accepted plugin name to the set of parameter names it has been granted
     * write access to. Only plugins that explicitly requested write access (writable: true) appear; read-only
     * requesters are excluded. Must be called after all negotiate() calls are complete.
     */
    WriteAuthorisation writeAuthorisation() const;
};


}  // namespace plume