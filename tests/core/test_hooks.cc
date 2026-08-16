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
#include <unordered_set>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

#include "ManagerTestAccess.h"
#include "plume/Hook.h"
#include "plume/Manager.h"
#include "plume/data/ModelData.h"


using namespace eckit::testing;

namespace plume::test {

namespace {

// Three plugins:
//  - HookPluginA       bound to "hook-A"
//  - HookPluginAB      bound to "hook-A" and "hook-B"
//  - HookPluginDefault declares no hook point, so it is bound to the default one
const std::string managerConfig = R"YAML(
plugins:
  - lib: hook_plugins
    name: HookPluginA
    core-config: {}
  - lib: hook_plugins
    name: HookPluginAB
    core-config: {}
  - lib: hook_plugins
    name: HookPluginDefault
    core-config: {}
)YAML";


plume::Protocol makeOffers() {
    plume::Protocol offers;
    offers.offer<int>("I", "always", "param I");
    offers.offer<int>("J", "always", "param J");
    offers.offer<int>("count-A", "always", "counter for HookPluginA");
    offers.offer<int>("count-AB-hook-A", "always", "counter for HookPluginAB at hook-A");
    offers.offer<int>("count-AB-hook-B", "always", "counter for HookPluginAB at hook-B");
    offers.offer<int>("count-D", "always", "counter for HookPluginDefault");

    offers.offerHook("hook-A", "the first hook point");
    offers.offerHook("hook-B", "the second hook point");
    return offers;
}


// model-owned data: it has to outlive Plume, hence the namespace scope
int param_i = 1;
int param_j = 2;


void feedCounters(plume::data::ModelData& data) {
    data.provideParam("I", &param_i);
    data.provideParam("J", &param_j);

    // Plume-owned counters: the plugincores and this test share the same underlying values
    data.createParam("count-A", 0);
    data.createParam("count-AB-hook-A", 0);
    data.createParam("count-AB-hook-B", 0);
    data.createParam("count-D", 0);
}

}  // namespace


CASE("test_hook_dispatch") {

    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());

    // before any negotiation, only the default hook point is registered and running it is a no-op
    EXPECT(plume::Manager::registeredHooks().count(plume::DEFAULT_HOOK) == 1);
    EXPECT_NO_THROW(plume::Manager::run());

    eckit::YAMLConfiguration mgr_cfg(managerConfig);
    plume::Manager::configure(mgr_cfg);
    EXPECT(plume::Manager::isConfigured());

    plume::Manager::negotiate(makeOffers());

    // all three plugins are accepted
    EXPECT(plume::Manager::isPluginActivated("HookPluginA"));
    EXPECT(plume::Manager::isPluginActivated("HookPluginAB"));
    EXPECT(plume::Manager::isPluginActivated("HookPluginDefault"));

    // registered hook points: the two offered ones plus the implicit default
    std::set<std::string> expectedHooks = {plume::DEFAULT_HOOK, "hook-A", "hook-B"};
    EXPECT_EQUAL(plume::Manager::registeredHooks(), expectedHooks);

    // all three hook points have something bound to them
    EXPECT(plume::Manager::isHookActive("hook-A"));
    EXPECT(plume::Manager::isHookActive("hook-B"));
    EXPECT(plume::Manager::isHookActive(plume::DEFAULT_HOOK));

    plume::data::ModelData data;
    feedCounters(data);
    plume::Manager::feedPlugins(data);

    // --- run "hook-A": HookPluginA and HookPluginAB only
    plume::Manager::run("hook-A");
    EXPECT_EQUAL(data.getParam<int>("count-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-B"), 0);
    EXPECT_EQUAL(data.getParam<int>("count-D"), 0);

    // --- run "hook-B": HookPluginAB only, and currentHook() must have followed the invocation
    plume::Manager::run("hook-B");
    EXPECT_EQUAL(data.getParam<int>("count-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-B"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-D"), 0);

    // --- run the default hook point: the plugin that declared no hook point only
    plume::Manager::run();
    EXPECT_EQUAL(data.getParam<int>("count-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-A"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-AB-hook-B"), 1);
    EXPECT_EQUAL(data.getParam<int>("count-D"), 1);

    // --- an unregistered hook point is an error, not a silent no-op
    EXPECT_THROWS(plume::Manager::run("not-a-hook"));
    EXPECT_THROWS(plume::Manager::isHookActive("not-a-hook"));

    plume::Manager::teardown();
}


CASE("test_params_per_hook") {

    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());

    eckit::YAMLConfiguration mgr_cfg(managerConfig);
    plume::Manager::configure(mgr_cfg);
    plume::Manager::negotiate(makeOffers());

    // params of the plugins bound to "hook-A": HookPluginA and HookPluginAB
    std::unordered_set<std::string> paramsAtA = plume::Manager::getActiveParamsAtHook("hook-A");
    std::set<std::string> expectedAtA        = {"I", "count-A", "count-AB-hook-A", "count-AB-hook-B"};
    EXPECT_EQUAL(std::set<std::string>(paramsAtA.begin(), paramsAtA.end()), expectedAtA);

    // params of the plugins bound to "hook-B": HookPluginAB only. A plugin bound to several hook
    // points contributes the same params to each of them.
    std::unordered_set<std::string> paramsAtB = plume::Manager::getActiveParamsAtHook("hook-B");
    std::set<std::string> expectedAtB        = {"I", "count-AB-hook-A", "count-AB-hook-B"};
    EXPECT_EQUAL(std::set<std::string>(paramsAtB.begin(), paramsAtB.end()), expectedAtB);

    // params of the plugin bound to the default hook point
    std::unordered_set<std::string> paramsAtDefault = plume::Manager::getActiveParamsAtHook(plume::DEFAULT_HOOK);
    std::set<std::string> expectedAtDefault        = {"J", "count-D"};
    EXPECT_EQUAL(std::set<std::string>(paramsAtDefault.begin(), paramsAtDefault.end()), expectedAtDefault);

    // the per-hook queries discriminate
    EXPECT(plume::Manager::isParamRequestedAtHook("count-A", "hook-A"));
    EXPECT_NOT(plume::Manager::isParamRequestedAtHook("count-A", "hook-B"));
    EXPECT_NOT(plume::Manager::isParamRequestedAtHook("J", "hook-A"));
    EXPECT(plume::Manager::isParamRequestedAtHook("J", plume::DEFAULT_HOOK));

    // the union of the per-hook sets is the global set
    std::set<std::string> unionOfHooks;
    for (const auto& hook : plume::Manager::registeredHooks()) {
        auto params = plume::Manager::getActiveParamsAtHook(hook);
        unionOfHooks.insert(params.begin(), params.end());
    }
    auto globalParams = plume::Manager::getActiveParams();
    EXPECT_EQUAL(unionOfHooks, std::set<std::string>(globalParams.begin(), globalParams.end()));

    // querying an unregistered hook point is an error
    EXPECT_THROWS(plume::Manager::getActiveParamsAtHook("not-a-hook"));
    EXPECT_THROWS(plume::Manager::isParamRequestedAtHook("I", "not-a-hook"));

    // none of these params is derived, so asking for the model-provided ones only changes nothing
    std::unordered_set<std::string> sourceAtA = plume::Manager::getActiveParamsAtHook("hook-A", false);
    EXPECT_EQUAL(std::set<std::string>(sourceAtA.begin(), sourceAtA.end()), expectedAtA);
}


CASE("test_hook_config_override") {

    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());

    // the configuration re-targets HookPluginA, which declares "hook-A", onto "hook-B"
    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: hook_plugins
        name: HookPluginA
        hooks: [hook-B]
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);
    plume::Manager::configure(mgr_cfg);
    plume::Manager::negotiate(makeOffers());

    EXPECT(plume::Manager::isPluginActivated("HookPluginA"));

    // the configured hook point replaces the declared one
    EXPECT_NOT(plume::Manager::isHookActive("hook-A"));
    EXPECT(plume::Manager::isHookActive("hook-B"));
    EXPECT_NOT(plume::Manager::isHookActive(plume::DEFAULT_HOOK));

    EXPECT(plume::Manager::isParamRequestedAtHook("count-A", "hook-B"));
    EXPECT_NOT(plume::Manager::isParamRequestedAtHook("count-A", "hook-A"));
}


CASE("test_plugin_rejected_when_hook_not_offered") {

    ManagerTestAccess::reset();
    EXPECT_NOT(plume::Manager::isConfigured());

    std::string mgr_conf_str = R"YAML(
    plugins:
      - lib: hook_plugins
        name: HookPluginAB
        core-config: {}
    )YAML";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);
    plume::Manager::configure(mgr_cfg);

    // HookPluginAB requires both "hook-A" and "hook-B", but only "hook-A" is offered: hook
    // requirements are all-or-nothing, so the plugin must be rejected
    plume::Protocol offers;
    offers.offer<int>("I", "always", "param I");
    offers.offer<int>("count-AB-hook-A", "always", "counter");
    offers.offer<int>("count-AB-hook-B", "always", "counter");
    offers.offerHook("hook-A", "only one of the two required hook points");

    plume::Manager::negotiate(offers);

    EXPECT_NOT(plume::Manager::isPluginActivated("HookPluginAB"));
    EXPECT_NOT(plume::Manager::isHookActive("hook-A"));
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
