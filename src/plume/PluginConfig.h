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


namespace plume {

class PluginConfig : public CheckedConfigurable {

public:

    PluginConfig(const eckit::Configuration& config) : 
        CheckedConfigurable{config, {"name", "lib"}, {"parameters", "core-config"}} {
            if (!hasValidParameterFormat(config)) {
                throw eckit::BadValue("PluginConfig: parameters must be a list of configurations", Here());
            }
        }

    /**
     * @brief check if the plugin configuration is valid
     * 
     * @param config 
     * @return true 
     * @return false 
     */
    static bool isValid(const eckit::Configuration& config) {
        return CheckedConfigurable::isValid(config, {"name", "lib"}, {"parameters", "core-config"}) && hasValidParameterFormat(config);
    }

    /**
     * @brief get the name of the plugin
     * 
     * @return std::string 
     */
    std::string name() const {
        return config().getString("name");
    }

    /**
     * @brief get the library name of the plugin
     * 
     * @return std::string 
     */
    std::string lib() const {
        return config().getString("lib");
    }

    /**
     * @brief get the optional required parameters
     * 
     * @return std::vector<eckit::LocalConfiguration> 
     */
    std::vector<eckit::LocalConfiguration> parameters() const {
        std::vector<eckit::LocalConfiguration> config_params;
        if (config().has("parameters")) {
            config_params = config().getSubConfigurations("parameters");
        }
        return config_params;
    }

    /**
     * @brief get the plugincore configuration
     * 
     * @return eckit::LocalConfiguration 
     */
    eckit::LocalConfiguration coreConfig() const {
        return config().getSubConfiguration("core-config");
    }

private:

    static bool hasValidParameterFormat(const eckit::Configuration& config) {
        if (config.has("parameters")) {
            if (!config.isList("parameters")) {
                return false;
            }
        }
        return true;

    }

};
}  // namespace plume