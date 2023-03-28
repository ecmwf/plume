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

SimplePlugin::~SimplePlugin(){};

const SimplePlugin& SimplePlugin::instance() {
    static SimplePlugin instance;
    return instance;
}
//--------------------------------------------------------------


// SimpleKernel
static plume::KernelBuilder<SimpleKernel> runnable_kernel_FooBuilder_;

SimpleKernel::SimpleKernel(const eckit::Configuration& conf) : Kernel(conf) {}

SimpleKernel::~SimpleKernel() {}

void SimpleKernel::run() {
    eckit::Log::info() << "Consuming parameters: (" 
                       << "I=" << modelData().getInt("I") << ", "
                       << "J=" << modelData().getInt("J") << ", "
                       << "K=" << modelData().getInt("K") << ") "
                       << std::endl;
}

//--------------------------------------------------------------


}  // namespace foo_plugin
