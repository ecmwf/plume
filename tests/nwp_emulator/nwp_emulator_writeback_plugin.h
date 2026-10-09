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

#include "atlas/field/Field.h"

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

namespace nwp_emulator_test_plugin {

// ------ Runnable plugincore that writes back into the emulator's "u" field -------
class NWPEmulatorWriteBackPluginCore : public plume::PluginCore {
public:
    NWPEmulatorWriteBackPluginCore(const eckit::Configuration& conf);
    ~NWPEmulatorWriteBackPluginCore();
    void run() override;
    constexpr static const char* type() { return "nwpemulator-writeback-plugincore"; }
};
// ------------------------------------------------------

// ------------------------------------------------------
// Exercises the emulator's --writable option: negotiates write access to "u", which the emulator
// only offers as writable when it is passed on the CLI (or set via NWPEmulatorRunOptions).
class NWPEmulatorWriteBackPlugin : public plume::Plugin {

public:
    NWPEmulatorWriteBackPlugin();
    ~NWPEmulatorWriteBackPlugin();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireWritable<atlas::Field>("u");

        return protocol;
    }

    // Return the static instance
    static const NWPEmulatorWriteBackPlugin& instance();

    std::string version() const override { return "0.0.1-NWPEmulatorWriteBack"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return NWPEmulatorWriteBackPluginCore::type(); }
};
// ------------------------------------------------------

}  // namespace nwp_emulator_test_plugin
