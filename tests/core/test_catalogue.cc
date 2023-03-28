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
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;

namespace eckit {
namespace test {

CASE("test catalogue") {

    std::string jsonstr(
        "{"
        "  \"params\": ["
        "     {\"name\":\"param-1\", \"type\":\"INT\", \"available\": \"always\", \"comment\":\"none\"},"
        "     {\"name\":\"param-2\", \"type\":\"INT\", \"available\": \"always\", \"comment\":\"none\"},"
        "     {\"name\":\"param-3\", \"type\":\"INT\", \"available\": \"always\", \"comment\":\"none\"}"
        "  ]"
        "}"
        "\n");

    YAMLConfiguration conf(jsonstr);

    // a data catalogue
    plume::data::ParameterCatalogue catalogue(conf);

    // check all the params are there
    EXPECT_EQUAL(catalogue.hasParam("param-1"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-2"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-3"), true);

    // Filter only a subset of the catalogue
    plume::data::ParameterCatalogue filtered = catalogue.filter(std::vector<std::string>{"param-1", "param-2"});    

    // check the filtered catalogue
    EXPECT_EQUAL(filtered.hasParam("param-1"), true);
    EXPECT_EQUAL(filtered.hasParam("param-2"), true);
    EXPECT_EQUAL(filtered.hasParam("not-a-param"), false);

    // insert new param
    YAMLConfiguration param_999_config(std::string(
    "{"
      "\"name\": \"param-999\","
      "\"type\": \"INT\","
      "\"available\": \"always\","
      "\"comment\": \"none\""
    "}"));
    filtered.insertParam( plume::data::Parameter{param_999_config} );
    EXPECT_EQUAL(catalogue.hasParam("param-1"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-2"), true);
    EXPECT_EQUAL(filtered.hasParam("param-999"), true);
    EXPECT_EQUAL(filtered.hasParam("not-a-param"), false);

    // check that original catalogue doesn't have the new param
    EXPECT_EQUAL(catalogue.hasParam("param-999"), false);

}

CASE("test empty catalogue + insert") {

    // insert params to empty catalogue
    plume::data::ParameterCatalogue catalogue2;

    YAMLConfiguration param_11_config(std::string(
    "{"
      "\"name\": \"param-11\","
      "\"type\": \"INT\","
      "\"available\": \"always\","
      "\"comment\": \"none\""
    "}"));

    YAMLConfiguration param_22_config(std::string(
     "{"
      "\"name\": \"param-22\","
      "\"type\": \"INT\","
      "\"available\": \"always\","
      "\"comment\": \"none\""
    "}"));

    YAMLConfiguration param_33_config(std::string(
    "{"
      "\"name\": \"param-33\","
      "\"type\": \"INT\","
      "\"available\": \"always\","
      "\"comment\": \"none\""
    "}"));       

    catalogue2.insertParam( plume::data::Parameter{param_11_config} );
    catalogue2.insertParam( plume::data::Parameter{param_22_config} );
    catalogue2.insertParam( plume::data::Parameter{param_33_config} );

    EXPECT_EQUAL(catalogue2.hasParam("param-11"), true);
    EXPECT_EQUAL(catalogue2.hasParam("param-22"), true);
    EXPECT_EQUAL(catalogue2.hasParam("param-33"), true);
    EXPECT_EQUAL(catalogue2.hasParam("not-a-param"), false);

    // sequentially insert params to empty catalogue
    plume::data::ParameterCatalogue catalogue3;
    catalogue3.insertParam(param_11_config).insertParam(param_22_config).insertParam( param_33_config);

    EXPECT_EQUAL(catalogue3.hasParam("param-11"), true);
    EXPECT_EQUAL(catalogue3.hasParam("param-22"), true);
    EXPECT_EQUAL(catalogue3.hasParam("param-33"), true);    
    EXPECT_EQUAL(catalogue3.hasParam("not-a-param"), false);
}
//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}