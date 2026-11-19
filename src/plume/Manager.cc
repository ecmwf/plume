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
#include <array>
#include <cstdlib>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
#include "plume/coupling/WriteBackTracker.h"
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
        hookParams_.clear();
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

        // cache the params needed at each of those hook points (with and without the derived ones), so
        // that querying them from the model does not rebuild the sets at every call. A param restricted
        // to some hook points (hook scope) only counts at those. Plugins are only activated during
        // negotiation, so the cache is complete afterwards.
        for (const auto& hook : decision.agreedHooks()) {
            auto& cached = PluginRegistry::instance().hookParams_[hook];
            for (bool derived : {true, false}) {
                auto names = decision.offeredParamNamesAtHook(hook, derived);
                cached[derived].insert(names.begin(), names.end());
            }
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

    void requireHookRegistered(const std::string& hook) const {
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

    // Parameters requested by the active plugins bound to a hook point (cached at activation time).
    // A hook with plugins is necessarily registered, so the common case is a single lookup; the
    // registration check is only paid for hooks without any plugin bound.
    const std::unordered_set<std::string>& getActiveParamsAtHook(const std::string& hook, bool derived = true) const {
        static const std::unordered_set<std::string> noParams;
        auto it = hookParams_.find(hook);
        if (it != hookParams_.end()) {
            return it->second[derived];
        }
        requireHookRegistered(hook);
        return noParams;
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

    // This method is usually called by models to sort through their own data, not derived ones
    data::ParameterCatalogue getActiveDataCatalogue(bool derived = false) {
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

    // params requested at each hook point, indexed by [derived]: [false] = model-provided only
    std::unordered_map<std::string, std::array<std::unordered_set<std::string>, 2>> hookParams_;

    // hook points registered by the model (the default one is always registered)
    std::set<std::string> registeredHooks_{DEFAULT_HOOK};
};
// -------------------------------------------------------------------


std::optional<ManagerConfig> Manager::managerConfig_;

bool Manager::isConfigured_{false};

WriteAuthorisation Manager::writeAuthorisation_;

std::unique_ptr<coupling::WriteBackTracker> Manager::writeBackTracker_;


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

    // A param offered only at some hook points must name hook points the model registers: anything
    // else is a mistake on the model side. Checked here, not in Protocol::offer, because the model
    // is free to call offerHook() after offer().
    auto hnames = offers.offeredHookNames();
    for (const auto& param : offers.offers().getParams()) {
        for (const auto& hook : param.hooks()) {
            if (hnames.find(hook) == hnames.end()) {
                throw eckit::BadValue("Parameter " + param.name() + " is offered at hook point " + hook +
                                          ", which the model has not registered!",
                                      Here());
            }
        }
        for (const auto& hook : param.writableHooks()) {
            if (hnames.find(hook) == hnames.end()) {
                throw eckit::BadValue("Parameter " + param.name() + " is offered writable at hook point " + hook +
                                          ", which the model has not registered!",
                                      Here());
            }
        }
    }

    // Register the hook points offered by the model
    std::vector<std::string> hooks(hnames.begin(), hnames.end());
    eckit::Log::info() << "Offered hook points: " << hooks << std::endl;
    PluginRegistry::instance().setRegisteredHooks(hnames);

    const WriteBackPolicy policy = managerConfig_.value().writeBackPolicy();
    eckit::Log::info() << "Plume Write-back policy: " << policy << std::endl;

    // Negotiate with each plugin
    Negotiator negotiator(policy);

    for (const auto& pconfig : managerConfig_.value().plugins()) {

        auto name = pconfig.name();
        auto lib  = pconfig.lib();

        eckit::Log::info() << std::endl << " <== Evaluating Plugin: " << name << " from Library: " << lib << std::endl;

        Plugin& plugin    = loadPlugin(lib, name);
        Protocol requires = plugin.negotiate();

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
        PluginDecision decision = negotiator.negotiate(name, offers, requires, config_params, config_hooks);
        eckit::Log::info() << decision << std::endl;

        if (decision.accepted()) {
            PluginRegistry::instance().setActive(plugin, pconfig, decision);
        }
    }

    negotiator.logSummary();

    writeAuthorisation_ = negotiator.writeAuthorisation();

    PluginRegistry::instance().setDataCatalogue(offers.offers());

    // Report which plugins ended up bound to which hook point. A model that adopts hook points and
    // stops calling the hook-less Manager::run() leaves everything bound to the default hook point
    // dormant, so this summary is worth having in the log.
    eckit::Log::info() << "\n--- Plume hook point summary ---\n";
    for (const auto& hook : PluginRegistry::instance().getRegisteredHooks()) {
        std::vector<std::string> boundPlugins;
        for (const auto& pluginIdx : PluginRegistry::instance().getPluginsAtHook(hook)) {
            boundPlugins.push_back(PluginRegistry::instance().getActivePlugins()[pluginIdx].pluginName());
        }
        eckit::Log::info() << " - Hook point '" << hook << "': " << boundPlugins << "\n";
    }
    eckit::Log::info() << std::endl;  // single flush for the whole summary
};


// Let each plugin take its own share of data (pointers)
void Manager::feedPlugins(data::ModelData& data) {

    // check data
    Manager::checkData(data);

    // Initialise write-back tracker before feeding plugins so it's propagated into each plugin's filtered ModelData view
    if (!writeAuthorisation_.empty()) {
        writeBackTracker_ =
            std::make_unique<coupling::WriteBackTracker>(writeAuthorisation_, managerConfig_.value().writeBackPolicy());
        data.enrollWritebackParams(*writeBackTracker_, writeAuthorisation_);
        data.attachWritebackTracker(writeBackTracker_.get());
    }

    // PLUME-72: if a future refactor introduces plugin deactivation (e.g. setup() failure recovery,
    // runtime removal), tracker slots opened here for writable params would never be written.
    // At submit() they silently reset to IDLE, masking the missing write. A cross-check between
    // writeAuthorisation_ and the active plugin list at this point would catch this.

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
        auto requiredParams              = pluginHandler.getRequiredParamNames();
        data::ModelDataView requiredData = data.filter(requiredParams, pluginHandler.pluginName());

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
    PluginRegistry::instance().requireHookRegistered(hook);

    // One write-back cycle per hook point call: only the slots of the params writable at this hook
    // point are opened (ACKNOWLEDGED → IDLE → READY). open() throws if one of them is still PENDING,
    // i.e. the model has not acknowledged its previous write.
    if (writeBackTracker_) {
        warnPendingReadAt(hook);
        writeBackTracker_->open(hook);
    }

    // PLUME-72: getActivePlugins() returns all handlers without filtering by isActive(). If plugin
    // deactivation is introduced in a future refactor, open slots for inactive plugins would silently
    // reset at submit(). Filtering by isActive() before open() would be the fix.
    auto& pluginHandlers = PluginRegistry::instance().getActivePlugins();
    for (const auto& pluginIdx : PluginRegistry::instance().getPluginsAtHook(hook)) {
        pluginHandlers[pluginIdx].run(hook);
    }

    if (writeBackTracker_) {
        writeBackTracker_->submit();  // opened slots: WRITTEN → PENDING; READY → IDLE; throws on ERROR
    }
};


// A param written back at a previous hook point and not yet acknowledged by the model, which is
// read at this hook point: if the model refreshes it before ingesting the write, the write is lost.
void Manager::warnPendingReadAt(const std::string& hook) {
    const auto& activeParams = PluginRegistry::instance().getActiveParamsAtHook(hook);
    for (const auto& param : writeBackTracker_->pendingWritebacks()) {
        if (activeParams.find(param) != activeParams.end()) {
            eckit::Log::warning() << "Plume Manager::run(\"" << hook << "\"): param \"" << param
                                  << "\" is used at this hook point but its previous write-back has not been "
                                     "acknowledged by the model yet. If the model refreshed it in the meantime, the "
                                     "written value has been overwritten."
                                  << std::endl;
        }
    }
}


std::set<std::string> Manager::getWritableParamsAtHook(const std::string& hook) {
    PluginRegistry::instance().requireHookRegistered(hook);
    return writeAuthorisation_.writableParamsAt(hook);
}


std::set<std::string> Manager::registeredHooks() {
    return PluginRegistry::instance().getRegisteredHooks();
}


bool Manager::isHookActive(const std::string& hook) {
    PluginRegistry::instance().requireHookRegistered(hook);
    return !PluginRegistry::instance().getPluginsAtHook(hook).empty();
}


// Teardown all active plugins
void Manager::teardown() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        pluginHandler.teardown();
    }

    if (writeBackTracker_) {
        if (!writeBackTracker_->allAcknowledged()) {
            eckit::Log::warning() << "Plume Manager::teardown(): write-back tracker has unacknowledged slots. "
                                  << "The model did not acknowledge all pending write-backs before teardown."
                                  << std::endl;
        }
        writeBackTracker_.reset();  // destructor fires onDetach_, nulling ModelData::tracker_
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


std::vector<std::string> Manager::getActivePluginNames() {
    std::vector<std::string> names;
    for (const auto& handler : PluginRegistry::instance().getActivePlugins()) {
        names.push_back(handler.pluginName());
    }
    return names;
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
    // bind to the cached set: no copy on this (potentially hot) path
    const auto& activeParams = PluginRegistry::instance().getActiveParamsAtHook(hook);
    return activeParams.find(name) != activeParams.end();
}


bool Manager::isConfigured() {
    return Manager::isConfigured_;
}

const WriteAuthorisation& Manager::writeAuthorisation() {
    return writeAuthorisation_;
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
    if (writeBackTracker_) {
        writeBackTracker_.reset();  // destructor fires onDetach_, nulling ModelData::tracker_
    }
    PluginRegistry::instance().reset();
    isConfigured_ = false;
    managerConfig_.reset();
    writeAuthorisation_ = WriteAuthorisation{};
}


}  // namespace plume
