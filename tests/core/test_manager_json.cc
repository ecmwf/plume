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

#include "plume/Manager.h"
#include "plume/data/ParameterCatalogue.h"


using namespace eckit::testing;

namespace plume::test {


CASE("test_valid_manager_configuration_json") {

    // simple plugin loaded
    std::string mgr_conf_str = R"JSON(
    {
      "plugins": [
        {
          "lib": "simple_plugin",
          "name": "SimplePlugin",
          "core-config": {}
        }
      ]
    }
    )JSON";

    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    // protocol from config
    std::string data_conf_str = R"JSON(
    {
      "offered": [
        {
          "name": "I",
          "type": "INT",
          "available": "always",
          "comment": "none-1"
        },
        {
          "name": "J",
          "type": "INT",
          "available": "always",
          "comment": "none-2"
        },
        {
          "name": "K",
          "type": "INT",
          "available": "always",
          "comment": "none-3"
        }
      ]
    }
    )JSON";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    plume::Manager::configure(mgr_cfg);
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    // negotiate
    plume::Manager::negotiate(data_cfg);

    // Full list of loaded libs (including eckit, fckit, etc..)
    EXPECT_NOT_EQUAL( plume::Manager::list().size(), 0);

    // active params
    std::unordered_set<std::string> paramList = plume::Manager::getActiveParams();
    EXPECT_EQUAL(paramList.size(), 3);
    EXPECT(paramList.find("I") != paramList.end());
    EXPECT(paramList.find("J") != paramList.end());
    EXPECT(paramList.find("K") != paramList.end());

    // check active data catalogue
    plume::data::ParameterCatalogue catalogue = plume::Manager::getActiveDataCatalogue();

    std::vector<std::string> params = catalogue.getParamNames();
    EXPECT_EQUAL(params.size(), 3);

    // check that the params are in the catalogue
    EXPECT(catalogue.hasParam("I"));
    EXPECT(catalogue.hasParam("J"));
    EXPECT(catalogue.hasParam("K"));

    // search each param in the params vector
    EXPECT( std::find(params.begin(), params.end(), "I") != params.end() );
    EXPECT( std::find(params.begin(), params.end(), "J") != params.end() );
    EXPECT( std::find(params.begin(), params.end(), "K") != params.end() );

    // check that invalid are NOT in the catalogue
    EXPECT( std::find(params.begin(), params.end(), "not-a-param") == params.end() );

}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}