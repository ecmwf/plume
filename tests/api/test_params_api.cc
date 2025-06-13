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
#include "eckit/runtime/Main.h"
#include "plume/api/plume.h"
#include "test_api_utils.h"


using namespace eckit::testing;

namespace plume::test {

CASE("test_params_api") {

    plume_protocol_handle_t* protocol_handle;
    plume_manager_handle_t* mgr_handle;

    // Init plume
    EXPECT_PLUME_CODE_SUCCESS(plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv()));


    // offer parameters
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_create_handle(&protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "I", "always", "this is param I"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "J", "always", "this is param J"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_float(protocol_handle, "FF1", "always", "this is param FF1"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_float(protocol_handle, "FF2", "always", "this is param FF2"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_double(protocol_handle, "DD1", "always", "this is param DD1"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_atlas_field(protocol_handle, "AA1", "always", "this is param AA1"));

    // configure and Negotiate
    std::string mgr_conf_str = 
    R"YAML({"plugins": [
        {
            "lib": "plume_plugin_test_api",
            "name": "PluginTestAPI", 
            "parameters": [
                [
                    {"name":"I", "type":"INT"},
                    {"name":"J", "type":"INT"},
                    {"name":"FF1", "type":"FLOAT"},
                    {"name":"DD1", "type":"DOUBLE"},
                    {"name":"AA1", "type":"ATLAS_FIELD"}
                ],
                [
                    {"name":"JJJ", "type":"INT"},
                    {"name":"J", "type":"INT"},
                    {"name":"KKMM", "type":"INT"}
                ],
                [
                    {"name":"XYZ", "type":"INT"},
                    {"name":"K", "type":"INT"}
                ]
            ],
            "core-config": {}
        }
    ]})YAML";
    
    // create manager handle
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_create_handle(&mgr_handle));

    // configure manager from string
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_configure_from_string(mgr_handle, mgr_conf_str.c_str()));

    // Negotiate
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_negotiate(mgr_handle, protocol_handle));

    // check that the active parameters are I and J only
    bool param_requested = false;
    for (const auto& name : {"I", "J", "FF1", "DD1", "AA1"}) {
        EXPECT_PLUME_CODE_SUCCESS(plume_manager_is_param_requested(mgr_handle, name, &param_requested));
        EXPECT(param_requested);
        param_requested = false;
    }

    // FF2 is offered but not requested
    param_requested = true;  // reset to true for next checks
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_is_param_requested(mgr_handle, "FF2", &param_requested));
    EXPECT(!param_requested);  // FF2 is not requested

    // check params in rejected groups (these should not be requested)
    param_requested = true;
    for (const auto& name : {"JJJ", "KKMM", "XYZ", "K"}) {
        EXPECT_PLUME_CODE_SUCCESS(plume_manager_is_param_requested(mgr_handle, name, &param_requested));
        EXPECT(!param_requested);
        param_requested = true;
    }

    // check active fields
    char* active_fields_csv = nullptr;
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_active_fields(mgr_handle, &active_fields_csv));
    std::string active_fields(active_fields_csv);

    for (const auto& name : {"I", "J", "FF1", "DD1", "AA1"}) {
        EXPECT(active_fields.find(name) != std::string::npos);
    }

    // check parameters that are not requested
    for (const auto& name : {"FF2", "JJJ", "KKMM", "XYZ", "K"}) {
        EXPECT(active_fields.find(name) == std::string::npos);
    }

    // finalise plume
    EXPECT_PLUME_CODE_SUCCESS(plume_finalise());
}



}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}