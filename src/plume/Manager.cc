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

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"
#include "eckit/runtime/Main.h"
#include "eckit/utils/StringTools.h"

#include "plume/plume.h"
#include "plume/utils.h"
#include "plume/PluginCore.h"
#include "plume/PluginHandler.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/DataChecker.h"
#include "plume/Protocol.h"

#include "plume/Manager.h"



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

    ~PluginRegistry() {
        // Destroy the active plugincores (i.e. plugincores built when the corresponding plugins were set as active)
        for (plume::PluginHandler pluginHandle : PluginRegistry::instance().getActivePlugins()) {
            
            plume::PluginCore* plugincorePtr = pluginHandle.plugincore();

            // Plugin is now disassociated
            pluginHandle.deactivate();

            // delete the plugincore
            delete plugincorePtr;

        }
    }

    void setActive(Plugin& plugin, const eckit::Configuration& config) {

        std::string name = plugin.plugincoreName();
        eckit::LocalConfiguration plugincoreConfig = config.getSubConfiguration("plugincore-config");
        plume::PluginCore* plugincorePtr  = plume::PluginCoreFactory::instance().build(name, plugincoreConfig);

        PluginHandler pluginHandle(&plugin);

        pluginHandle.activate(plugincorePtr);

        // plugin added to the active plugin list
        PluginRegistry::instance().pluginHandlers_.push_back(pluginHandle);
    }

    // get the active Plugins
    std::vector<PluginHandler> getActivePlugins() { return pluginHandlers_; }


    // Parameters requested by all active plugins collectively
    std::vector<std::string> getActiveParams() {
        std::vector<std::string> requiredParams;
        for (const auto& pluginHandle : pluginHandlers_) {
            auto req_fields = pluginHandle.plugin()->negotiate().requiredParamNames();
            requiredParams.insert(requiredParams.end(), req_fields.begin(), req_fields.end());
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


eckit::LocalConfiguration Manager::config_{};
bool Manager::isConfigured_{false};



void Manager::configure(const eckit::Configuration& config) {
    if (!Manager::isConfigured_){
        eckit::LocalConfiguration tmp{config};
        config_ = tmp;
        Manager::isConfigured_ = true;
    }
}


// Negotiate with all candidate plugins
void Manager::negotiate(const Protocol& offers) {

    // before negotiation, make sure the manager has been configured
    ASSERT_MSG(isConfigured_, "Plume manager needs to be configured first!");

    eckit::Log::info() << "Plume config: " << config_ 
                       << ", offers: " << offers.offeredParamNames() << std::endl;

    std::vector<eckit::LocalConfiguration> plugins = config_.getSubConfigurations("plugins");

    // Load all selected plugins as per configuration
    for (const auto& conf : plugins) {

        std::string name = conf.getString("name");
        std::string lib  = conf.getString("lib");

        void* libHandle = eckit::system::LibraryManager::loadLibrary(lib);
        if (!libHandle) {
            throw eckit::BadValue("Loading library " + lib + " failed!", Here());
        }

        eckit::Log::info() << "Loading Library: " << lib << " containing Plugin: " << name << std::endl;

        // here we are loading a Plume plugin
        Plugin& plugin = dynamic_cast<Plugin&>(eckit::system::LibraryManager::loadPlugin(name));

        // Negotiate with each plugin
        Protocol requires = plugin.negotiate();
        if (decideOnPlugin(offers, requires)) {

            eckit::Log::info() << " ==> Plugin manager has accepted plugin: " << plugin.name() << std::endl;

            // Evaluation of plugin successful => Plugin now set as active
            PluginRegistry::instance().setActive(plugin, conf);
        }
        else {
            eckit::Log::info() << " ==> Plugin manager has rejected plugin: " << plugin.name() << std::endl;
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
        auto requiredParams          = pluginHandler.plugin()->negotiate().requiredParamNames();
        data::ModelData requiredData = data.filter(requiredParams);

        // get the associated plugincore
        plume::PluginCore* plugincore  = pluginHandler.plugincore();

        // grab data
        plugincore->grabData(requiredData);

        // setup
        plugincore->setup();
    }
}


// Run all active plugincores
void Manager::run() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        pluginHandler.plugincore()->run();
    }
};


// Teardown all active plugins
void Manager::teardown() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        // teardown the plugincore first
        pluginHandler.plugincore()->teardown();
    }
};


bool Manager::decideOnPlugin(const Protocol& offers, const Protocol& requires) {

    /// TODO: this negotiation should be done in a separate class

    eckit::Log::info() << "Requesting Plume Version: " << LibVersion(requires.requiredPlumeVersion()).asString() 
                       <<  " VS Actual Plume version "<< plume_VERSION << std::endl;

    eckit::Log::info() << "Requesting Atlas Version: " << LibVersion(requires.requiredAtlasVersion()).asString() 
                       <<  " VS Actual Atlas version "<< offers.offeredAtlasVersion() << std::endl;
    
    // Check Plume version
    if (LibVersion(requires.requiredPlumeVersion()) > LibVersion(plume_VERSION)) {
        return false;
    }

    // Check Atlas version
    if (LibVersion(requires.requiredAtlasVersion()) > LibVersion( offers.offeredAtlasVersion() )) {        
        return false;
    }    

    // Check requested parameters
    eckit::Log::info() << "Requesting Parameters: [";
    for (int i = 0; i < requires.requiredParamNames().size(); i++) {
        if (i)
            eckit::Log::info() << ", ";
        eckit::Log::info() << requires.requiredParamNames()[i];
    }
    eckit::Log::info() << "]" << std::endl;

    for (const auto& f : requires.requiredParamNames()) {

        eckit::Log::info() << " - Considering Parameter: " << f << std::endl;

        if (!offers.isParamOffered(f)) {
            eckit::Log::warning() << "Parameter " << f << " not found!" << std::endl;
            return false;
        }
    }
    return true;
};


std::vector<std::string> Manager::getActiveParams() {
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
    data::DataChecker::checkAllParams(data, PluginRegistry::instance().getDataCatalogue(), plume::data::CheckPolicyWarning{});

    eckit::Log::info() << "--- Plume manager has checked data." << std::endl;
}


}  // namespace plume
