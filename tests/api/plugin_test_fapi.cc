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
#include "plugin_test_fapi.h"
#include <iostream>


namespace plugin_fapi {

REGISTER_LIBRARY(InterfacePlugin_fapi)

InterfacePlugin_fapi::InterfacePlugin_fapi() : Plugin(InterfacePlugin_fapi::name()) {}

const InterfacePlugin_fapi& InterfacePlugin_fapi::instance() {
    static InterfacePlugin_fapi instance;
    return instance;
}
//--------------------------------------------------------------

// InterfacePluginCore_fapi
static plume::PluginCoreBuilder<InterfacePluginCore_fapi> plugincore_Interface_Builder_fapi;
InterfacePluginCore_fapi::InterfacePluginCore_fapi(const eckit::Configuration& conf) : PluginCore(conf), config_{conf} {}
//--------------------------------------------------------------


}  // namespace plugin_fapi
