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
#include "plugin_bar.h"
#include <iostream>


namespace plugin_bar {

REGISTER_LIBRARY(PluginBar)

PluginBar::PluginBar() : Plugin("PluginBar"){};

PluginBar::~PluginBar(){};

const PluginBar& PluginBar::instance() {
    static PluginBar instance;
    return instance;
}
//--------------------------------------------------------------


// KernelBar
static plume::KernelBuilder<KernelBar> runnable_kernel_BarBuilder_;

KernelBar::KernelBar(const eckit::Configuration& conf) : Kernel(conf) {}

KernelBar::~KernelBar() {}

void KernelBar::run() {
    eckit::Log::info() << "Plugin Bar consuming parameters: (" 
                       << "K=" << modelData().getInt("K") << ", "
                       << "field=" << modelData().getAtlasFieldShared("field_dummy_1") << ") "
                       << std::endl;
}

//--------------------------------------------------------------


}  // namespace plugin_bar
