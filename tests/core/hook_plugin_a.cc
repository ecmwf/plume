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

void bumpCounter(plume::data::ModelData& data, const std::string& name) {
    if (!data.hasParameter(name)) {
        eckit::Log::warning() << "Hook fixture: counter " << name << " not available!" << std::endl;
        return;
    }
    data.updateParam<int>(name, data.getParam<int>(name) + 1);
}


REGISTER_LIBRARY(HookPluginA)

HookPluginA::HookPluginA() : Plugin("HookPluginA"){};

const HookPluginA& HookPluginA::instance() {
    static HookPluginA instance;
    return instance;
}
//--------------------------------------------------------------


static plume::PluginCoreBuilder<HookPluginCoreA> hook_plugincore_A_Builder_;

HookPluginCoreA::HookPluginCoreA(const eckit::Configuration& conf) : PluginCore(conf) {}

void HookPluginCoreA::run() {
    eckit::Log::info() << "HookPluginCoreA running at hook point: " << currentHook() << std::endl;
    bumpCounter(modelData(), "count-A");
}

}  // namespace plume_hook_plugin
