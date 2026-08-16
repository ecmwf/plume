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

#include <optional>
#include <set>
#include <string>

#include "eckit/config/LocalConfiguration.h"

#include "Hook.h"
#include "PluginDecision.h"
#include "Protocol.h"
#include "data/ParameterCatalogue.h"


namespace plume {

class Negotiator {
private:
    bool isParamOffered(const Protocol& offers, const data::ParameterDefinition& param);

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

public:
    /**
     * @brief Negotiate with a plugin
     *
     * @param requires
     * @param config_params
     * @param config_hooks hook points from the plugin configuration (nullopt if the key is absent)
     * @return PluginDecision
     */
    PluginDecision negotiate(
        const Protocol& offers, const Protocol& requires,
        const std::vector<eckit::LocalConfiguration>& config_params = std::vector<eckit::LocalConfiguration>{},
        const std::optional<std::set<std::string>>& config_hooks = std::nullopt);
};


}  // namespace plume