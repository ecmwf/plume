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


CASE("test_data_api") {

    plume_manager_handle_t* mgr_handle;
    plume_data_handle_t* data_handle;

    // Init plume
    EXPECT_PLUME_CODE_SUCCESS( plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv()) );
    
    // Provide parameters
    int param_i = 111;
    int param_j = 222;
    float param_ff1 = 333.3;
    double param_dd1 = 444.4;
    
    // Provide parameters
    EXPECT_PLUME_CODE_SUCCESS( plume_data_create_handle_t(&data_handle) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "I", &param_i) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_int(data_handle, "J", &param_j) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_float(data_handle, "FF1", &param_ff1) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_provide_double(data_handle, "DD1", &param_dd1) );

    // Create parameters
    EXPECT_PLUME_CODE_SUCCESS( plume_data_create_int(data_handle, "CC_I", -1) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_create_float(data_handle, "CC_F", -1.1) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_create_double(data_handle, "CC_D", -2.2) );

    std::cout << "Parameters provided" << std::endl;

    // check parameters
    int param_i_check = 0;
    int param_j_check = 0;
    float param_ff1_check = 0.0f;
    double param_dd1_check = 0.0;

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_int(data_handle, "I", &param_i_check) );
    EXPECT_EQUAL(param_i_check, param_i);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_int(data_handle, "J", &param_j_check) );
    EXPECT_EQUAL(param_j_check, param_j);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_float(data_handle, "FF1", &param_ff1_check) );
    EXPECT_EQUAL(param_ff1_check, param_ff1);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_double(data_handle, "DD1", &param_dd1_check) );
    EXPECT_EQUAL(param_dd1_check, param_dd1);

    // check not found parameters
    int param_not_found = -999;
    EXPECT_PLUME_CODE_FAILURE( plume_data_get_int(data_handle, "param-not-found", &param_not_found) );
    EXPECT_EQUAL(param_not_found, -999);


    // check created parameters
    int param_cc_i_check = 0;
    float param_cc_f_check = 0.0f;
    double param_cc_d_check = 0.0;

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_int(data_handle, "CC_I", &param_cc_i_check) );
    EXPECT_EQUAL(param_cc_i_check, -1);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_float(data_handle, "CC_F", &param_cc_f_check) );
    EXPECT_EQUAL(param_cc_f_check, -1.1f);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_double(data_handle, "CC_D", &param_cc_d_check) );
    EXPECT_EQUAL(param_cc_d_check, -2.2);


    // now update the (created) parameters
    EXPECT_PLUME_CODE_SUCCESS( plume_data_update_int(data_handle, "CC_I", 777) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_update_float(data_handle, "CC_F", 888.8f) );
    EXPECT_PLUME_CODE_SUCCESS( plume_data_update_double(data_handle, "CC_D", 999.9) );

    // check updated parameters
    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_int(data_handle, "CC_I", &param_cc_i_check) );
    EXPECT_EQUAL(param_cc_i_check, 777);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_float(data_handle, "CC_F", &param_cc_f_check) );
    EXPECT_EQUAL(param_cc_f_check, 888.8f);

    EXPECT_PLUME_CODE_SUCCESS( plume_data_get_double(data_handle, "CC_D", &param_cc_d_check) );
    EXPECT_EQUAL(param_cc_d_check, 999.9);


    // Print data handle
    EXPECT_PLUME_CODE_SUCCESS( plume_data_print(data_handle) );

    // finalise
    EXPECT_PLUME_CODE_SUCCESS( plume_data_delete_handle(data_handle) );
    EXPECT_PLUME_CODE_SUCCESS( plume_finalise() );
}



}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}