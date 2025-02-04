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



CASE("test_invalid_plugin_configuration_parameters") {

    // invalid yaml
    std::string mgr_conf_str = R"YAML({"plugins": [{"lib": "simple_plugin", "name": "SimplePlugin", "parameters": 999 "core-config": {}}]})YAML";
    EXPECT_THROWS(eckit::YAMLConfiguration mgr_cfg_invalid_json(mgr_conf_str));

    // wrong "parameters" type
    std::string mgr_conf_str2 = R"YAML({"plugins": [{"lib": "simple_plugin", "name": "SimplePlugin", "parameters": 999, "core-config": {}}]})YAML";
    eckit::YAMLConfiguration mgr_cfg_invalid_param_type(mgr_conf_str2);

    // protocol from config
    std::string data_conf_str = R"YAML({
                                        "offered": [
                                            {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                            {"name":"J", "type":"INT", "available": "always", "comment":"none-2"},
                                            {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                        })YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);
    EXPECT_THROWS(plume::Manager::configure(mgr_cfg_invalid_param_type));
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);

}


CASE("test_valid_plugin_configuration_parameters") {

    // invalid yaml
    std::string mgr_conf_str = R"YAML({"plugins": [
                                            {
                                                "lib": "simple_plugin",
                                                "name": "SimplePlugin", 
                                                "parameters": [
                                                    [{"name":"I", "type":"INT"}, {"name":"J", "type":"INT"}],
                                                    [{"name":"JJJ", "type":"INT"}, {"name":"J", "type":"INT"}, {"name":"KKMM", "type":"INT"}],
                                                    [{"name":"XYZ", "type":"INT"}, {"name":"K", "type":"INT"}]
                                                ],
                                                "core-config": {}
                                            }
                                        ]
                                      })YAML";

    eckit::YAMLConfiguration mgr_cfg_valid_json(mgr_conf_str);

    // protocol from config
    std::string data_conf_str = R"YAML({
                                        "offered": [
                                            {"name":"I", "type":"INT", "available": "always", "comment":"none-1"},
                                            {"name":"J", "type":"INT", "available": "always", "comment":"none-1"},
                                            {"name":"JJJ", "type":"INT", "available": "always", "comment":"none-2"},
                                            {"name":"XYZ", "type":"INT", "available": "always", "comment":"none-2"},
                                            {"name":"K", "type":"INT", "available": "always", "comment":"none-3"}]
                                        })YAML";

    eckit::YAMLConfiguration data_cfg(data_conf_str);

    // configure
    EXPECT_EQUAL(plume::Manager::isConfigured(), false);

    // valid configuration
    plume::Manager::configure(mgr_cfg_valid_json);

    // should be validly initialised
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);
    
    
    // negotiate
    plume::Manager::negotiate(data_cfg);

    // still configured
    EXPECT_EQUAL(plume::Manager::isConfigured(), true);

    // expected to be active parame: I, J, XYZ, K
    EXPECT_EQUAL(plume::Manager::getActiveParams().size(), 4);
    
}



//----------------------------------------------------------------------------------------------------------------------

}  // namespace test
}  // namespace eckit

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}