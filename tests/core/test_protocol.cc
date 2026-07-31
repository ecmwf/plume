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

#include "eckit/testing/Test.h"
#include "eckit/config/YAMLConfiguration.h"
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


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}