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

#include "Configurable.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"

#include "PluginConfig.h"

namespace plume {

class ManagerConfig final : public CheckedConfigurable {

public:

ManagerConfig() : 
    CheckedConfigurable{eckit::YAMLConfiguration(std::string("{\"plugins\":[]}")), {"plugins"}, {"verbose"}} {}

ManagerConfig(const eckit::Configuration& config) : 
    CheckedConfigurable{config, {"plugins"}, {"verbose"}} {

    // plugins must be a list
    if (!this->config().isSubConfigurationList("plugins")) {
        throw eckit::BadValue("ManagerConfig: plugins must be a list of configurations", Here());
    }

    // check each plugin configuration
    for (const auto& pconfig : this->config().getSubConfigurations("plugins")) {
        auto valid = PluginConfig::isValid(pconfig);
        if (!valid) {
            eckit::Log::error() << "Plugin configuration NOT valid:" << pconfig << std::endl;
            throw eckit::BadValue("ManagerConfig: plugin configuration is not valid", Here());
        }
    }

}


/**
 * @brief get the plugins (configuration)
 * 
 * @return std::vector<PluginConfig>
 */
std::vector<PluginConfig> plugins() const {

    std::vector<PluginConfig> pluginConfigs;

    auto pconfigs = config().getSubConfigurations("plugins");

    for (const auto& pconfig : pconfigs) {
        pluginConfigs.push_back(PluginConfig(pconfig));
    }

    return pluginConfigs;
}

};

}  // namespace plume