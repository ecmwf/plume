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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/memory/NonCopyable.h"
#include "eckit/system/LibraryManager.h"

#include "plume/Plugin.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ModelData.h"


namespace plume {

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
     * @brief Negotiate with Plugins
     * 
     * @param offers 
     * @return
     */
    static void negotiate(const Protocol& offers);

    /**
     * @brief Let each plugin take its own share of data
     * 
     * @param data 
     */
    static void feedPlugins(const data::ModelData& data);

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
     * @brief check if plugin exists
     * 
     * @param name 
     * @return true 
     * @return false 
     */
    static bool exists(const std::string& name);

    /**
     * @brief List of Active Params
     * 
     * @return data::ParamList 
     */
    static std::vector<std::string> getActiveParams();

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
private:

    /**
     * @brief Make a decision on each plugins, to establish which ones will run
     * 
     * @param dataConfig 
     * @param protocol 
     * @return true 
     * @return false 
     */
    static bool decideOnPlugin(const Protocol& offers, const Protocol& requires);

    /**
     * @brief Check data before feeding plugins
     * 
     * @param data 
     */
    static void checkData(const data::ModelData& data);

    static eckit::LocalConfiguration config_;
    static bool isConfigured_;

};

}  // namespace plume