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
#include "eckit/exception/Exceptions.h"
#include "plume/Hook.h"
#include "plume/Protocol.h"


using namespace eckit::testing;

namespace plume::test {

CASE("test protocol - required params") {

    std::string text = R"YAML(
    ---
    required:
      - name: param-1
        type: INT

      - name: param-2
        type: INT

      - name: param-3
        type: INT
    )YAML";

    std::string cfgtxt(text);
    eckit::YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    std::set<std::string> expected = {"param-1", "param-2", "param-3"};

    EXPECT_EQUAL(protocol.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol.requiredParamNames(), expected);

    // copy constructor
    plume::Protocol protocol2(protocol);
    EXPECT_EQUAL(protocol2.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol2.requiredParamNames(), expected);

    // assignment operator
    plume::Protocol protocol3;
    protocol3 = protocol2;
    EXPECT_EQUAL(protocol3.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol3.requiredParamNames(), expected);

}


CASE("test protocol - offered params") {

    std::string text = R"YAML(
    ---
    offered:
      - name: param-1
        type: INT
        available: always
        comment: none

      - name: param-2
        type: INT
        available: always
        comment: none

      - name: param-3
        type: INT
        available: always
        comment: none

    )YAML";

    std::string cfgtxt(text);
    eckit::YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    std::set<std::string> expected = {"param-1", "param-2", "param-3"};

    EXPECT_EQUAL(protocol.requiredParamNames().size(), 0);

    EXPECT_EQUAL(protocol.offeredParamNames().size(), 3);  
    EXPECT_EQUAL(protocol.offeredParamNames(), expected);

    // copy constructor
    plume::Protocol protocol2(protocol);
    EXPECT_EQUAL(protocol2.offeredParamNames().size(), 3);
    EXPECT_EQUAL(protocol2.offeredParamNames(), expected);

    // assignment operator
    plume::Protocol protocol3;
    protocol3 = protocol2;
    EXPECT_EQUAL(protocol3.offeredParamNames().size(), 3);
    EXPECT_EQUAL(protocol3.offeredParamNames(), expected);
}


CASE("test protocol - hook points") {

    plume::Protocol protocol;

    // the default hook point is always offered, even by a model that registers none
    EXPECT(protocol.isHookOffered(plume::DEFAULT_HOOK));
    EXPECT_EQUAL(protocol.offeredHookNames().size(), 1);
    EXPECT_EQUAL(protocol.requiredHooks().size(), 0);

    protocol.offerHook("hook-A", "the first hook point");
    protocol.offerHook("hook-B", "the second hook point");

    std::set<std::string> expectedOffered = {plume::DEFAULT_HOOK, "hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.offeredHookNames(), expectedOffered);
    EXPECT(protocol.isHookOffered("hook-A"));
    EXPECT_NOT(protocol.isHookOffered("hook-C"));

    // offering is idempotent: re-offering only updates the comment
    protocol.offerHook("hook-A", "an updated description");
    EXPECT_EQUAL(protocol.offeredHookNames().size(), 3);
    EXPECT_EQUAL(protocol.offeredHooks().at("hook-A"), std::string("an updated description"));

    // the default hook point may be re-offered too
    protocol.offerHook(plume::DEFAULT_HOOK, "end of my time step");
    EXPECT_EQUAL(protocol.offeredHookNames().size(), 3);

    // required hook points
    protocol.requireHook("hook-A");
    protocol.requireHook("hook-A");  // duplicates collapse
    protocol.requireHook("hook-B");
    std::set<std::string> expectedRequired = {"hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.requiredHooks(), expectedRequired);
    EXPECT(protocol.isHookRequired("hook-A"));
    EXPECT_NOT(protocol.isHookRequired("hook-C"));

    // copy constructor and assignment carry the hook points
    plume::Protocol protocol2(protocol);
    EXPECT_EQUAL(protocol2.offeredHookNames(), expectedOffered);
    EXPECT_EQUAL(protocol2.requiredHooks(), expectedRequired);

    plume::Protocol protocol3;
    protocol3 = protocol2;
    EXPECT_EQUAL(protocol3.offeredHookNames(), expectedOffered);
    EXPECT_EQUAL(protocol3.requiredHooks(), expectedRequired);
}


CASE("test protocol - hook points from config") {

    std::string text = R"YAML(
    ---
    required:
      - name: param-1
        type: INT
    requiredHooks: [hook-A, hook-B]
    offeredHooks:
      - name: hook-A
        comment: the first hook point
      - name: hook-B
    )YAML";

    std::string cfgtxt(text);
    eckit::YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    std::set<std::string> expectedRequired = {"hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.requiredHooks(), expectedRequired);

    // the default hook point is added on top of the configured ones
    std::set<std::string> expectedOffered = {plume::DEFAULT_HOOK, "hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.offeredHookNames(), expectedOffered);
    EXPECT_EQUAL(protocol.offeredHooks().at("hook-A"), std::string("the first hook point"));
    EXPECT_EQUAL(protocol.offeredHooks().at("hook-B"), std::string(""));
}


CASE("test protocol - hook-scoped params") {

    plume::Protocol protocol;
    protocol.offerHook("hook-A", "the first hook point");
    protocol.offerHook("hook-B", "the second hook point");

    // offers: unscoped means every offered hook point
    protocol.offer<int>("I", "always", "param I");
    protocol.offerAt<int>("J", "always", "param J", {"hook-B"});
    std::set<std::string> allHooks = {plume::DEFAULT_HOOK, "hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.offeredHooksFor("I"), allHooks);
    EXPECT_EQUAL(protocol.offeredHooksFor("J"), std::set<std::string>{"hook-B"});
    EXPECT(protocol.offeredHooksFor("not-offered").empty());

    // requirements: scopes of repeated requirements unite, an unscoped requirement wins
    protocol.requireAt<int>("K", {"hook-A"});
    protocol.requireAt<int>("K", {"hook-B"});
    std::set<std::string> expectedAB = {"hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.requires().getParam("K").hooks(), expectedAB);
    EXPECT_EQUAL(protocol.requires().getParams().size(), size_t(1));

    protocol.require<int>("K");
    EXPECT(protocol.requires().getParam("K").hooks().empty());

    // an unscoped requirement is not narrowed by a later scoped one
    protocol.require<int>("L");
    protocol.requireAt<int>("L", {"hook-A"});
    EXPECT(protocol.requires().getParam("L").hooks().empty());
}


CASE("test protocol - hook-scoped params from config") {

    std::string text = R"YAML(
    ---
    required:
      - name: param-1
        type: INT
        hooks: [hook-A]
      - name: param-1
        type: INT
        hooks: [hook-B]
    offered:
      - name: param-2
        type: INT
        hooks: [hook-B]
    offeredHooks:
      - name: hook-A
      - name: hook-B
    )YAML";

    std::string cfgtxt(text);
    eckit::YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    // the two definitions of param-1 are merged
    std::set<std::string> expectedAB = {"hook-A", "hook-B"};
    EXPECT_EQUAL(protocol.requires().getParam("param-1").hooks(), expectedAB);
    EXPECT_EQUAL(protocol.offeredHooksFor("param-2"), std::set<std::string>{"hook-B"});
}

CASE("test protocol - writable offered params") {

    plume::Protocol protocol;
    protocol.offer<int>("W1", "always", "writable param", /*writable=*/true);
    protocol.offer<int>("W2", "always", "read-only param");

    EXPECT_EQUAL(protocol.offeredParamNames().size(), 2);
    EXPECT_EQUAL(protocol.offers().getParam("W1").writable(), true);
    EXPECT_EQUAL(protocol.offers().getParam("W2").writable(), false);
}


CASE("test protocol - writable required params") {

    plume::Protocol protocol;
    protocol.require<int>("R1");
    protocol.requireWritable<int>("R2");

    EXPECT_EQUAL(protocol.requiredParamNames().size(), 2);
    EXPECT_EQUAL(protocol.requires().getParam("R1").writable(), false);
    EXPECT_EQUAL(protocol.requires().getParam("R2").writable(), true);
}



CASE("test protocol - per-hook access") {

    plume::Protocol protocol;

    // plugin side: P written at H1, read-only at H2
    protocol.requireWritableAt<int>("P", {"H1"});
    protocol.requireAt<int>("P", {"H2"});
    const auto& p = protocol.requires().getParam("P");
    EXPECT_EQUAL(p.hooks(), (std::set<std::string>{"H1", "H2"}));
    EXPECT(p.writable());
    EXPECT(p.isWritableAt("H1"));
    EXPECT_NOT(p.isWritableAt("H2"));
    EXPECT_EQUAL(protocol.requires().getParams().size(), size_t(1));

    // an empty scope is rejected
    EXPECT_THROWS_AS(protocol.requireWritableAt<int>("Q", {}), eckit::BadValue);

    // model side: P offered writable at H1, read-only at H2
    protocol.offerHook("H1");
    protocol.offerHook("H2");
    protocol.offerAt<int>("P", "always", "", {"H1"}, /*writable=*/true);
    protocol.offerAt<int>("P", "always", "", {"H2"});
    const auto& o = protocol.offers().getParam("P");
    EXPECT(o.isWritableAt("H1"));
    EXPECT_NOT(o.isWritableAt("H2"));
    EXPECT_EQUAL(protocol.offeredHooksFor("P"), (std::set<std::string>{"H1", "H2"}));

    EXPECT_THROWS_AS(protocol.offerAt<int>("R", "always", "", {}), eckit::BadValue);

    // develop's unscoped API still works: writable everywhere
    protocol.offer<int>("W", "always", "", /*writable=*/true);
    EXPECT(protocol.offers().getParam("W").isWritableAt("H1"));
    EXPECT(protocol.offers().getParam("W").isWritableAt("H2"));
}


CASE("test protocol - per-hook access from config") {

    std::string text = R"YAML(
    ---
    required:
      - name: P
        type: INT
        hooks: [H1, H2]
        writable: [H1]
      - name: Q
        type: INT
        hooks: [H1]
        writable: true
      - name: Q
        type: INT
        hooks: [H2]
    )YAML";

    eckit::YAMLConfiguration conf(text);
    plume::Protocol protocol(conf);

    for (const auto& name : {"P", "Q"}) {
        const auto& param = protocol.requires().getParam(name);
        EXPECT(param.isWritableAt("H1"));
        EXPECT_NOT(param.isWritableAt("H2"));
    }
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}