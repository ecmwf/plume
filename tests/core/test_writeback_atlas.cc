/*
 * (C) Copyright 2025- ECMWF.
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

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackLedger.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/ModelData.h"

#include "atlas/array/ArrayShape.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/DataType.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"

using namespace eckit::testing;

namespace plume::test {

/**
 * @brief Verifies that writing back to a model-provided Atlas field updates the model's own buffer in place.
 *
 * A model-provided field is not owned by Plume: `provideParam` keeps a reference-counted handle that shares the
 * model's `FieldImpl`, so the underlying data array is the model's memory. A plugin write-back must therefore land
 * directly in that shared buffer — mirroring the provided-scalar case, where `writeParam` writes straight through
 * the model's raw pointer. Concretely this requires that:
 *   - the write is observable through the model's original field handle (no read-back needed), and
 *   - the stored implementation is preserved (the handle is not swapped for the plugin's field).
 *
 * The write-back lifecycle is driven directly through the ledger here (rather than the Manager/plugin machinery)
 * to isolate the `ModelData::writeParam` semantics for a provided Atlas field. The model-facing ModelData carries
 * an empty consumer name, so the authorisation is granted to that empty name.
 */
CASE("test writeback - provided atlas field is written back in place") {
    plume::data::ModelData data;

    // Model-owned field with its own atlas-allocated storage, seeded with initial values.
    atlas::Field modelField("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    {
        auto initView = atlas::array::make_view<int, 1>(modelField);
        for (int i = 0; i < 4; ++i) {
            initView(i) = i + 1;
        }
    }
    data.provideParam("F", &modelField);

    // Capture the model's implementation to assert the write-back preserves the handle identity.
    const atlas::Field::Implementation* modelImpl = modelField.get();

    // Confirm the field starts out carrying the initial values, so the post-write check is meaningful.
    {
        auto beforeView = atlas::array::make_view<int, 1>(modelField);
        EXPECT_EQUAL(beforeView(0), 1);
        EXPECT_EQUAL(beforeView(3), 4);
    }

    // Wire the write-back ledger directly, authorising the (empty-named) model-facing consumer to write "F".
    WriteAuthorisation auth;
    auth.grant("", "F");
    coupling::WriteBackLedger ledger(auth, WriteBackPolicy::single_writer);
    data.enrollWritebackParams(ledger, auth);
    data.attachWritebackLedger(&ledger);
    ledger.open();

    // A plugin writes a new field carrying updated values.
    atlas::Field pluginField("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    {
        auto pluginView = atlas::array::make_view<int, 1>(pluginField);
        pluginView(0) = 10;
        pluginView(1) = 20;
        pluginView(2) = 30;
        pluginView(3) = 40;
    }
    data.writeParam<atlas::Field>("F", pluginField);

    // The write must land in the model's own buffer, observable through the model's original handle, without
    // swapping the underlying implementation.
    EXPECT(modelField.get() == modelImpl);
    {
        auto afterView = atlas::array::make_view<int, 1>(modelField);
        EXPECT_EQUAL(afterView(0), 10);
        EXPECT_EQUAL(afterView(3), 40);
    }

    // getParam must observe the same in-place update on the same implementation.
    atlas::Field got = data.getParam<atlas::Field>("F");
    EXPECT(got.get() == modelImpl);

    // Complete the write-back cycle cleanly before the ledger is destroyed.
    ledger.flush();
    data.acknowledgeWriteback("F");
    data.detachWritebackLedger();
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
