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
#include "simple_plugin.h"
#include <iostream>


namespace plume_example_plugin {

REGISTER_LIBRARY(SimplePlugin)

SimplePlugin::SimplePlugin() : Plugin("SimplePlugin"){};

const SimplePlugin& SimplePlugin::instance() {
    static SimplePlugin instance;
    return instance;
}
//--------------------------------------------------------------


// SimplePluginCore
static plume::PluginCoreBuilder<SimplePluginCore> runnable_plugincore_FooBuilder_;

SimplePluginCore::SimplePluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

void SimplePluginCore::run() {
    eckit::Log::info() << "Consuming parameters: (" 
                       << "I=" << modelData().getInt("I") << ", "
                       << "J=" << modelData().getInt("J") << ", "
                       << "K=" << modelData().getInt("K") << ") "
                       << std::endl;
}

//--------------------------------------------------------------


}  // namespace foo_plugin
