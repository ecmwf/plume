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
#pragma once

#include <string>

#include "plume/Plugin.h"
#include "plume/PluginCore.h"

namespace plume_example_plugin {

// PluginCore that relies on configuration-driven parameter selection only.
class SimpleConfigPluginCore final : public plume::PluginCore {
public:
    explicit SimpleConfigPluginCore(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "simple-config-plugincore"; }
};

class SimpleConfigPlugin final : public plume::Plugin {
public:
    SimpleConfigPlugin();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        // No hardcoded required parameters; selection is fully config-driven.
        return protocol;
    }

    static const SimpleConfigPlugin& instance();

    std::string version() const override { return "0.0.1-SimpleConfig"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    std::string plugincoreName() const override { return SimpleConfigPluginCore::type(); }
};

}  // namespace plume_example_plugin
