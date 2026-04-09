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


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}