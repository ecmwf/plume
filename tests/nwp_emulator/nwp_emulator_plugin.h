/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <string>
#include "plume/Plugin.h"
#include "plume/PluginCore.h"

namespace nwp_emulator_test_plugin {


// ------ Foo runnable plugincore that self-registers! -------
class NWPEmulatorPluginCore : public plume::PluginCore {
public:
    NWPEmulatorPluginCore(const eckit::Configuration& conf);
    ~NWPEmulatorPluginCore();
    void run() override;
    constexpr static const char* type() { return "nwpemulator-plugincore"; }
};
// ------------------------------------------------------

// ------------------------------------------------------
class NWPEmulatorPlugin : public plume::Plugin {

public:
    NWPEmulatorPlugin();
    ~NWPEmulatorPlugin();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireAtlasField("100u");
        protocol.requireAtlasField("u");
        protocol.requireAtlasField("v");

        return protocol;
    }

    // Return the static instance
    static const NWPEmulatorPlugin& instance();

    std::string version() const override { return "0.0.1-NWPEmulator"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return NWPEmulatorPluginCore::type(); }
};
// ------------------------------------------------------

}  // namespace nwp_emulator_test_plugin
