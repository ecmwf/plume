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
#include <set>
#include <string>

#include "eckit/testing/Test.h"
#include "eckit/config/YAMLConfiguration.h"

#include "plume/ManagerConfig.h"
#include "plume/PluginConfig.h"


using namespace eckit::testing;

namespace plume::test {

CASE("test_manager_configuration") {

    std::string mgr_configstr_valid = R"YAML(
    plugins:
      - name: simple_plugin
        lib: libsimple_plugin
        parameters:
          - name: param1
            type: INT
            available: always
            comment: none
      - name: simple_plugin2
        lib: libsimple_plugin2
        parameters:
          - name: param2
            type: INT
            available: always
            comment: none
    verbose: true
    )YAML";

    eckit::YAMLConfiguration config(mgr_configstr_valid);
    plume::ManagerConfig managerConfig(config);

    EXPECT_EQUAL(managerConfig.plugins().size(), 2);

    // test class PluginConfig
    plume::PluginConfig pluginConfig(managerConfig.plugins()[0]);

    EXPECT_EQUAL(pluginConfig.name(), "simple_plugin");
    EXPECT_EQUAL(pluginConfig.lib(), "libsimple_plugin");
    EXPECT_EQUAL(pluginConfig.parameters().size(), 1);

}

CASE("test_manager_configuration_invalid") {

    std::string missing_plugins = R"YAML(
    verbose: true
    )YAML";

    eckit::YAMLConfiguration config(missing_plugins);

    EXPECT_THROWS(plume::ManagerConfig managerConfig(config));

}


CASE("test_plugin_configuration") {

    std::string valid_config = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    parameters:
      - name: param1
        type: INT
        available: always
        comment: none
      - name: param2
        type: INT
        available: always
        comment: none
    )YAML";

    eckit::YAMLConfiguration config(valid_config);
    plume::PluginConfig pluginConfig(config);

    EXPECT_EQUAL(pluginConfig.name(), "simple_plugin");
    EXPECT_EQUAL(pluginConfig.lib(), "libsimple_plugin");
    EXPECT_EQUAL(pluginConfig.parameters().size(), 2);


    // check parameters one by one
    std::vector<eckit::LocalConfiguration> params = pluginConfig.parameters();
    EXPECT_EQUAL(params[0].getString("name"), "param1");
    EXPECT_EQUAL(params[0].getString("type"), "INT");
    EXPECT_EQUAL(params[0].getString("available"), "always");
    EXPECT_EQUAL(params[0].getString("comment"), "none");

    EXPECT_EQUAL(params[1].getString("name"), "param2");
    EXPECT_EQUAL(params[1].getString("type"), "INT");
    EXPECT_EQUAL(params[1].getString("available"), "always");
    EXPECT_EQUAL(params[1].getString("comment"), "none");
    

}


CASE("test_plugin_configuration_invalid") {

    std::string missing_name = R"YAML(
    lib: libsimple_plugin
    parameters:
      - name: param1
        type: INT
        available: always
        comment: none
      - name: param2
        type: INT
        available: always
        comment: none
    )YAML";

    eckit::YAMLConfiguration config(missing_name);
    EXPECT_THROWS(plume::PluginConfig pluginConfig(config));


    // missing lib
    std::string missing_lib = R"YAML(
    name: simple_plugin
    parameters:
      - name: param1
        type: INT
        available: always
        comment: none
      - name: param2
        type: INT
        available: always
        comment: none
    )YAML";

    eckit::YAMLConfiguration config2(missing_lib);
    EXPECT_THROWS(plume::PluginConfig pluginConfig2(config2));

}

CASE("test_plugin_configuration_hooks") {

    // no "hooks" key: the plugin's own declaration applies
    std::string no_hooks = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    )YAML";

    eckit::YAMLConfiguration config_no_hooks(no_hooks);
    plume::PluginConfig pluginConfigNoHooks(config_no_hooks);
    EXPECT_NOT(pluginConfigNoHooks.hooks().has_value());

    // "hooks" key present: it replaces whatever the plugin declared
    std::string with_hooks = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    hooks: [hook-A, hook-B]
    )YAML";

    eckit::YAMLConfiguration config_with_hooks(with_hooks);
    plume::PluginConfig pluginConfigWithHooks(config_with_hooks);
    EXPECT(pluginConfigWithHooks.hooks().has_value());

    std::set<std::string> expected = {"hook-A", "hook-B"};
    EXPECT_EQUAL(pluginConfigWithHooks.hooks().value(), expected);
}


CASE("test_plugin_configuration_hooks_invalid") {

    // an empty list is not the same as an absent key: it is a configuration mistake
    std::string empty_hooks = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    hooks: []
    )YAML";

    eckit::YAMLConfiguration config_empty(empty_hooks);
    EXPECT_THROWS(plume::PluginConfig pluginConfigEmpty(config_empty));
    EXPECT_NOT(plume::PluginConfig::isValid(config_empty));

    // not a list
    std::string scalar_hooks = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    hooks: hook-A
    )YAML";

    eckit::YAMLConfiguration config_scalar(scalar_hooks);
    EXPECT_THROWS(plume::PluginConfig pluginConfigScalar(config_scalar));
    EXPECT_NOT(plume::PluginConfig::isValid(config_scalar));

    // unknown keys are still rejected
    std::string unknown_key = R"YAML(
    name: simple_plugin
    lib: libsimple_plugin
    hooks: [hook-A]
    not-a-key: 1
    )YAML";

    eckit::YAMLConfiguration config_unknown(unknown_key);
    EXPECT_THROWS(plume::PluginConfig pluginConfigUnknown(config_unknown));
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}