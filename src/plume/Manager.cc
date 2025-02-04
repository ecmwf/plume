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
#include <dirent.h>
#include <cstdlib>
#include <functional>
#include <map>
#include <algorithm>
#include <memory>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"
#include "eckit/runtime/Main.h"
#include "eckit/utils/StringTools.h"

#include "plume/plume.h"
#include "plume/utils.h"
#include "plume/PluginCore.h"
#include "plume/PluginConfig.h"
#include "plume/PluginHandler.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/DataChecker.h"
#include "plume/Protocol.h"

#include "plume/Manager.h"
#include "plume/Negotiator.h"



namespace plume {

/**
 * @brief Plugin registry (Singleton)
 * 
 */
class PluginRegistry {

public:
    static PluginRegistry& instance() {
        static PluginRegistry reg;
        return reg;
    }

    void setActive(Plugin& plugin, const PluginConfig& pconfig, const std::vector<std::string>& offeredParams) {

        std::string name = plugin.plugincoreName();

        // create a plugin handler
        PluginHandler pluginHandle(plugin, pconfig, offeredParams);

        // instantiate the plugincore (the plugin handler takes ownership of it)
        pluginHandle.activate( std::unique_ptr<PluginCore>( plume::PluginCoreFactory::instance().build(name, pconfig.coreConfig()) ) );

        // plugin added to the active plugin list
        PluginRegistry::instance().pluginHandlers_.push_back(std::move(pluginHandle));
    }

    // get the active Plugins
    std::vector<PluginHandler>& getActivePlugins() { return pluginHandlers_; }


    // Parameters requested by all active plugins collectively
    std::unordered_set<std::string> getActiveParams() {
        std::unordered_set<std::string> requiredParams;
        for (const auto& pluginHandle : pluginHandlers_) {
            auto req_fields = pluginHandle.getRequiredParamNames();
            requiredParams.insert(req_fields.begin(), req_fields.end());
        }
        return requiredParams;
    }

    data::ParameterCatalogue getActiveDataCatalogue() {
        return dataCatalogue_.filter(getActiveParams());
    }

    void setDataCatalogue(const data::ParameterCatalogue& dataCatalogue) {
        dataCatalogue_ = dataCatalogue;
    }

    const data::ParameterCatalogue& getDataCatalogue() {
        return dataCatalogue_;
    }

private:
    // List of active plugins
    std::vector<PluginHandler> pluginHandlers_;

    // stores a copy of the data catalogue that 
    // resulted in the activated plugins
    data::ParameterCatalogue dataCatalogue_;

};
// -------------------------------------------------------------------


ManagerConfig Manager::managerConfig_{};

bool Manager::isConfigured_{false};



void Manager::configure(const eckit::Configuration& config) {
    if (!Manager::isConfigured_){
        managerConfig_ = ManagerConfig(config);
        Manager::isConfigured_ = true;
    }
}

// load a plugin from a shared library
Plugin& Manager::loadPlugin(const std::string& lib, const std::string& name) {

    void* libHandle = eckit::system::LibraryManager::loadLibrary(lib);
    if (!libHandle) {
        throw eckit::BadValue("Loading library " + lib + " failed!", Here());
    }

    eckit::Log::info() << "Loading Library: " << lib << " containing Plugin: " << name << std::endl;

    // here we are loading a Plume plugin
    Plugin& plugin = dynamic_cast<Plugin&>(eckit::system::LibraryManager::loadPlugin(name));

    return plugin;
}


// Negotiate with all candidate plugins
void Manager::negotiate(const Protocol& offers) {

    // before negotiation, make sure the manager has been configured
    ASSERT_MSG(isConfigured_, "Plume manager needs to be configured first!");

    eckit::Log::info() << "Plume config: " << managerConfig_ << ", offers: " << offers.offeredParamNames() << std::endl;

    // Negotiate with each plugin
    Negotiator negotiator;
    
    // Load all selected plugins as per configuration
    for (const auto& pconfig : managerConfig_.plugins()) {
        
        auto name = pconfig.name();
        auto lib = pconfig.lib();

        eckit::Log::info() << std::endl << " <== Evaluating Plugin: " << name << " from Library: " << lib << std::endl;

        // Load the plugin
        Plugin& plugin = loadPlugin(lib, name);

        // check what each plugin requires
        Protocol requires = plugin.negotiate();

        // Check plugin parameters requested through configuration (if any)
        auto config_params = pconfig.parameters();
        if (config_params.size() > 0) {
            eckit::Log::info() << "Parameters from Config: " << config_params << std::endl;
        } else {
            eckit::Log::info() << "No additional parameters found in Config." << std::endl;
        }

        // negotiator handles the negotiation
        PluginDecision decision = negotiator.negotiate(offers, requires, config_params);
        eckit::Log::info() << decision << std::endl;

        // If the plugin is accepted, set it as active
        if (decision.accepted()) {
            PluginRegistry::instance().setActive(plugin, pconfig, decision.offeredParams());
        }
        
    }

    PluginRegistry::instance().setDataCatalogue(offers.offers());
};


// Let each plugin take its own share of data (pointers)
void Manager::feedPlugins(const data::ModelData& data) {

    // check data
    Manager::checkData(data);

    // Run each PluginCore for every active plugin
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {

        // get the share of run data needed to run the plugincore
        auto requiredParams = pluginHandler.getRequiredParamNames();
        data::ModelData requiredData = data.filter(requiredParams);

        // grab data 
        pluginHandler.grabData(requiredData);

        // setup
        pluginHandler.setup();
    }
}


// Run all active plugincores
void Manager::run() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        pluginHandler.run();
    }
};


// Teardown all active plugins
void Manager::teardown() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        // teardown the plugincore first
        pluginHandler.teardown();
    }
};


std::unordered_set<std::string> Manager::getActiveParams() {
    return PluginRegistry::instance().getActiveParams();
}


data::ParameterCatalogue Manager::getActiveDataCatalogue() {    
    return PluginRegistry::instance().getActiveDataCatalogue();
}


bool Manager::isParamRequested(const std::string& name) {
    auto activeParams = Manager::getActiveParams();
    if (find(activeParams.begin(), activeParams.end(), name) != activeParams.end()) {
        return true;
    } else {
        return false;
    }
}


bool Manager::isConfigured() {
    return Manager::isConfigured_;
}


void Manager::checkData(const data::ModelData& data) {

    eckit::Log::info() << "--- Plume manager is checking data ..." << std::endl;

    // Check all requested params (regardless of whether they are "always-available" or "on-demand")    
    data::DataChecker::checkAllParams(data, PluginRegistry::instance().getActiveDataCatalogue(), plume::data::CheckPolicyWarning{} );

    // Check that all the "always" params are present
    data::DataChecker::checkAlwaysAvailParams(data, PluginRegistry::instance().getDataCatalogue(), plume::data::CheckPolicyWarning{});

    eckit::Log::info() << "--- Plume manager has checked data." << std::endl;
}


}  // namespace plume
