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

#include <cstdint>
#include <string>

#include "eckit/testing/Test.h"
#include "eckit/runtime/Main.h"
#include "plume/api/plume.h"
#include "test_api_utils.h"

using namespace eckit::testing;

namespace plume::test {

// -----------------------------------------------------------------------
// Tests the standalone plume_state_* C API, which queries the global
// PlumeState singleton independently of the manager handle.
// The manager is used only to drive state transitions.
// -----------------------------------------------------------------------

static const std::string MGR_CONF_STR = R"YAML(
plugins:
  - lib: plume_plugin_test_api
    name: PluginTestAPI
    parameters:
      -
        - name: I
          type: INT
        - name: J
          type: INT
    core-config: {}
)YAML";

CASE("test_state_api_lifecycle") {

    plume_protocol_handle_t* protocol_handle = nullptr;
    plume_manager_handle_t*  mgr_handle      = nullptr;
    plume_data_handle_t*     data_handle     = nullptr;
    char*    state_name          = nullptr;
    int64_t  state_iteration     = -1;
    int64_t  state_iteration_rel = -1;

    EXPECT_PLUME_CODE_SUCCESS(plume_initialise(eckit::Main::instance().argc(),
                                               eckit::Main::instance().argv()));

    // ----- offers -----
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_create_handle(&protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "I", "always", "param I"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "J", "always", "param J"));

    // ----- configure -----
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_create_handle(&mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_configure_from_string(mgr_handle, MGR_CONF_STR.c_str()));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "configure");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    // ----- negotiate -----
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_negotiate(mgr_handle, protocol_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "negotiate");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    // ----- feed plugins -----
    int param_i = 1;
    int param_j = 2;
    EXPECT_PLUME_CODE_SUCCESS(plume_data_create_handle_t(&data_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_int(data_handle, "I", &param_i));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_int(data_handle, "J", &param_j));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_feed_plugins(mgr_handle, data_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "feed_plugins");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    // ----- run (3 iterations) -----
    for (int i = 1; i <= 3; ++i) {
      EXPECT_PLUME_CODE_SUCCESS(plume_manager_run(mgr_handle, PLUME_TAG_ID_RUN, false, PLUME_TAG_ID_RUN));

        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
        EXPECT_EQUAL(std::string(state_name), "run");
        delete[] state_name;
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
        EXPECT_EQUAL(state_iteration, static_cast<int64_t>(i));
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
        EXPECT_EQUAL(state_iteration_rel, static_cast<int64_t>(i));
    }

    // ----- teardown -----
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_teardown(mgr_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "teardown");
    delete[] state_name;
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
    EXPECT_EQUAL(state_iteration, 1);
    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
    EXPECT_EQUAL(state_iteration_rel, 1);

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_delete_handle(mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_delete_handle(protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_delete_handle(data_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_finalise());
}


CASE("test_state_api_enum_tags") {

    plume_protocol_handle_t* protocol_handle = nullptr;
    plume_manager_handle_t*  mgr_handle      = nullptr;
    plume_data_handle_t*     data_handle     = nullptr;
    char*    state_name          = nullptr;
    int64_t  state_iteration     = -1;
    int64_t  state_iteration_rel = -1;

    EXPECT_PLUME_CODE_SUCCESS(plume_initialise(eckit::Main::instance().argc(),
                                               eckit::Main::instance().argv()));

    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_create_handle(&protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "I", "always", "param I"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int(protocol_handle, "J", "always", "param J"));

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_create_handle(&mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_configure_from_string(mgr_handle, MGR_CONF_STR.c_str()));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "configure");
    delete[] state_name;

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_negotiate(mgr_handle, protocol_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "negotiate");
    delete[] state_name;

    int param_i = 1;
    int param_j = 2;
    EXPECT_PLUME_CODE_SUCCESS(plume_data_create_handle_t(&data_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_int(data_handle, "I", &param_i));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_int(data_handle, "J", &param_j));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_feed_plugins(mgr_handle, data_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "feed_plugins");
    delete[] state_name;

    // ----- nested run: outer (2 iterations) / inner (1 per outer) -----
    for (int i = 1; i <= 2; ++i) {
      EXPECT_PLUME_CODE_SUCCESS(plume_manager_run(mgr_handle, PLUME_TAG_ID_RUN_LVL1, false, PLUME_TAG_ID_RUN_LVL1));

        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
        EXPECT_EQUAL(std::string(state_name), "run_lvl1");
        delete[] state_name;
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
        EXPECT_EQUAL(state_iteration, static_cast<int64_t>(i));
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
        EXPECT_EQUAL(state_iteration_rel, static_cast<int64_t>(i));

        // inner run -- relative iteration resets when outer advances
        EXPECT_PLUME_CODE_SUCCESS(plume_manager_run(mgr_handle, PLUME_TAG_ID_RUN_LVL2, true, PLUME_TAG_ID_RUN_LVL1));

        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
        EXPECT_EQUAL(std::string(state_name), "run_lvl2");
        delete[] state_name;
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration(&state_iteration));
        EXPECT_EQUAL(state_iteration, static_cast<int64_t>(i));  // cumulative
        EXPECT_PLUME_CODE_SUCCESS(plume_state_current_iteration_rel(&state_iteration_rel));
        EXPECT_EQUAL(state_iteration_rel, int64_t{1});            // reset by parent advance
    }

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_teardown(mgr_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_state_current_name(&state_name));
    EXPECT_EQUAL(std::string(state_name), "teardown");
    delete[] state_name;

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_delete_handle(mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_delete_handle(protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_delete_handle(data_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_finalise());
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return eckit::testing::run_tests(argc, argv);
}
