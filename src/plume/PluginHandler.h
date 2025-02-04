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

#include <memory>

#include "plume/Plugin.h"
#include "plume/PluginCore.h"
#include "plume/PluginConfig.h"


namespace plume {

/**
 * @brief Handles a Plugin (and its association to a PluginCore)
 * 
 */
class PluginHandler {

public:

    PluginHandler(Plugin& plugin, const PluginConfig& config, const std::vector<std::string>& offeredParams);

    ~PluginHandler() = default;

    // Delete the copy constructor and assignment operator
    PluginHandler(const PluginHandler&) = delete;
    PluginHandler& operator=(const PluginHandler&) = delete;

    // Default move constructor and assignment operator
    PluginHandler(PluginHandler&& other) = default;
    PluginHandler& operator=(PluginHandler&& other) = default;

    /**
     * @brief is Active
     * 
     * @return true 
     * @return false 
     */
    bool isActive() const ;

    /** Activate this plugin (associating it to a plugincore)
     * @brief 
     * 
     * @param plugincorePtr 
     */
    void activate(std::unique_ptr<PluginCore> plugincorePtr);

    /**
     * @brief Get the Active Param Names
     * 
     * @return std::vector<std::string> 
     */
    const std::vector<std::string>& getRequiredParamNames() const;

    /**
     * @brief Forward data to the plugincore
     * 
     * @param data 
     */
    void grabData(const data::ModelData& data);

    /**
     * @brief setup the plugincore
     * 
     */
    void setup();

    /**
     * @brief run the plugincore
     * 
     */
    void run();

    /**
     * @brief teardown the plugincore
     * 
     */
    void teardown();

private:

    // internal Plugin ref
    Plugin& pluginRef_;

    // store the plugin configuration
    PluginConfig config_;

    // internal PluginCore ptr
    std::unique_ptr<PluginCore> plugincorePtr_;

    // offered parameters
    std::vector<std::string> offeredParams_;
};

}  // namespace plume