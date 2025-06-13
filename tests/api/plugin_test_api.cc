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
#include <iostream>
#include "plugin_test_api.h"


namespace plume_test_api {

REGISTER_LIBRARY(PluginTestAPI)

PluginTestAPI::PluginTestAPI() : Plugin("PluginTestAPI"){};

const PluginTestAPI& PluginTestAPI::instance() {
    static PluginTestAPI instance;
    return instance;
}
//--------------------------------------------------------------


// PluginTestAPICore
static plume::PluginCoreBuilder<PluginTestAPICore> runnable_plugincore_FooBuilder_;

PluginTestAPICore::PluginTestAPICore(const eckit::Configuration& conf) : PluginCore(conf) {}

void PluginTestAPICore::run() {
    eckit::Log::info() << "Consuming parameters.. " << std::endl;
}

//--------------------------------------------------------------


}  // namespace plume_test_api
