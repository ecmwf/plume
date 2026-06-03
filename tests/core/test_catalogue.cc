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

namespace plume::test {

CASE("test catalogue") {

    std::string yamlstr(R"YAML(
    params:
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
    )YAML");

    eckit::YAMLConfiguration conf(yamlstr);

    // a data catalogue
    plume::data::ParameterCatalogue catalogue(conf);

    // check all the params are there
    EXPECT_EQUAL(catalogue.hasParam("param-1"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-2"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-3"), true);

    // Filter only a subset of the catalogue
    plume::data::ParameterCatalogue filtered = catalogue.filter(std::unordered_set<std::string>{"param-1", "param-2"});

    // check the filtered catalogue
    EXPECT_EQUAL(filtered.hasParam("param-1"), true);
    EXPECT_EQUAL(filtered.hasParam("param-2"), true);
    EXPECT_EQUAL(filtered.hasParam("not-a-param"), false);

    // insert new param
    eckit::YAMLConfiguration param_999_config(std::string(R"YAML(
    name: param-999
    type: INT
    available: always
    comment: none
    )YAML"));


    filtered.insertParam( plume::data::ParameterDefinition{param_999_config} );
    EXPECT_EQUAL(catalogue.hasParam("param-1"), true);
    EXPECT_EQUAL(catalogue.hasParam("param-2"), true);
    EXPECT_EQUAL(filtered.hasParam("param-999"), true);
    EXPECT_EQUAL(filtered.hasParam("not-a-param"), false);

    // check that original catalogue doesn't have the new param
    EXPECT_EQUAL(catalogue.hasParam("param-999"), false);

}

CASE("test writable parameter definition") {

    // constructors with explicit writable flag
    plume::data::ParameterDefinition writable_param("p", "INT", "always", "comment", /*writable=*/true);
    EXPECT_EQUAL(writable_param.writable(), true);

    plume::data::ParameterDefinition readonly_param("p", "INT", "always", "comment");
    EXPECT_EQUAL(readonly_param.writable(), false);

    // YAML config with writable: true
    eckit::YAMLConfiguration writable_config(std::string(R"YAML(
    name: param-w
    type: INT
    available: always
    comment: writable param
    writable: true
    )YAML"));
    plume::data::ParameterDefinition from_yaml_writable{writable_config};
    EXPECT_EQUAL(from_yaml_writable.writable(), true);

    // YAML config without writable key defaults to false
    eckit::YAMLConfiguration readonly_config(std::string(R"YAML(
    name: param-r
    type: INT
    available: always
    comment: read-only param
    )YAML"));
    plume::data::ParameterDefinition from_yaml_readonly{readonly_config};
    EXPECT_EQUAL(from_yaml_readonly.writable(), false);
}


CASE("test derived parameter cannot be writable") {

    // Requesting a derived param (with level options) as writable must throw at construction.
    eckit::YAMLConfiguration derived_writable_config(std::string(R"YAML(
    name: u
    type: ATLAS_FIELD
    height: 100
    writable: true
    )YAML"));

    EXPECT_THROWS(plume::data::ParameterDefinition bad{derived_writable_config});

    // A derived param without writable is fine.
    eckit::YAMLConfiguration derived_readonly_config(std::string(R"YAML(
    name: u
    type: ATLAS_FIELD
    height: 100
    )YAML"));

    plume::data::ParameterDefinition derived_readonly{derived_readonly_config};
    EXPECT_EQUAL(derived_readonly.writable(), false);
}


CASE("test empty catalogue + insert") {

    // insert params to empty catalogue
    plume::data::ParameterCatalogue catalogue2;

    eckit::YAMLConfiguration param_11_config(std::string(R"YAML(
    name: param-11
    type: INT
    available: always
    comment: none
    )YAML"));

    eckit::YAMLConfiguration param_22_config(std::string(R"YAML(
    name: param-22
    type: INT
    available: always
    comment: none
    )YAML"));

    eckit::YAMLConfiguration param_33_config(std::string(R"YAML(
    name: param-33
    type: INT
    available: always
    comment: none
    )YAML"));

    catalogue2.insertParam( plume::data::ParameterDefinition{param_11_config} );
    catalogue2.insertParam( plume::data::ParameterDefinition{param_22_config} );
    catalogue2.insertParam( plume::data::ParameterDefinition{param_33_config} );

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

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}