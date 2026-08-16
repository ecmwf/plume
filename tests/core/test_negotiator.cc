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
#include <optional>
#include <set>
#include <string>

#include "eckit/testing/Test.h"
#include "eckit/config/YAMLConfiguration.h"


#include "plume/Hook.h"
#include "plume/Negotiator.h"
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;
using namespace plume;

namespace plume::test {

CASE("test_negotiator ") {

    Negotiator negotiator;

    std::string offers_str = R"YAML(
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

    std::string requires_str = R"YAML(
    required:
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

    std::string requires_not_fullfilled_str = R"YAML(
    required:
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
      - name: K_new
        type: INT
        available: always
        comment: none-3
      - name: K_new2
        type: INT
        available: always
        comment: none-3
    )YAML";

    std::string requires_invalid_str = R"YAML(
    required_invalid_key:
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

    // negotiate                                        
    PluginDecision decision = negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_str));
    EXPECT_EQUAL(decision.accepted(), true);

    // requests not fullfilled
    PluginDecision decision_not_fullfilled = negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_not_fullfilled_str));
    EXPECT_EQUAL(decision_not_fullfilled.accepted(), false);

    // invalid key
    EXPECT_THROWS(negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_invalid_str)));

}


CASE("test_negotiator_hooks") {

    Negotiator negotiator;

    Protocol offers;
    offers.offer<int>("I", "always", "none-1");
    offers.offerHook("hook-A", "the first hook point");

    // 1) a plugin that requires no hook point is bound to the default one
    Protocol requiresNoHook;
    requiresNoHook.require<int>("I");
    PluginDecision decisionNoHook = negotiator.negotiate(offers, requiresNoHook);
    EXPECT_EQUAL(decisionNoHook.accepted(), true);
    EXPECT_EQUAL(decisionNoHook.agreedHooks(), std::set<std::string>{DEFAULT_HOOK});

    // 2) a plugin that requires an offered hook point is bound to it, and to it only
    Protocol requiresA;
    requiresA.require<int>("I");
    requiresA.requireHook("hook-A");
    PluginDecision decisionA = negotiator.negotiate(offers, requiresA);
    EXPECT_EQUAL(decisionA.accepted(), true);
    EXPECT_EQUAL(decisionA.agreedHooks(), std::set<std::string>{"hook-A"});

    // 3) hook requirements are all-or-nothing: "hook-B" is not offered, so the plugin is rejected
    //    even though "hook-A" is
    Protocol requiresAB;
    requiresAB.require<int>("I");
    requiresAB.requireHook("hook-A");
    requiresAB.requireHook("hook-B");
    PluginDecision decisionAB = negotiator.negotiate(offers, requiresAB);
    EXPECT_EQUAL(decisionAB.accepted(), false);

    // 4) the default hook point is always offered, even by a model that registers none
    Protocol bareOffers;
    bareOffers.offer<int>("I", "always", "none-1");
    Protocol requiresDefault;
    requiresDefault.require<int>("I");
    requiresDefault.requireHook(DEFAULT_HOOK);
    EXPECT_EQUAL(negotiator.negotiate(bareOffers, requiresDefault).accepted(), true);
}


CASE("test_negotiator_hooks_from_config") {

    Negotiator negotiator;

    Protocol offers;
    offers.offer<int>("I", "always", "none-1");
    offers.offerHook("hook-A", "the first hook point");
    offers.offerHook("hook-B", "the second hook point");

    Protocol requiresA;
    requiresA.require<int>("I");
    requiresA.requireHook("hook-A");

    // narrowing/re-targeting: the configured hook point replaces the declared one
    std::optional<std::set<std::string>> configB = std::set<std::string>{"hook-B"};
    PluginDecision decisionB = negotiator.negotiate(offers, requiresA, {}, configB);
    EXPECT_EQUAL(decisionB.accepted(), true);
    EXPECT_EQUAL(decisionB.agreedHooks(), std::set<std::string>{"hook-B"});

    // widening: a plugin that declares nothing can be bound to several hook points by config
    Protocol requiresNoHook;
    requiresNoHook.require<int>("I");
    std::optional<std::set<std::string>> configAB = std::set<std::string>{"hook-A", "hook-B"};
    PluginDecision decisionAB = negotiator.negotiate(offers, requiresNoHook, {}, configAB);
    EXPECT_EQUAL(decisionAB.accepted(), true);
    std::set<std::string> expectedAB = {"hook-A", "hook-B"};
    EXPECT_EQUAL(decisionAB.agreedHooks(), expectedAB);

    // a configured hook point that the model does not offer rejects the plugin
    std::optional<std::set<std::string>> configMissing = std::set<std::string>{"hook-C"};
    EXPECT_EQUAL(negotiator.negotiate(offers, requiresA, {}, configMissing).accepted(), false);
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}