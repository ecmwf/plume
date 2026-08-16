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

REGISTER_LIBRARY(HookPluginDefault)

HookPluginDefault::HookPluginDefault() : Plugin("HookPluginDefault"){};

const HookPluginDefault& HookPluginDefault::instance() {
    static HookPluginDefault instance;
    return instance;
}
//--------------------------------------------------------------


static plume::PluginCoreBuilder<HookPluginCoreDefault> hook_plugincore_Default_Builder_;

HookPluginCoreDefault::HookPluginCoreDefault(const eckit::Configuration& conf) : PluginCore(conf) {}

void HookPluginCoreDefault::run() {
    eckit::Log::info() << "HookPluginCoreDefault running at hook point: " << currentHook() << std::endl;
    bumpCounter(modelData(), "count-D");
}

}  // namespace plume_hook_plugin
