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
#include <set>

#include "plume/Negotiator.h"
#include "plume/plume.h"
#include "plume/utils.h"

namespace plume {

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
    return true;
}


PluginDecision Negotiator::negotiate(const Protocol& offers, const Protocol& requires,
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

    std::set<data::ParameterDefinition> allRequestedParams;
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
        allRequestedParams.insert(requested_params.begin(), requested_params.end());
    }

    // 2) Check requested parameters (from the configuration)
    // Here we check "groups" of requested parameters. A plugin can run if ANY of the "groups" are satisfied
    if (!config_params.empty()) {

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

                // add all parameters in the group to the set "allRequestedParams"
                allRequestedParams.insert(requested_params.begin(), requested_params.end());
            }
            else {
                eckit::Log::warning() << "---> Group rejected!" << std::endl;
            }
        }
    }

    return PluginDecision(true, allRequestedParams);
};


}  // namespace plume
