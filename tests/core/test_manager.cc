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

namespace eckit {
namespace test {

CASE("test manager 1") {

    // simple plugin loaded
    std::string mgr_conf_str = R"YAML({"plugins": [{"lib": "simple_plugin", "name": "SimplePlugin"}]})YAML";
    eckit::YAMLConfiguration mgr_cfg(mgr_conf_str);

    // protocol from config
    std::string data_conf_str = R"YAML({
                                        "offered": [
                                            {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                            {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                            {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                       })YAML";
    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    plume::Manager::configure(mgr_cfg);

    // negotiate
    plume::Manager::negotiate(data_cfg);

    // Full list of loaded libs (including eckit, fckit, etc..)
    EXPECT_NOT_EQUAL( plume::Manager::list().size(), 0);

    // active params
    std::vector<std::string> paramList = plume::Manager::getActiveParams();
    EXPECT_EQUAL(paramList.size(), 3);
    EXPECT_EQUAL(paramList[0], "I");
    EXPECT_EQUAL(paramList[1], "J");
    EXPECT_EQUAL(paramList[2], "K");

    // check active data catalogue
    plume::data::ParameterCatalogue catalogue = plume::Manager::getActiveDataCatalogue();
    EXPECT_EQUAL(catalogue.getParamNames().size(), 3);
    EXPECT_EQUAL(catalogue.getParamNames()[0], "I");
    EXPECT_EQUAL(catalogue.getParamNames()[1], "J");
    EXPECT_EQUAL(catalogue.getParamNames()[2], "K");
}


//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}