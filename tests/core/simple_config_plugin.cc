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
#include "simple_config_plugin.h"

#include <iostream>

namespace plume_example_plugin {

REGISTER_LIBRARY(SimpleConfigPlugin)

SimpleConfigPlugin::SimpleConfigPlugin() : Plugin("SimpleConfigPlugin") {}

const SimpleConfigPlugin& SimpleConfigPlugin::instance() {
    static SimpleConfigPlugin instance;
    return instance;
}

static plume::PluginCoreBuilder<SimpleConfigPluginCore> runnable_plugincore_simple_config_builder_;

SimpleConfigPluginCore::SimpleConfigPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {}

void SimpleConfigPluginCore::run() {
    std::cout << "[SimpleConfigPlugin] state=" << currentStateName()
              << " (parent=" << currentStateParent()
              << "), iter=" << currentStateIteration()
              << ", rel_iter=" << currentStateIterationRel() << std::endl;
}

}  // namespace plume_example_plugin
