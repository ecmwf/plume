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


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}