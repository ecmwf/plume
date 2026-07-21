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

#include "eckit/runtime/Main.h"
#include "eckit/testing/Test.h"
#include "eckit/types/FloatCompare.h"
#include "plume/api/plume.h"
#include "test_api_utils.h"

#include "atlas/array/ArrayShape.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/DataType.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"

using namespace eckit::testing;

namespace plume::test {

CASE("test_writeback_api") {

    plume_protocol_handle_t* protocol_handle;
    plume_manager_handle_t* mgr_handle;
    plume_data_handle_t* data_handle;

    EXPECT_PLUME_CODE_SUCCESS(plume_initialise(eckit::Main::instance().argc(), eckit::Main::instance().argv()));

    // Offer writable parameters (model side)
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_create_handle(&protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_int_writable(protocol_handle, "W_INT", "always", "writable int"));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_offer_bool_writable(protocol_handle, "W_BOOL", "always", "writable bool"));
    EXPECT_PLUME_CODE_SUCCESS(
        plume_protocol_offer_float_writable(protocol_handle, "W_FLOAT", "always", "writable float"));
    EXPECT_PLUME_CODE_SUCCESS(
        plume_protocol_offer_double_writable(protocol_handle, "W_DOUBLE", "always", "writable double"));
    EXPECT_PLUME_CODE_SUCCESS(
        plume_protocol_offer_atlas_field_writable(protocol_handle, "W_FIELD", "always", "writable atlas field"));

    std::string mgr_conf_str = R"YAML(
      write-back-policy: single-writer
      plugins:
        - lib: plume_plugin_writeback_api
          name: WriteBackTestAPI
          parameters:
            -
              - name: W_INT
                type: INT
                writable: true
              - name: W_BOOL
                type: BOOL
                writable: true
              - name: W_FLOAT
                type: FLOAT
                writable: true
              - name: W_DOUBLE
                type: DOUBLE
                writable: true
              - name: W_FIELD
                type: ATLAS_FIELD
                writable: true
          core-config: {}
    )YAML";

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_create_handle(&mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_configure_from_string(mgr_handle, mgr_conf_str.c_str()));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_negotiate(mgr_handle, protocol_handle));

    EXPECT_PLUME_CODE_SUCCESS(plume_data_create_handle_t(&data_handle));

    // Writable params can be either provided (model-owned memory, raw pointer) or
    // created (Plume-owned memory). Both are tested here.
    int w_int     = 0;
    bool w_bool   = false;
    float w_float = 0.0f;
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_int(data_handle, "W_INT", &w_int));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_bool(data_handle, "W_BOOL", &w_bool));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_float(data_handle, "W_FLOAT", &w_float));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_create_double(data_handle, "W_DOUBLE", 0.0));  // Plume-owned

    // Provided (model-owned) atlas field: storage stays with the model, shared with Plume via a handle. The
    // write-back must land in this buffer in place, observable through this very field without a read-back.
    atlas::Field w_field("W_FIELD", atlas::array::make_datatype<int>(), atlas::array::make_shape(3));
    {
        auto seed = atlas::array::make_view<int, 1>(w_field);
        seed(0) = 1;
        seed(1) = 2;
        seed(2) = 3;
    }
    const atlas::Field::Implementation* w_field_impl = w_field.get();
    EXPECT_PLUME_CODE_SUCCESS(plume_data_provide_atlas_field_shared(data_handle, "W_FIELD", w_field.get()));

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_feed_plugins(mgr_handle, data_handle));

    // Run the plugin — it writes W_INT=42, W_BOOL=true, W_FLOAT=3.14f, W_DOUBLE=2.718
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_run(mgr_handle));

    // Verify values written to model-owned memory via raw pointer
    EXPECT_EQUAL(w_int, 42);
    EXPECT_EQUAL(w_bool, true);
    EXPECT(eckit::types::is_approximately_equal(w_float, 3.14f));

    // Verify value written to Plume-owned memory via get
    double w_double_check = 0.0;
    EXPECT_PLUME_CODE_SUCCESS(plume_data_get_double(data_handle, "W_DOUBLE", &w_double_check));
    EXPECT(eckit::types::is_approximately_equal(w_double_check, 2.718));

    // Verify the provided atlas field was written in place: the model's own handle sees the new values and the
    // underlying implementation was not swapped for the plugin's field.
    EXPECT(w_field.get() == w_field_impl);
    {
        auto check = atlas::array::make_view<int, 1>(w_field);
        EXPECT_EQUAL(check(0), 100);
        EXPECT_EQUAL(check(1), 200);
        EXPECT_EQUAL(check(2), 300);
    }

    // Verify pending write-backs
    char* pending_csv = nullptr;
    EXPECT_PLUME_CODE_SUCCESS(plume_data_pending_writebacks(data_handle, &pending_csv));
    std::string pending(pending_csv);
    plume_free_string(pending_csv);

    EXPECT(pending.find("W_INT") != std::string::npos);
    EXPECT(pending.find("W_BOOL") != std::string::npos);
    EXPECT(pending.find("W_FLOAT") != std::string::npos);
    EXPECT(pending.find("W_DOUBLE") != std::string::npos);
    EXPECT(pending.find("W_FIELD") != std::string::npos);

    // Acknowledge each write-back
    EXPECT_PLUME_CODE_SUCCESS(plume_data_acknowledge_writeback(data_handle, "W_INT"));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_acknowledge_writeback(data_handle, "W_BOOL"));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_acknowledge_writeback(data_handle, "W_FLOAT"));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_acknowledge_writeback(data_handle, "W_DOUBLE"));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_acknowledge_writeback(data_handle, "W_FIELD"));

    // Verify no more pending write-backs after acknowledgement
    EXPECT_PLUME_CODE_SUCCESS(plume_data_pending_writebacks(data_handle, &pending_csv));
    pending = pending_csv;
    plume_free_string(pending_csv);

    EXPECT(pending.find("W_INT") == std::string::npos);
    EXPECT(pending.find("W_BOOL") == std::string::npos);
    EXPECT(pending.find("W_FLOAT") == std::string::npos);
    EXPECT(pending.find("W_DOUBLE") == std::string::npos);
    EXPECT(pending.find("W_FIELD") == std::string::npos);

    EXPECT_PLUME_CODE_SUCCESS(plume_manager_teardown(mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_manager_delete_handle(mgr_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_data_delete_handle(data_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_protocol_delete_handle(protocol_handle));
    EXPECT_PLUME_CODE_SUCCESS(plume_finalise());
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
