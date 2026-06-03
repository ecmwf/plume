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
#include "coupling/WriteBackPolicy.h"

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/exception/Exceptions.h"

#include "PluginConfig.h"

namespace plume {

class ManagerConfig final : public CheckedConfigurable {

public:

ManagerConfig() : 
    CheckedConfigurable{eckit::YAMLConfiguration(std::string("{\"plugins\":[]}")),
                        {"plugins"}, {"verbose", "write-back-policy"}} {}

ManagerConfig(const eckit::Configuration& config) : 
    CheckedConfigurable{config, {"plugins"}, {"verbose", "write-back-policy"}} {

    // Validate write-back-policy string early so errors are caught at configure() time.
    if (this->config().isString("write-back-policy")) {
        writeBackPolicyFromString(this->config().getString("write-back-policy"));
    }

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

/**
 * @brief Write-back policy for this manager instance.
 *
 * Returns WriteBackPolicy::disabled when the "write-back-policy" key is absent,
 * ensuring existing model configurations that do not use write-back are unaffected.
 * Throws eckit::BadValue if an unrecognised policy string is supplied.
 *
 * @return WriteBackPolicy
 */
WriteBackPolicy writeBackPolicy() const {
    return writeBackPolicyFromString(config().getString("write-back-policy", "disabled"));
}

};

}  // namespace plume