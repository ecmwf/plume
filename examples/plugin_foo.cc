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
#include "plugin_foo.h"
#include <iostream>


namespace plugin_foo {

REGISTER_LIBRARY(PluginFoo)

PluginFoo::PluginFoo() : Plugin("PluginFoo"){};

const PluginFoo& PluginFoo::instance() {
    static PluginFoo instance;
    return instance;
}
//--------------------------------------------------------------


// PluginCoreFoo
static plume::PluginCoreBuilder<PluginCoreFoo> runnable_plugincore_FooBuilder_;

PluginCoreFoo::PluginCoreFoo(const eckit::Configuration& conf) : PluginCore(conf) {}

void PluginCoreFoo::run() {

    eckit::Log::info() << "Plugin Foo running..." << std::endl;
    eckit::Log::info() << " ---> data contains parameters: " << std::endl;
    modelData().print();

    eckit::Log::info() << "Plugin Foo consuming parameters: (" 
                       << "I=" << modelData().getInt("I") << ", "
                       << "J=" << modelData().getInt("J") << ") "
                       << std::endl;
}

//--------------------------------------------------------------


}  // namespace foo_plugin
