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
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"
#include "eckit/runtime/Main.h"
#include "eckit/utils/StringTools.h"

#include "plume/Manager.h"
#include "plume/Negotiator.h"
#include "plume/PluginConfig.h"
#include "plume/PluginCore.h"
#include "plume/PluginHandler.h"
#include "plume/Protocol.h"
#include "plume/data/DataChecker.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/plume.h"
#include "plume/utils.h"


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

    void reset() {
        pluginHandlers_.clear();
        dataCatalogue_ = data::ParameterCatalogue();
        hookIndex_.clear();
        // the default hook point is always registered, so that Manager::run() stays valid even
        // before a negotiation has taken place
        registeredHooks_ = {DEFAULT_HOOK};
    }

    void setActive(Plugin& plugin, const PluginConfig& pconfig, const PluginDecision& decision) {

        std::string name = plugin.plugincoreName();

        // create a plugin handler
        PluginHandler pluginHandle(plugin, pconfig, decision);

        // instantiate the plugincore (the plugin handler takes ownership of it)
        pluginHandle.activate(
            std::unique_ptr<PluginCore>(plume::PluginCoreFactory::instance().build(name, pconfig.coreConfig())));

        // plugin added to the active plugin list
        PluginRegistry::instance().pluginHandlers_.push_back(std::move(pluginHandle));

        // index the plugin by each of the hook points it has been accepted for. Indices, not
        // pointers: PluginHandler is move-only and the vector reallocates as it grows.
        std::size_t pluginIdx = PluginRegistry::instance().pluginHandlers_.size() - 1;
        for (const auto& hook : decision.agreedHooks()) {
            PluginRegistry::instance().hookIndex_[hook].push_back(pluginIdx);
        }
    }

    // get the active Plugins
    std::vector<PluginHandler>& getActivePlugins() { return pluginHandlers_; }

    // register the hook points offered by the model
    void setRegisteredHooks(const std::set<std::string>& hooks) {
        registeredHooks_.insert(hooks.begin(), hooks.end());
    }

    const std::set<std::string>& getRegisteredHooks() const { return registeredHooks_; }

    bool isHookRegistered(const std::string& hook) const {
        return registeredHooks_.find(hook) != registeredHooks_.end();
    }

    void checkHookRegistered(const std::string& hook) const {
        if (!isHookRegistered(hook)) {
            throw eckit::BadValue("Hook point " + hook + " has not been registered by the model!", Here());
        }
    }

    // indices of the active plugins bound to a hook point (empty if none)
    const std::vector<std::size_t>& getPluginsAtHook(const std::string& hook) const {
        static const std::vector<std::size_t> noPlugins;
        auto it = hookIndex_.find(hook);
        return (it == hookIndex_.end()) ? noPlugins : it->second;
    }

    // Parameters requested by the active plugins bound to a hook point
    std::unordered_set<std::string> getActiveParamsAtHook(const std::string& hook, bool derived = true) {
        checkHookRegistered(hook);
        std::unordered_set<std::string> requiredParams;
        for (const auto& pluginIdx : getPluginsAtHook(hook)) {
            auto req_fields = pluginHandlers_[pluginIdx].getRequiredParamNames(derived);
            requiredParams.insert(req_fields.begin(), req_fields.end());
        }
        return requiredParams;
    }


    // Parameters requested by all active plugins collectively
    std::unordered_set<std::string> getActiveParams(bool derived = true) {
        std::unordered_set<std::string> requiredParams;
        for (const auto& pluginHandle : pluginHandlers_) {
            auto req_fields = pluginHandle.getRequiredParamNames(derived);
            requiredParams.insert(req_fields.begin(), req_fields.end());
        }
        return requiredParams;
    }

    data::ParameterCatalogue getActiveDataCatalogue(bool derived = true) {
        return dataCatalogue_.filter(getActiveParams(derived));
    }

    void setDataCatalogue(const data::ParameterCatalogue& dataCatalogue) { dataCatalogue_ = dataCatalogue; }

    const data::ParameterCatalogue& getDataCatalogue() { return dataCatalogue_; }

private:
    // List of active plugins
    std::vector<PluginHandler> pluginHandlers_;

    // stores a copy of the data catalogue that
    // resulted in the activated plugins
    data::ParameterCatalogue dataCatalogue_;

    // hook point -> indices of the active plugins bound to it
    std::map<std::string, std::vector<std::size_t>> hookIndex_;

    // hook points registered by the model (the default one is always registered)
    std::set<std::string> registeredHooks_{DEFAULT_HOOK};
};
// -------------------------------------------------------------------


std::optional<ManagerConfig> Manager::managerConfig_;

bool Manager::isConfigured_{false};


void Manager::configure(const eckit::Configuration& config) {
    if (!Manager::isConfigured_) {
        managerConfig_         = ManagerConfig(config);
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

    auto pnames = offers.offeredParamNames();
    std::vector<std::string> names(pnames.begin(), pnames.end());
    eckit::Log::info() << "Plume config: " << *managerConfig_ << ", offers: " << names << std::endl;

    // Register the hook points offered by the model
    auto hnames = offers.offeredHookNames();
    std::vector<std::string> hooks(hnames.begin(), hnames.end());
    eckit::Log::info() << "Offered hook points: " << hooks << std::endl;
    PluginRegistry::instance().setRegisteredHooks(hnames);

    // Negotiate with each plugin
    Negotiator negotiator;

    // Load all selected plugins as per configuration
    for (const auto& pconfig : managerConfig_.value().plugins()) {

        auto name = pconfig.name();
        auto lib  = pconfig.lib();

        eckit::Log::info() << std::endl << " <== Evaluating Plugin: " << name << " from Library: " << lib << std::endl;

        // Load the plugin
        Plugin& plugin = loadPlugin(lib, name);

        // check what each plugin requires
        Protocol requires = plugin.negotiate();

        // Check plugin parameters requested through configuration (if any)
        auto config_params = pconfig.parameters();
        if (config_params.size() > 0) {
            eckit::Log::info() << "Parameters from Config: " << config_params << std::endl;
        }
        else {
            eckit::Log::info() << "No additional parameters found in Config." << std::endl;
        }

        // Check hook points requested through configuration (if any). When the key is present it
        // replaces the hook points declared by the plugin itself.
        auto config_hooks = pconfig.hooks();
        if (config_hooks.has_value()) {
            std::vector<std::string> chooks(config_hooks->begin(), config_hooks->end());
            eckit::Log::info() << "Hook points from Config: " << chooks << std::endl;
        }

        // negotiator handles the negotiation
        PluginDecision decision = negotiator.negotiate(offers, requires, config_params, config_hooks);
        eckit::Log::info() << decision << std::endl;

        // If the plugin is accepted, set it as active
        if (decision.accepted()) {
            PluginRegistry::instance().setActive(plugin, pconfig, decision);
        }
    }

    PluginRegistry::instance().setDataCatalogue(offers.offers());

    // Report which plugins ended up bound to which hook point. A model that adopts hook points and
    // stops calling the hook-less Manager::run() leaves everything bound to the default hook point
    // dormant, so this summary is worth having in the log.
    eckit::Log::info() << std::endl << "--- Plume hook point summary ---" << std::endl;
    for (const auto& hook : PluginRegistry::instance().getRegisteredHooks()) {
        std::vector<std::string> boundPlugins;
        for (const auto& pluginIdx : PluginRegistry::instance().getPluginsAtHook(hook)) {
            boundPlugins.push_back(PluginRegistry::instance().getActivePlugins()[pluginIdx].pluginName());
        }
        eckit::Log::info() << " - Hook point '" << hook << "': " << boundPlugins << std::endl;
    }
};


// Let each plugin take its own share of data (pointers)
void Manager::feedPlugins(data::ModelData& data) {

    // check data
    Manager::checkData(data);

    // Run each PluginCore for every active plugin
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        // Create derived fields if requested
        // Will do nothing if a previous plugin has already triggered the parameter creation
        for (const auto& requestedParam : pluginHandler.getRequiredParams()) {
            if (!requestedParam.strategy().empty()) {
                data.dispatchCreateParam(requestedParam.strategy(), requestedParam.config());
            }
        }

        // get the share of run data needed to run the plugincore
        auto requiredParams          = pluginHandler.getRequiredParamNames();
        data::ModelData requiredData = data.filter(requiredParams);

        // grab data
        pluginHandler.grabData(requiredData);

        // setup
        pluginHandler.setup();
    }
}


// Run the active plugincores bound to the default hook point
void Manager::run() {
    Manager::run(DEFAULT_HOOK);
};


// Run the active plugincores bound to a specific hook point
void Manager::run(const std::string& hook) {

    // an unregistered hook point is a mistake on the model side: fail loudly rather than
    // silently running nothing
    PluginRegistry::instance().checkHookRegistered(hook);

    auto& pluginHandlers = PluginRegistry::instance().getActivePlugins();
    for (const auto& pluginIdx : PluginRegistry::instance().getPluginsAtHook(hook)) {
        pluginHandlers[pluginIdx].run(hook);
    }
};


std::set<std::string> Manager::registeredHooks() {
    return PluginRegistry::instance().getRegisteredHooks();
}


bool Manager::isHookActive(const std::string& hook) {
    PluginRegistry::instance().checkHookRegistered(hook);
    return !PluginRegistry::instance().getPluginsAtHook(hook).empty();
}


// Teardown all active plugins
void Manager::teardown() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        // teardown the plugincore first
        pluginHandler.teardown();
    }
};

bool Manager::isPluginActivated(const std::string& name) {
    auto& pluginHandlers = PluginRegistry::instance().getActivePlugins();
    for (const auto& pluginHandler : pluginHandlers) {
        if (pluginHandler.pluginName() == name) {
            return true;
        }
    }
    return false;
}


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
    }
    else {
        return false;
    }
}


std::unordered_set<std::string> Manager::getActiveParamsAtHook(const std::string& hook, bool derived) {
    return PluginRegistry::instance().getActiveParamsAtHook(hook, derived);
}


bool Manager::isParamRequestedAtHook(const std::string& name, const std::string& hook) {
    auto activeParams = Manager::getActiveParamsAtHook(hook);
    return activeParams.find(name) != activeParams.end();
}


bool Manager::isConfigured() {
    return Manager::isConfigured_;
}


void Manager::checkData(const data::ModelData& data) {

    eckit::Log::info() << "--- Plume manager is checking data ..." << std::endl;

    // Check all requested params (regardless of whether they are "always-available" or "on-demand")
    // Skip all derived params as they are not yet created
    data::DataChecker::checkAllParams(data, PluginRegistry::instance().getActiveDataCatalogue(false),
                                      plume::data::CheckPolicyWarning{});

    // Check that all the "always" params are present
    data::DataChecker::checkAlwaysAvailParams(data, PluginRegistry::instance().getDataCatalogue(),
                                              plume::data::CheckPolicyWarning{});

    eckit::Log::info() << "--- Plume manager has checked data." << std::endl;
}

void Manager::reset() {
    PluginRegistry::instance().reset();
    isConfigured_ = false;
    managerConfig_.reset();
}


}  // namespace plume
