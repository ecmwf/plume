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

#include "atlas/field/Field.h"

namespace plume_example_plugin {
// ------ Simple plugin which requests a derived field -------
class SimpleDerivedPluginCore final : public plume::PluginCore {
public:
    SimpleDerivedPluginCore(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "simple-derived-plugincore"; }
};
// ------------------------------------------------------

// ------------------------------------------------------
class SimpleDerivedPlugin final : public plume::Plugin {

public:
    SimpleDerivedPlugin();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.require<atlas::Field>("u", {{"height", "100"}});

        return protocol;
    }

    // Return the static instance
    static const SimpleDerivedPlugin& instance();

    std::string version() const override { return "0.0.1-SimpleDerived"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return SimpleDerivedPluginCore::type(); }
};
// ------------------------------------------------------
}  // namespace plume_example_plugin