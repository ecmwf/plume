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
#include "plugin_hooks.h"

#include "eckit/log/Log.h"


namespace plugin_hooks {

REGISTER_LIBRARY(PluginHooks)

PluginHooks::PluginHooks() : Plugin("PluginHooks"){};

const PluginHooks& PluginHooks::instance() {
    static PluginHooks instance;
    return instance;
}
//--------------------------------------------------------------


// PluginCoreHooks
static plume::PluginCoreBuilder<PluginCoreHooks> runnable_plugincore_HooksBuilder_;

PluginCoreHooks::PluginCoreHooks(const eckit::Configuration& conf) : PluginCore(conf) {}


void PluginCoreHooks::logAccess(const std::string& name) {
    // isWritable() answers for the hook point being run: the same parameter can be writable at
    // one hook point and read-only at another one.
    eckit::Log::info() << "   - " << name << ": " << (modelData().isWritable(name) ? "writable" : "read-only")
                       << std::endl;
}


void PluginCoreHooks::run() {

    // A plugin bound to several hook points has a single run() method: Plume calls it once for each
    // Manager::run(hook) call of the model, and currentHook() tells which hook point it is being run at.
    const std::string& hook = currentHook();

    eckit::Log::info() << "Plugin Hooks running at hook point '" << hook << "', access to parameters:" << std::endl;
    logAccess("step");
    logAccess("forcing");
    logAccess("temperature");

    // "step" is read-only at both hook points
    const int step = modelData().getParam<int>("step");

    if (hook == "pre-physics") {

        // Before the physics: "temperature" is read-only here, "forcing" is writable.
        const double temperature = modelData().getParam<double>("temperature");

        // Write "forcing" back to the model. The value lands directly in the memory of the model,
        // which is then expected to acknowledge the write-back (see example4.cc / example4.F90).
        const double forcing = 0.1 * step;
        modelData().writeParam<double>("forcing", forcing);

        eckit::Log::info() << " ---> step " << step << ": read temperature=" << temperature
                           << ", wrote forcing=" << forcing << std::endl;

        // Writing "temperature" at this hook point would throw: the plugin has been granted write
        // access to it at "post-physics" only.
        //   modelData().writeParam<double>("temperature", 0.0);  // eckit::BadValue
    }
    else if (hook == "post-physics") {

        // After the physics: the roles are swapped, "forcing" is read-only and "temperature" is writable.
        const double forcing     = modelData().getParam<double>("forcing");
        const double temperature = modelData().getParam<double>("temperature");

        // Apply a correction to the temperature computed by the model
        const double corrected = temperature - 0.05;
        modelData().writeParam<double>("temperature", corrected);

        eckit::Log::info() << " ---> step " << step << ": read forcing=" << forcing << ", temperature=" << temperature
                           << ", wrote temperature=" << corrected << std::endl;
    }
}

//--------------------------------------------------------------


}  // namespace plugin_hooks
