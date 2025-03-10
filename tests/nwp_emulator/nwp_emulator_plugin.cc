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
#include "nwp_emulator_plugin.h"
#include <iostream>


namespace nwp_emulator_test_plugin {

REGISTER_LIBRARY(NWPEmulatorPlugin)

NWPEmulatorPlugin::NWPEmulatorPlugin() : Plugin("NWPEmulatorPlugin"){};

NWPEmulatorPlugin::~NWPEmulatorPlugin(){};

const NWPEmulatorPlugin& NWPEmulatorPlugin::instance() {
    static NWPEmulatorPlugin instance;
    return instance;
}
//--------------------------------------------------------------


// NWPEmulatorPluginCore
static plume::PluginCoreBuilder<NWPEmulatorPluginCore> runnable_plugincore_FooBuilder_;

NWPEmulatorPluginCore::NWPEmulatorPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

NWPEmulatorPluginCore::~NWPEmulatorPluginCore() {}

void NWPEmulatorPluginCore::run() {
    eckit::Log::info() << "Consuming parameters " << modelData().getAtlasFieldShared("100u").name() << ", "
                       << modelData().getAtlasFieldShared("u").name() << ", "
                       << modelData().getAtlasFieldShared("v").name() << std::endl;
}

//--------------------------------------------------------------


}  // namespace nwp_emulator_test_plugin
