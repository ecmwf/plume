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
#include <vector>

#include "Configurable.h"

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"


namespace plume {

class PluginConfig : public CheckedConfigurable {

public:

    PluginConfig(const eckit::Configuration& config) :
        CheckedConfigurable{config, {"name", "lib"}, {"parameters", "core-config", "hooks"}} {
            if (!hasValidParameterFormat(config)) {
                throw eckit::BadValue("PluginConfig: parameters must be a list of configurations", Here());
            }
            if (!hasValidHooksFormat(config)) {
                throw eckit::BadValue("PluginConfig: hooks must be a non-empty list of hook point names", Here());
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
        return CheckedConfigurable::isValid(config, {"name", "lib"}, {"parameters", "core-config", "hooks"}) &&
               hasValidParameterFormat(config) && hasValidHooksFormat(config);
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

    /**
     * @brief get the hook points requested through the configuration (if any)
     *
     * An empty optional means that the key is absent, in which case the hook points declared by
     * the plugin itself apply. A value means that the configuration *replaces* what the plugin
     * declared, so that a deployment can re-target a plugin without recompiling it.
     *
     * @return std::optional<std::set<std::string>>
     */
    std::optional<std::set<std::string>> hooks() const {
        if (!config().has("hooks")) {
            return std::nullopt;
        }
        std::vector<std::string> hookNames = config().getStringVector("hooks");
        return std::set<std::string>(hookNames.begin(), hookNames.end());
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

    static bool hasValidHooksFormat(const eckit::Configuration& config) {
        if (config.has("hooks")) {
            if (!config.isList("hooks")) {
                return false;
            }
            // an empty list is rejected: it is not the same as the key being absent, and
            // conflating the two would hide a configuration mistake
            std::vector<std::string> hookNames;
            try {
                hookNames = config.getStringVector("hooks");
            }
            catch (const eckit::Exception&) {
                // not a list of strings
                return false;
            }
            if (hookNames.empty()) {
                return false;
            }
        }
        return true;
    }

};
}  // namespace plume