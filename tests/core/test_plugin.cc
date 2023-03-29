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
#include "eckit/testing/Test.h"
#include "plume/Plugin.h"


using namespace eckit::testing;

namespace eckit {
namespace test {

class DummyPlugin : public plume::Plugin {
public:
    DummyPlugin(const std::string& name, const std::string& lib=0) : plume::Plugin(name, lib){}
    virtual plume::Protocol negotiate() override {return plume::Protocol();};
    std::string version() const override { return "version-0.0.1"; }
    std::string gitsha1(unsigned int count) const override { return "dummy_sha"; }
    virtual std::string plugincoreName() const override {return "dummy-plugincore";};
};

CASE("test plugin 1") {

    DummyPlugin plugin("dummy_plugin", "dummy_plugin_lib");
    std::string name = plugin.name();
    std::string version = plugin.version();
    std::string sha = plugin.gitsha1(0);
    std::string plugincoreName = plugin.plugincoreName();

    EXPECT_EQUAL(name,    "dummy_plugin");
    EXPECT_EQUAL(version, "version-0.0.1");
    EXPECT_EQUAL(sha,     "dummy_sha");
    EXPECT_EQUAL(plugincoreName, "dummy-plugincore");

    EXPECT_NO_THROW(plugin.init());
    EXPECT_NO_THROW(plugin.finalise());

}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}