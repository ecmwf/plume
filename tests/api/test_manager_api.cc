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

CASE("test_manager_api") {

    plume_protocol_handle_t* protocol_handle;
    plume_manager_handle_t* mgr_handle;
    plume_data_handle_t* data_handle;
    char* state_name;
    int64_t state_iteration = -1;
    int64_t state_iteration_rel = -1;

    int error_code = PlumeErrorValues::PLUME_ERROR_GENERAL_EXCEPTION;

    // Init plume
    EXPECT_PLUME_CODE_SUCCESS( plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv()));

    // offer parameters
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_create_handle(&protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_int(protocol_handle, "I", "always", "this is param I"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_int(protocol_handle, "J", "always", "this is param J"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_float(protocol_handle, "FF1", "always", "this is param FF1"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_float(protocol_handle, "FF2", "on-demand", "this is param FF2"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_double(protocol_handle, "DD1", "always", "this is param DD1"));


    // offer parameters for the Fortran plugin
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_int(protocol_handle, "FORT_I", "always", "this is param FORT_I"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_int(protocol_handle, "FORT_J", "always", "this is param FORT_J"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_float(protocol_handle, "FORT_FF1", "always", "this is param FORT_FF1"));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_double(protocol_handle, "FORT_DD1", "always", "this is param FORT_DD1"));

    // configure and Negotiate
    std::string mgr_conf_str = 
    R"YAML(
      plugins:
        - lib: plume_plugin_test_api
          name: PluginTestAPI
          parameters:
            -
              - name: I
                type: INT
              - name: J
                type: INT
              - name: FF1
                type: FLOAT
              - name: DD1
                type: DOUBLE
            -
              - name: JJJ
                type: INT
              - name: J
                type: INT
              - name: KKMM
                type: INT
            -
              - name: XYZ
                type: INT
              - name: K
                type: INT
          core-config: {}
        - lib: plume_plugin_test_fapi
          name: PluginTestFAPI
          parameters:
            -
              - name: FORT_I
                type: INT
              - name: FORT_J
                type: INT
              - name: FORT_FF1
                type: FLOAT
              - name: FORT_DD1
                type: DOUBLE
          core-config: {}
    )YAML";
    
    // create manager handle
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_create_handle(&mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_configure_from_string(mgr_handle, mgr_conf_str.c_str()));
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_name(mgr_handle, &state_name));
    EXPECT_EQUAL(std::string(state_name), "configure");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration(mgr_handle, &state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration_rel(mgr_handle, &state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    EXPECT_PLUME_CODE_SUCCESS( plume_manager_negotiate(mgr_handle, protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_name(mgr_handle, &state_name));
    EXPECT_EQUAL(std::string(state_name), "negotiate");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration(mgr_handle, &state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration_rel(mgr_handle, &state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    // check that the plugins are activated
    bool plugin_activated = false;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_plugin_activated(mgr_handle, "PluginTestAPI", &plugin_activated));
    EXPECT(plugin_activated);

    // check that the Fortran plugin is activated
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_plugin_activated(mgr_handle, "PluginTestFAPI", &plugin_activated));
    EXPECT(plugin_activated);

    // check that a non-existent plugin is not activated
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_plugin_activated(mgr_handle, "NonExistentPlugin", &plugin_activated));
    EXPECT(!plugin_activated);

    // Provide data as needed
    int param_i = 111;
    int param_j = 222;
    float param_ff1 = 333.3;
    double param_dd1 = 444.4;

    // parameters for the Fortran plugin
    int param_fort_i = 555;
    int param_fort_j = 666;
    float param_fort_ff1 = 777.7;
    double param_fort_dd1 = 888.8;

    EXPECT_PLUME_CODE_SUCCESS( plume_data_create_handle_t(&data_handle) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "I", &param_i) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "J", &param_j) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_float(data_handle, "FF1", &param_ff1) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_double(data_handle, "DD1", &param_dd1) );

    // parameters for the Fortran plugin
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "FORT_I", &param_fort_i) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "FORT_J", &param_fort_j) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_float(data_handle, "FORT_FF1", &param_fort_ff1) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_double(data_handle, "FORT_DD1", &param_fort_dd1) );

    // Feed the plugins (i.e. each plugin grabs its own share of data)
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_feed_plugins(mgr_handle, data_handle) );
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_name(mgr_handle, &state_name));
    EXPECT_EQUAL(std::string(state_name), "feed_plugins");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration(mgr_handle, &state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration_rel(mgr_handle, &state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    // run the plugin for 2 iterations
    for (int i = 0; i < 2; ++i) {
      EXPECT_PLUME_CODE_SUCCESS( plume_manager_run(mgr_handle, PLUME_TAG_ID_RUN, false, PLUME_TAG_ID_RUN));
      EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_name(mgr_handle, &state_name));
      EXPECT_EQUAL(std::string(state_name), "run");
      delete[] state_name;
      EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration(mgr_handle, &state_iteration));
      EXPECT_EQUAL(state_iteration, i + 1);
      EXPECT_PLUME_CODE_SUCCESS( plume_manager_current_state_iteration_rel(mgr_handle, &state_iteration_rel));
      EXPECT_EQUAL(state_iteration_rel, i + 1);
    }

    // finalise plume
    EXPECT_PLUME_CODE_SUCCESS( plume_data_delete_handle(data_handle));
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_delete_handle(protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_delete_handle(mgr_handle));
    // Finalise plume
    EXPECT_PLUME_CODE_SUCCESS( plume_finalise());
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}