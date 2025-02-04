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

#include "plume/Negotiator.h"
#include "plume/utils.h"
#include "plume/plume.h"

 namespace plume {


PluginDecision Negotiator::negotiate(const Protocol& offers, const Protocol& requires, const std::vector<eckit::LocalConfiguration>& config_params) {

    eckit::Log::info() << "Requesting Plume Version: " << LibVersion(requires.requiredPlumeVersion()).asString() 
                       <<  " VS Actual Plume version "<< plume_VERSION << std::endl;

    eckit::Log::info() << "Requesting Atlas Version: " << LibVersion(requires.requiredAtlasVersion()).asString() 
                       <<  " VS Actual Atlas version "<< offers.offeredAtlasVersion() << std::endl;
    
    // Check Plume version
    if (LibVersion(requires.requiredPlumeVersion()) > LibVersion( plume_VERSION )) {
        return PluginDecision{false};
    }

    // Check Atlas version
    if (LibVersion(requires.requiredAtlasVersion()) > LibVersion( offers.offeredAtlasVersion() )) {        
        return PluginDecision{false};
    }

    std::set<std::string> allRequestedParams;
    std::vector<std::string> requested_params = requires.requiredParamNames();

    // 1) Check requested parameters (from the plugin)
    // if ANY one of these parameters is not offered, the plugin is rejected
    eckit::Log::info() << "Requesting Parameters: [";
    if (requested_params.size()) {
        for (int i = 0; i < requested_params.size()-1; i++) {
            eckit::Log::info() << requested_params[i] << ", ";
        }
        eckit::Log::info() << requested_params[requested_params.size()-1] << "]" << std::endl;
    } else {
        eckit::Log::info() << "]" << std::endl;
    }    
    
    for (const auto& param_name : requested_params) {
        eckit::Log::info() << " - Considering Parameter: " << param_name << std::endl;
        if (!offers.isParamOffered(param_name)) {
            eckit::Log::warning() << "Parameter " << param_name << " not found!" << std::endl;
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

            // loop over parameters in the group and check if they are all offered
            bool group_satisfied = std::all_of(params.begin(), params.end(), [&offers](const auto& param){
                std::string param_name = param.getString("name");
                eckit::Log::info() << " - Considering Parameter: " << param_name << std::endl;
                if (!offers.isParamOffered(param_name)) {
                    eckit::Log::warning() << " ---> Configuration Parameter " << param_name << " not found! => Group rejected!" << std::endl;
                    return false;
                }
                return true;
            });

            // group is satisfied
            if (group_satisfied) {
                eckit::Log::info() << " ---> Parameter Group Accepted!" << std::endl;

                // add all parameters in the group to the set "allRequestedParams"
                for (const auto& param : params) {
                    allRequestedParams.insert(param.getString("name"));
                } 
            }
        }
    }

    return PluginDecision{true, std::vector<std::string>(allRequestedParams.begin(), allRequestedParams.end())};
};



 }  // namespace plume
