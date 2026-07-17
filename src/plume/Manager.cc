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
#include <memory>
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
#include "plume/coupling/WriteBackLedger.h"
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
    }

    // get the active Plugins
    std::vector<PluginHandler>& getActivePlugins() { return pluginHandlers_; }


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
};
// -------------------------------------------------------------------


std::optional<ManagerConfig> Manager::managerConfig_;

bool Manager::isConfigured_{false};

WriteAuthorisation Manager::writeAuthorisation_;

std::unique_ptr<coupling::WriteBackLedger> Manager::writeBackLedger_;


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

    const WriteBackPolicy policy = managerConfig_.value().writeBackPolicy();
    eckit::Log::info() << "Plume Write-back policy: " << policy << std::endl;

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

        PluginDecision decision = negotiator.negotiate(name, offers, requires, config_params);
        eckit::Log::info() << decision << std::endl;

        if (decision.accepted()) {
            PluginRegistry::instance().setActive(plugin, pconfig, decision);
        }
    }

    negotiator.logSummary();

    writeAuthorisation_ = negotiator.writeAuthorisation();

    PluginRegistry::instance().setDataCatalogue(offers.offers());
};


// Let each plugin take its own share of data (pointers)
void Manager::feedPlugins(data::ModelData& data) {

    // check data
    Manager::checkData(data);

    // Initialise write-back ledger before feeding plugins so it's propagated into each plugin's filtered ModelData view
    if (!writeAuthorisation_.empty()) {
        writeBackLedger_ =
            std::make_unique<coupling::WriteBackLedger>(writeAuthorisation_, managerConfig_.value().writeBackPolicy());
        data.enrollWritebackParams(*writeBackLedger_, writeAuthorisation_);
        data.attachWritebackLedger(writeBackLedger_.get());
    }

    // PLUME-72: if a future refactor introduces plugin deactivation (e.g. setup() failure recovery,
    // runtime removal), ledger slots opened here for writable params would never be written.
    // At flush() they silently reset to IDLE, masking the missing write. A cross-check between
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
        auto requiredParams          = pluginHandler.getRequiredParamNames();
        data::ModelData requiredData = data.filter(requiredParams, pluginHandler.pluginName());

        // grab data
        pluginHandler.grabData(requiredData);

        // setup
        pluginHandler.setup();
    }
}


// Run all active plugincores
void Manager::run() {
    if (writeBackLedger_) {
        // reset() is safe on the first call (all slots are IDLE); on subsequent calls it transitions
        // CONFIRMED → IDLE, clearing acknowledgements from the previous cycle.
        writeBackLedger_->reset();
        writeBackLedger_->open();
    }

    // PLUME-72: getActivePlugins() returns all handlers without filtering by isActive(). If plugin
    // deactivation is introduced in a future refactor, open slots for inactive plugins would silently
    // reset at flush(). Filtering by isActive() before open() would be the fix.
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        pluginHandler.run();
    }

    if (writeBackLedger_) {
        writeBackLedger_->flush();  // STAGED → FLUSHED; READY → IDLE; throws on ERROR
    }
};


// Teardown all active plugins
void Manager::teardown() {
    for (auto& pluginHandler : PluginRegistry::instance().getActivePlugins()) {
        pluginHandler.teardown();
    }

    if (writeBackLedger_) {
        if (!writeBackLedger_->allConfirmed()) {
            eckit::Log::warning() << "Plume Manager::teardown(): write-back ledger has unconfirmed slots. "
                                  << "The model did not acknowledge all pending write-backs before teardown."
                                  << std::endl;
        }
        writeBackLedger_.reset();  // destructor fires onDetach_, nulling ModelData::ledger_
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
    if (writeBackLedger_) {
        writeBackLedger_.reset();  // destructor fires onDetach_, nulling ModelData::ledger_
    }
    PluginRegistry::instance().reset();
    isConfigured_ = false;
    managerConfig_.reset();
    writeAuthorisation_ = WriteAuthorisation{};
}


}  // namespace plume
