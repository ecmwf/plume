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
#include <string>
#include "plume/PluginCore.h"
#include "plume/Plugin.h"

namespace plugin_bar {


/**
 * @brief PluginCore simple example
 * 
 */
class PluginCoreBar final : public plume::PluginCore {
public:
    PluginCoreBar(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "plugincore-bar"; }
};


/**
 * @brief Plugin simple example
 * 
 */
class PluginBar final : public plume::Plugin {

public:
    PluginBar();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireInt("K");
        protocol.requireAtlasField("field_dummy_1");
        return protocol;
    }

    static const PluginBar& instance();

    std::string version() const override { return "0.0.1"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return PluginCoreBar::type(); }
};


}  // namespace plugin_bar
