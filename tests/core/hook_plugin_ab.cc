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
#include "hook_plugin.h"

#include "eckit/log/Log.h"


namespace plume_hook_plugin {

REGISTER_LIBRARY(HookPluginAB)

HookPluginAB::HookPluginAB() : Plugin("HookPluginAB"){};

const HookPluginAB& HookPluginAB::instance() {
    static HookPluginAB instance;
    return instance;
}
//--------------------------------------------------------------


static plume::PluginCoreBuilder<HookPluginCoreAB> hook_plugincore_AB_Builder_;

HookPluginCoreAB::HookPluginCoreAB(const eckit::Configuration& conf) : PluginCore(conf) {}

void HookPluginCoreAB::run() {
    eckit::Log::info() << "HookPluginCoreAB running at hook point: " << currentHook() << std::endl;

    // this plugin is bound to two hook points: it counts them separately, so that the test can
    // tell that currentHook() really does follow the invocation
    bumpCounter(modelData(), "count-AB-" + currentHook());
}

}  // namespace plume_hook_plugin
