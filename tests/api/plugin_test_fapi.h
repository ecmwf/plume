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
#include <iostream>
#include <string>

#include "eckit/utils/StringTools.h"
#include "plume/Plugin.h"
#include "plume/PluginCore.h"


// plugincore
extern "C" void plugincore_setup_fapi(void* config, void* modelData);
extern "C" void plugincore_run_fapi(void* config, void* modelData);
extern "C" void plugincore_teardown_fapi(void* config, void* modelData);


namespace plugin_fapi {


class InterfacePluginCore_fapi : public plume::PluginCore {
public:
    InterfacePluginCore_fapi(const eckit::Configuration& conf);

    virtual void setup() override {
        plugincore_setup_fapi(&config_, &modelData()); 
    }

    void run() override {
        plugincore_run_fapi(&config_, &modelData()); 
    }

    virtual void teardown() override {
        plugincore_teardown_fapi(&config_, &modelData()); 
    }

    static const char* type() { return "fapi"; }
private:
    eckit::LocalConfiguration config_;
};
// ------------------------------------------------------

// ------------------------------------------------------
class InterfacePlugin_fapi : public plume::Plugin {

public:
    InterfacePlugin_fapi();

    // Name set in the Fortran interface
    static const char* name() { return "PluginTestFAPI"; }

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        return protocol;
    }

    // Return the static instance
    static const InterfacePlugin_fapi& instance();
    std::string version() const override { return "0.1.0"; }
    std::string gitsha1(unsigned int count) const override { return "N/A"; }
    virtual std::string plugincoreName() const override { return InterfacePluginCore_fapi::type(); }
};
// ------------------------------------------------------

}  // namespace plugin_fapi
