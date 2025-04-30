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
#include "eckit/config/YAMLConfiguration.h"
#include "plume/Protocol.h"


using namespace eckit::testing;

namespace eckit {
namespace test {

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
    YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    EXPECT_EQUAL(protocol.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol.requiredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol.requiredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol.requiredParamNames()[2], "param-3");

    EXPECT_THROWS(protocol.requiredParamNames().at(3));
    EXPECT_THROWS(protocol.requiredParamNames().at(4));

    // copy constructor
    plume::Protocol protocol2(protocol);
    EXPECT_EQUAL(protocol2.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol2.requiredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol2.requiredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol2.requiredParamNames()[2], "param-3");
    EXPECT_THROWS(protocol2.requiredParamNames().at(4));

    // assignment operator
    plume::Protocol protocol3;
    protocol3 = protocol2;
    EXPECT_EQUAL(protocol3.requiredParamNames().size(), 3);
    EXPECT_EQUAL(protocol3.requiredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol3.requiredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol3.requiredParamNames()[2], "param-3");
    EXPECT_THROWS(protocol3.requiredParamNames().at(4));    

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
    YAMLConfiguration conf(cfgtxt);
    plume::Protocol protocol(conf);

    EXPECT_EQUAL(protocol.requiredParamNames().size(), 0);
    EXPECT_THROWS(protocol.requiredParamNames().at(1));
    EXPECT_THROWS(protocol.requiredParamNames().at(2));

    EXPECT_EQUAL(protocol.offeredParamNames().size(), 3);  
    EXPECT_EQUAL(protocol.offeredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol.offeredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol.offeredParamNames()[2], "param-3");

    // copy constructor
    plume::Protocol protocol2(protocol);
    EXPECT_EQUAL(protocol2.offeredParamNames().size(), 3);
    EXPECT_EQUAL(protocol2.offeredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol2.offeredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol2.offeredParamNames()[2], "param-3");
    EXPECT_THROWS(protocol2.offeredParamNames().at(4));

    // assignment operator
    plume::Protocol protocol3;
    protocol3 = protocol2;
    EXPECT_EQUAL(protocol3.offeredParamNames().size(), 3);
    EXPECT_EQUAL(protocol3.offeredParamNames()[0], "param-1");
    EXPECT_EQUAL(protocol3.offeredParamNames()[1], "param-2");
    EXPECT_EQUAL(protocol3.offeredParamNames()[2], "param-3");
    EXPECT_THROWS(protocol3.offeredParamNames().at(4));    

}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}