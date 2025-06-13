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

namespace plume_test_api {


// ------ Foo runnable plugincore that self-registers! -------
class PluginTestAPICore final : public plume::PluginCore {
public:
    PluginTestAPICore(const eckit::Configuration& conf);
    void run() override;
    constexpr static const char* type() { return "plugintest-plugincore"; }
};
// ------------------------------------------------------

// ------------------------------------------------------
class PluginTestAPI final : public plume::Plugin {

public:
    PluginTestAPI();

    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        return protocol;
    }

    // Return the static instance
    static const PluginTestAPI& instance();

    std::string version() const override { return "0.0.1"; }

    std::string gitsha1(unsigned int count) const override { return "undefined"; }

    virtual std::string plugincoreName() const override { return PluginTestAPICore::type(); }
};
// ------------------------------------------------------

}  // namespace plume_test_api
