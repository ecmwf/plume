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


#include "plume/Negotiator.h"
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;
using namespace plume;

namespace eckit {
namespace test {

CASE("test_negotiator ") {

    Negotiator negotiator;

    std::string offers_str = R"YAML({
                                    "offered": [
                                        {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                        {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                        {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                    })YAML";

    std::string requires_str = R"YAML({
                                     "required": [
                                         {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                         {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                         {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                     })YAML";

    std::string requires_not_fullfilled_str = R"YAML({
                                    "required": [
                                        {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                        {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                        {"name":"K", "type":"INT", "available": "always", "comment":"none-3"},
                                        {"name":"K_new", "type":"INT", "available": "always", "comment":"none-3"},
                                        {"name":"K_new2", "type":"INT", "available": "always", "comment":"none-3"}]
                                    })YAML";

    std::string requires_invalid_str = R"YAML({
                                        "required_invalid_key": [
                                            {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                            {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                            {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                        })YAML";                                    
   
    // negotiate                                        
    PluginDecision decision = negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_str));
    EXPECT_EQUAL(decision.accepted(), true);

    // requests not fullfilled
    PluginDecision decision_not_fullfilled = negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_not_fullfilled_str));
    EXPECT_EQUAL(decision_not_fullfilled.accepted(), false);

    // invalid key
    EXPECT_THROWS(negotiator.negotiate(eckit::YAMLConfiguration(offers_str), eckit::YAMLConfiguration(requires_invalid_str)));

}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}