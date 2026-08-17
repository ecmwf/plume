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

#include <string>

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

    // register an additional hook point (the default one is always registered)
    EXPECT_PLUME_CODE_SUCCESS( plume_protocol_offer_hook(protocol_handle, "api-hook", "an extra hook point"));

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
          hooks: [api-hook]
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
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_negotiate(mgr_handle, protocol_handle));

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

    // --- hook points -------------------------------------------------------------------------
    // the Fortran plugin has been re-targeted onto "api-hook" through its configuration, the C++
    // one declares no hook point and is therefore bound to the default one
    bool hook_active = false;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_hook_active(mgr_handle, "api-hook", &hook_active));
    EXPECT(hook_active);

    // an unregistered hook point is an error
    EXPECT_PLUME_CODE_FAILURE( plume_manager_is_hook_active(mgr_handle, "not-a-hook", &hook_active));

    // the registered hook points: the one offered above plus the implicit default
    char* registered_hooks = nullptr;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_registered_hooks(mgr_handle, &registered_hooks));
    EXPECT(registered_hooks != nullptr);
    EXPECT(std::string(registered_hooks).find("api-hook") != std::string::npos);
    EXPECT(std::string(registered_hooks).find("default") != std::string::npos);
    delete[] registered_hooks;

    // params requested at "api-hook" are the Fortran plugin's, not the C++ plugin's
    bool param_at_hook = false;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_param_requested_at_hook(mgr_handle, "FORT_I", "api-hook", &param_at_hook));
    EXPECT(param_at_hook);
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_param_requested_at_hook(mgr_handle, "I", "api-hook", &param_at_hook));
    EXPECT(!param_at_hook);

    // ... and the other way round at the default hook point
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_param_requested_at_hook(mgr_handle, "I", "default", &param_at_hook));
    EXPECT(param_at_hook);
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_is_param_requested_at_hook(mgr_handle, "FORT_I", "default", &param_at_hook));
    EXPECT(!param_at_hook);

    // the CSV listing of the params requested at a hook point
    char* hook_fields = nullptr;
    EXPECT_PLUME_CODE_SUCCESS( plume_manager_active_fields_at_hook(mgr_handle, "api-hook", true, &hook_fields));
    EXPECT(hook_fields != nullptr);
    EXPECT(std::string(hook_fields).find("FORT_I") != std::string::npos);
    EXPECT(std::string(hook_fields).find(",I,") == std::string::npos);
    delete[] hook_fields;

    EXPECT_PLUME_CODE_FAILURE( plume_manager_active_fields_at_hook(mgr_handle, "not-a-hook", true, &hook_fields));

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

    // run the plugin for 2 iterations: the default hook point runs the C++ plugin, "api-hook"
    // runs the Fortran one
    for (int i = 0; i < 2; ++i) {
        EXPECT_PLUME_CODE_SUCCESS( plume_manager_run(mgr_handle));
        EXPECT_PLUME_CODE_SUCCESS( plume_manager_run_hook(mgr_handle, "api-hook"));
    }

    // running an unregistered hook point is an error, not a silent no-op
    EXPECT_PLUME_CODE_FAILURE( plume_manager_run_hook(mgr_handle, "not-a-hook"));

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