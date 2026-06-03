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

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/memory/NonCopyable.h"
#include "eckit/system/LibraryManager.h"

#include "plume/ManagerConfig.h"
#include "plume/Plugin.h"
#include "plume/PluginDecision.h"
#include "plume/coupling/WriteAuthorisation.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ModelData.h"


namespace plume {

namespace test {
struct ManagerTestAccess; // forward declaration
}  // namespace test

/**
 * @brief Manages the loading and running of plugins
 * 
 */
class Manager : public eckit::system::LibraryManager {

public:

    /**
     * @brief configure the manager
     * 
     * @param config 
     */
    static void configure(const eckit::Configuration& config);

    /**
     * @brief Negotiate with all candidate plugins listed in the manager configuration.
     *
     * For each plugin (in config order):
     *   - Loads the plugin and retrieves its parameter requirements.
     *   - Delegates per-plugin checks (version compatibility, parameter availability,
     *     write-back policy gate) and cross-plugin conflict detection
     *     (write-write, write-read, write-derived-read) to a Negotiator instance
     *     constructed with the write-back policy from the manager config.
     *   - Activates the plugin if the Negotiator accepts it.
     *
     * Emits a post-negotiation parameter-claim summary via Negotiator::logSummary().
     *
     * @param offers  Protocol describing the parameters offered by the model.
     */
    static void negotiate(const Protocol& offers);

    /**
     * @brief Let each plugin take its own share of data
     * 
     * @param data 
     */
    static void feedPlugins(data::ModelData& data);

    /**
     * @brief run all active plugins
     * 
     */
    static void run();

    /**
     * @brief teardown all active plugins
     * 
     */
    static void teardown();

    /**
     * @brief check if a plugin is activated
     * 
     * @param name 
     * @return true 
     * @return false 
     */
    static bool isPluginActivated(const std::string& name);

    /**
     * @brief Ordered list of active plugin names, in the order they will run.
     *
     * @return std::vector<std::string>
     */
    static std::vector<std::string> getActivePluginNames();

    /**
     * @brief List of Active Params
     * 
     * @return data::ParamList 
     */
    static std::unordered_set<std::string> getActiveParams();

    /**
     * @brief subset of Data Catalogue for active params
     * 
     * @return data::DataCatalogue 
     */
    static data::ParameterCatalogue getActiveDataCatalogue();

    /**
     * @brief has a param been requested by active plugins?
     * 
     * @param name 
     * @return true 
     * @return false 
     */
    static bool isParamRequested(const std::string& name);

    static bool isConfigured();

    /**
     * @brief Write authorisation table produced by the last negotiate() call.
     *
     * Maps each accepted plugin name to the set of parameter names it has been granted write access to.
     * Only plugins that explicitly requested writable access appear; read-only requesters are excluded.
     * This table is used by Plume to enforce write-back policy at runtime.
     *
     * Returns a const reference — callers may query but not modify the authorisations.
     * Must be called after negotiate() has completed.
     */
    static const WriteAuthorisation& writeAuthorisation();

private:

    /**
     * @brief Load a plugin from a shared library
     * 
     * @param lib 
     * @param name 
     * @return Plugin& 
     */
    static Plugin& loadPlugin(const std::string& lib, const std::string& name);

    /**
     * @brief Check data before feeding plugins
     * 
     * @param data 
     */
    static void checkData(const data::ModelData& data);

    /**
     * @brief Reset the manager configuration, this method is only intended for use within tests.
     */
    static void reset();

    static std::optional<ManagerConfig> managerConfig_;

    static bool isConfigured_;

    static WriteAuthorisation writeAuthorisation_;

    friend struct test::ManagerTestAccess;

};

}  // namespace plume