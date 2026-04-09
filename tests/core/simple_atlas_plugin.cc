/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <iostream>

#include "simple_atlas_plugin.h"

namespace plume_example_plugin {

REGISTER_LIBRARY(SimpleDerivedPlugin)

SimpleDerivedPlugin::SimpleDerivedPlugin() : Plugin("SimpleDerivedPlugin") {};

const SimpleDerivedPlugin& SimpleDerivedPlugin::instance() {
    static SimpleDerivedPlugin instance;
    return instance;
}
//--------------------------------------------------------------


// SimpleDerivedPluginCore
static plume::PluginCoreBuilder<SimpleDerivedPluginCore> runnable_plugincore_SimpleDerivedBuilder_;

SimpleDerivedPluginCore::SimpleDerivedPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

void SimpleDerivedPluginCore::run() {
    eckit::Log::info() << "Consuming derived parameter: " << modelData().getParam<atlas::Field>("u", "100").name()
                       << std::endl;
}

//--------------------------------------------------------------


}  // namespace plume_example_plugin
