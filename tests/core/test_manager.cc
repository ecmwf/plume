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

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

#include "atlas/array.h"

#include "ManagerTestAccess.h"
#include "plume/Manager.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterType.h"


using namespace eckit::testing;

namespace plume::test {


CASE("test_invalid_manager_configuration") {

    // missing plugins
    std::string mgr_conf_str_plugins_missing = R"YAML({})YAML";
    eckit::YAMLConfiguration mgr_cfg_plugins_missing(mgr_conf_str_plugins_missing);

    // protocol from config
    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    EXPECT_THROWS(plume::Manager::configure(mgr_cfg_plugins_missing));
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);
}


CASE("test_valid_manager_configuration") {

    // simple plugin loaded
    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimplePlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    // protocol from config
    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    plume::Manager::configure(mgr_cfg);
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    // negotiate
    plume::Manager::negotiate(data_cfg);

    // Full list of loaded libs (including eckit, fckit, etc..)
    EXPECT_NOT_EQUAL(plume::Manager::list().size(), 0);

    // active params
    std::unordered_set<std::string> paramList = plume::Manager::getActiveParams();
    EXPECT_EQUAL(paramList.size(), 3);
    EXPECT(paramList.find("I") != paramList.end());
    EXPECT(paramList.find("J") != paramList.end());
    EXPECT(paramList.find("K") != paramList.end());

    // check active data catalogue
    plume::data::ParameterCatalogue catalogue = plume::Manager::getActiveDataCatalogue();

    std::set<std::string> params = catalogue.getParamNames();
    EXPECT_EQUAL(params.size(), 3);

    // check that the params are in the catalogue
    EXPECT(catalogue.hasParam("I"));
    EXPECT(catalogue.hasParam("J"));
    EXPECT(catalogue.hasParam("K"));

    // search each param in the params vector
    EXPECT(std::find(params.begin(), params.end(), "I") != params.end());
    EXPECT(std::find(params.begin(), params.end(), "J") != params.end());
    EXPECT(std::find(params.begin(), params.end(), "K") != params.end());

    // check that invalid are NOT in the catalogue
    EXPECT(std::find(params.begin(), params.end(), "not-a-param") == params.end());
}


CASE("test_feeding_plugins") {
    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());

    // simple plugin loaded
    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimpleDerivedPlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    // protocol from config
    std::string data_conf_str = R"YAML(
    offered:
      - name: u
        type: ATLAS_FIELD
        available: on-request
        comment: wind
      - name: z
        type: ATLAS_FIELD
        available: on-request
        comment: geopotential
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    plume::Manager::configure(mgr_cfg);
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    // negotiate
    plume::Manager::negotiate(data_cfg);

    std::set<std::string> expected = {"u", "u;hl;100", "z"};  // derived and its dependencies
    std::set<std::string> activeParams(plume::Manager::getActiveParams().begin(),
                                       plume::Manager::getActiveParams().end());
    EXPECT_EQUAL(activeParams, expected);

    // feed the plugin - will actually create the derived param
    atlas::Field u("u", atlas::array::make_datatype<float>(), atlas::array::make_shape(2, 2));
    atlas::Field z("z", atlas::array::make_datatype<float>(), atlas::array::make_shape(2, 2));

    auto u_view = atlas::array::make_view<float, 2>(u);
    auto z_view = atlas::array::make_view<float, 2>(z);
    for (size_t i = 0; i < 2; ++i) {
      u_view(i, 0) = 5.0f;
      u_view(i, 1) = 10.0f;
      // Ensure z_ = 100 * 9.80665 lies between the two levels.
      z_view(i, 0) = 1000.0f;
      z_view(i, 1) = 900.0f;
    }

    plume::data::ModelData data;
    data.provideParam("u", &u);
    data.provideParam("z", &z);

    EXPECT(data.hasParameter("u", plume::data::ParameterType::ATLAS_FIELD));
    EXPECT_NOT(data.hasParameter("u;hl;100"));
    EXPECT_NO_THROW(plume::Manager::feedPlugins(data));
    EXPECT(data.hasParameter("u;hl;100", plume::data::ParameterType::ATLAS_FIELD));
}


CASE("test_config_plugin_nested_run_smoke") {
    ManagerTestAccess::reset();

    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimpleConfigPlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    plume::Manager::configure(mgr_cfg);
    plume::Manager::negotiate(data_cfg);

    plume::data::ModelData data;
    data.createParam("I", 1);
    EXPECT_NO_THROW(plume::Manager::feedPlugins(data));

    for (int i = 0; i < 3; ++i) {
      EXPECT_NO_THROW(plume::Manager::run(plume::PlumeTag::RUN_LVL1));
        for (int j = 0; j < 4; ++j) {
        EXPECT_NO_THROW(plume::Manager::run(plume::PlumeTag::RUN_LVL2, plume::PlumeTag::RUN_LVL1));
            for (int k = 0; k < 2; ++k) {
          EXPECT_NO_THROW(plume::Manager::run(plume::PlumeTag::RUN_LVL3, plume::PlumeTag::RUN_LVL2));
            }
        }
    }

    EXPECT_NO_THROW(plume::Manager::teardown());
}


CASE("test_tagged_plume_state") {
    ManagerTestAccess::reset();

    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimplePlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    plume::Manager::configure(mgr_cfg);
    plume::Manager::negotiate(data_cfg);

    plume::data::ModelData data;
    data.createParam("I", 1);
    data.createParam("J", 2);
    data.createParam("K", 3);

    plume::Manager::feedPlugins(data);

    // run outer once
    plume::Manager::run(plume::PlumeTag::RUN_LVL1);

    [&]() {

      // run inner 5 times, and innermost 2 times
      for (int i = 0; i < 5; ++i) {
          plume::Manager::run(plume::PlumeTag::RUN_LVL2);

          for (int j = 0; j < 2; ++j) {
            plume::Manager::run(plume::PlumeTag::RUN_LVL3, plume::PlumeTag::RUN_LVL2);

            for (int k = 0; k < 2; ++k) {
              plume::Manager::run(plume::PlumeTag::RUN_LVL4, plume::PlumeTag::RUN_LVL3);
            }

            for (int l = 0; l < 4; ++l) {
              plume::Manager::run(plume::PlumeTag::RUN_LVL5, plume::PlumeTag::RUN_LVL3);
              if (i == 2 && l == 1) {
                return;
              }
            }
          }        
      }
    } ();
    
    
    std::cout << "----------------------------------" << std::endl;
    std::cout << plume::Manager::state().getConfig() << std::endl;
    std::cout << std::endl;
    std::cout << plume::Manager::state() << std::endl;
    std::cout << "----------------------------------" << std::endl;

    const auto state = plume::Manager::state().getConfig();
    auto tree        = state.getSubConfigurations("execution_tree");

    EXPECT_EQUAL(state.getString("current"), "run_lvl5");
    EXPECT_EQUAL(tree.size(), 5);
    EXPECT_EQUAL(tree[0].getString("name"), "configure");
    EXPECT_EQUAL(tree[0].getInt("iteration"), 1);
    EXPECT_EQUAL(tree[0].getInt("iteration_relative"), 1);
    EXPECT_EQUAL(tree[1].getString("name"), "negotiate");
    EXPECT_EQUAL(tree[1].getInt("iteration"), 1);
    EXPECT_EQUAL(tree[1].getInt("iteration_relative"), 1);
    EXPECT_EQUAL(tree[2].getString("name"), "feed_plugins");
    EXPECT_EQUAL(tree[2].getInt("iteration"), 1);
    EXPECT_EQUAL(tree[2].getInt("iteration_relative"), 1);
    EXPECT_EQUAL(tree[3].getString("name"), "run_lvl1");
    EXPECT_EQUAL(tree[3].getInt("iteration"), 1);
    EXPECT_EQUAL(tree[3].getInt("iteration_relative"), 1);
    EXPECT_EQUAL(tree[4].getString("name"), "run_lvl2");
    EXPECT_EQUAL(tree[4].getInt("iteration"), 3);
    EXPECT_EQUAL(tree[4].getInt("iteration_relative"), 3);

    auto innerChildren = tree[4].getSubConfigurations("children");
    EXPECT_EQUAL(innerChildren.size(), 1);
    EXPECT_EQUAL(innerChildren[0].getString("name"), "run_lvl3");
    EXPECT_EQUAL(innerChildren[0].getInt("iteration"), 5);
    EXPECT_EQUAL(innerChildren[0].getInt("iteration_relative"), 1);

    auto level11Children = innerChildren[0].getSubConfigurations("children");
    EXPECT_EQUAL(level11Children.size(), 2);
    EXPECT_EQUAL(level11Children[0].getString("name"), "run_lvl4");
    EXPECT_EQUAL(level11Children[0].getInt("iteration"), 10);
    EXPECT_EQUAL(level11Children[0].getInt("iteration_relative"), 2);
    EXPECT_EQUAL(level11Children[1].getString("name"), "run_lvl5");
    EXPECT_EQUAL(level11Children[1].getInt("iteration"), 18);
    EXPECT_EQUAL(level11Children[1].getInt("iteration_relative"), 2);

    auto currentNode = state.getSubConfiguration("current_node");
    EXPECT_EQUAL(currentNode.getString("name"), "run_lvl5");
    EXPECT_EQUAL(currentNode.getInt("iteration"), 18);
    EXPECT_EQUAL(currentNode.getInt("iteration_relative"), 2);

    plume::Manager::teardown();
    const auto finalState = plume::Manager::state().getConfig();
    auto finalTree        = finalState.getSubConfigurations("execution_tree");
    EXPECT_EQUAL(finalState.getString("current"), "teardown");

    EXPECT_EQUAL(finalTree.size(), 6);
    EXPECT_EQUAL(finalTree[5].getString("name"), "teardown");
    EXPECT_EQUAL(finalTree[5].getInt("iteration"), 1);
    EXPECT_EQUAL(finalTree[5].getInt("iteration_relative"), 1);

    auto finalCurrentNode = finalState.getSubConfiguration("current_node");
    EXPECT_EQUAL(finalCurrentNode.getString("name"), "teardown");
    EXPECT_EQUAL(finalCurrentNode.getInt("iteration"), 1);
    EXPECT_EQUAL(finalCurrentNode.getInt("iteration_relative"), 1);
}


CASE("test_manager_current_state_accessors") {
    ManagerTestAccess::reset();

    plume::Manager manager;
    EXPECT_EQUAL(manager.currentStateName(), "");
    EXPECT_EQUAL(manager.currentStateIteration(), 0);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 0);

    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimplePlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    plume::Manager::configure(mgr_cfg);
    EXPECT_EQUAL(manager.currentStateName(), "configure");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::Manager::negotiate(data_cfg);
    EXPECT_EQUAL(manager.currentStateName(), "negotiate");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::data::ModelData data;
    data.createParam("I", 1);
    data.createParam("J", 2);
    data.createParam("K", 3);

    plume::Manager::feedPlugins(data);
    EXPECT_EQUAL(manager.currentStateName(), "feed_plugins");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::Manager::run(plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl1");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::Manager::run(plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl1");
    EXPECT_EQUAL(manager.currentStateIteration(), 2);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 2);

    plume::Manager::run(plume::PlumeTag::RUN_LVL2, plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl2");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::Manager::run(plume::PlumeTag::RUN_LVL2, plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl2");
    EXPECT_EQUAL(manager.currentStateIteration(), 2);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 2);

    // Incrementing the parent starts a new epoch for its children relative iterations.
    plume::Manager::run(plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl1");
    EXPECT_EQUAL(manager.currentStateIteration(), 3);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 3);

    plume::Manager::run(plume::PlumeTag::RUN_LVL2, plume::PlumeTag::RUN_LVL1);
    EXPECT_EQUAL(manager.currentStateName(), "run_lvl2");
    EXPECT_EQUAL(manager.currentStateIteration(), 3);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);

    plume::Manager::teardown();
    EXPECT_EQUAL(manager.currentStateName(), "teardown");
    EXPECT_EQUAL(manager.currentStateIteration(), 1);
    EXPECT_EQUAL(manager.currentStateIterationRel(), 1);
}


CASE("test_manager_rejects_ambiguous_parent_tags") {
    ManagerTestAccess::reset();

    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: simple_plugins
        name: SimplePlugin
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    std::string data_conf_str = R"YAML(
    offered:
      - name: I
        type: INT
        available: always
        comment: none-1
      - name: J
        type: INT
        available: always
        comment: none-2
      - name: K
        type: INT
        available: always
        comment: none-3
    )YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    plume::Manager::configure(mgr_cfg);
    plume::Manager::negotiate(data_cfg);

    plume::data::ModelData data;
    data.createParam("I", 1);
    data.createParam("J", 2);
    data.createParam("K", 3);
    plume::Manager::feedPlugins(data);

    plume::Manager::run(plume::PlumeTag::RUN_LVL1);
    plume::Manager::run(plume::PlumeTag::RUN_LVL3, plume::PlumeTag::RUN_LVL1);
    plume::Manager::run(plume::PlumeTag::RUN_LVL2);
    plume::Manager::run(plume::PlumeTag::RUN_LVL3, plume::PlumeTag::RUN_LVL2);

    EXPECT_THROWS(plume::Manager::run(plume::PlumeTag::RUN_LVL4, plume::PlumeTag::RUN_LVL3));
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}