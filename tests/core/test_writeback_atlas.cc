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
#include <type_traits>

#include "eckit/exception/Exceptions.h"
#include "eckit/testing/Test.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackLedger.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"

#include "ManagerTestAccess.h"

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
        pluginView(0)   = 10;
        pluginView(1)   = 20;
        pluginView(2)   = 30;
        pluginView(3)   = 40;
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

/**
 * @brief Verifies the plugin-facing read-modify-write path: getParam yields a read-only FieldView, FieldView::clone()
 *        gives an independent mutable copy seeded with the current values, and writing that clone back through
 *        writeParam lands in the model's own buffer in place.
 *
 * getParam<atlas::Field> on a ModelDataView must return a plume::data::FieldView (not a mutable atlas::Field),
 * so the only way for a plugin to obtain a writable field is clone() — which is a disconnected deep copy.
 * Mutating the clone must NOT touch the model buffer (proving the read-only guarantee is not reopened);
 * the values reach the model only via writeParam, which copies them in place onto the model's existing FieldImpl.
 */
CASE("test writeback - plugin clones a FieldView, read-modify-writes, and lands in place") {
    plume::data::ModelData data;

    atlas::Field modelField("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    {
        auto initView = atlas::array::make_view<int, 1>(modelField);
        for (int i = 0; i < 4; ++i) {
            initView(i) = i + 1;  // 1, 2, 3, 4
        }
    }
    data.provideParam("F", &modelField);
    const atlas::Field::Implementation* modelImpl = modelField.get();

    // Wire the ledger authorising a named plugin consumer to write "F", then build the plugin-facing view.
    WriteAuthorisation auth;
    auth.grant("pluginA", "F");
    coupling::WriteBackLedger ledger(auth, WriteBackPolicy::single_writer);
    data.enrollWritebackParams(ledger, auth);
    data.attachWritebackLedger(&ledger);
    plume::data::ModelDataView view = data.filter(std::set<std::string>{"F"}, "pluginA");
    ledger.open();

    // getParam on the plugin-facing view yields a read-only FieldView, never a mutable atlas::Field.
    auto fieldView = view.getParam<atlas::Field>("F");
    static_assert(std::is_same_v<decltype(fieldView), plume::data::FieldView>,
                  "getParam<atlas::Field> on a ModelDataView must return a FieldView");

    // The read-only guarantee comes from the FieldView itself, NOT from the caller writing `const`. Request a view
    // with a non-const element type (int): because a FieldView only converts to `const atlas::array::Array&`,
    // make_view resolves to the const-source overload and still yields an ArrayView<const int, ...>. A mutable
    // ArrayView<int, ...> is unobtainable from a FieldView — that is what makes the read path immutable.
    {
        auto ro = atlas::array::make_view<int, 1>(fieldView);  // note: int, NOT const int
        static_assert(std::is_same_v<decltype(ro), atlas::array::ArrayView<const int, 1>>,
                      "make_view<int,1> over a FieldView must yield a read-only ArrayView<const int,1>");
        EXPECT_EQUAL(ro(0), 1);
        EXPECT_EQUAL(ro(3), 4);
    }

    // clone() gives an independent, mutable deep copy seeded with the current values.
    atlas::Field work = fieldView.clone();
    EXPECT(work.get() != modelImpl);  // distinct implementation — shares nothing with the model
    {
        auto wv = atlas::array::make_view<int, 1>(work);
        for (int i = 0; i < 4; ++i) {
            wv(i) += 100;  // 101, 102, 103, 104
        }
    }

    // Mutating the clone must NOT touch the model buffer before write-back (proves deep-copy independence).
    {
        auto stillOriginal = atlas::array::make_view<int, 1>(modelField);
        EXPECT_EQUAL(stillOriginal(0), 1);
        EXPECT_EQUAL(stillOriginal(3), 4);
    }

    // Write the modified clone back through the authorised plugin path.
    view.writeParam<atlas::Field>("F", work);

    // The write lands in the model's own buffer, in place, without swapping the implementation.
    EXPECT(modelField.get() == modelImpl);
    {
        auto afterView = atlas::array::make_view<int, 1>(modelField);
        EXPECT_EQUAL(afterView(0), 101);
        EXPECT_EQUAL(afterView(1), 102);
        EXPECT_EQUAL(afterView(2), 103);
        EXPECT_EQUAL(afterView(3), 104);
    }

    ledger.flush();
    data.acknowledgeWriteback("F");
    data.detachWritebackLedger();
}

/**
 * @brief The value-based writeParam(name, value) write-back must reject an atlas::Field whose shape or datatype does
 *        not match the stored field, via the writeFieldInPlace guard (eckit::UserError). The failure must also be
 *        reported to the ledger (the slot goes to error) rather than silently corrupting the model buffer.
 */
CASE("test writeback - value writeParam rejects a shape/datatype-mismatched field and reports the error") {
    plume::data::ModelData data;

    atlas::Field modelField("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    {
        auto initView = atlas::array::make_view<int, 1>(modelField);
        for (int i = 0; i < 4; ++i) {
            initView(i) = i + 1;
        }
    }
    data.provideParam("F", &modelField);

    WriteAuthorisation auth;
    auth.grant("", "F");
    coupling::WriteBackLedger ledger(auth, WriteBackPolicy::single_writer);
    data.enrollWritebackParams(ledger, auth);
    data.attachWritebackLedger(&ledger);
    ledger.open();

    // Wrong shape: 2 elements vs the stored 4 → UserError, and the failure is recorded on the ledger.
    atlas::Field wrongShape("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(2));
    EXPECT_THROWS_AS(data.writeParam<atlas::Field>("F", wrongShape), eckit::UserError);
    EXPECT(ledger.hasErrors());

    // The model buffer must be untouched by the rejected write.
    {
        auto after = atlas::array::make_view<int, 1>(modelField);
        EXPECT_EQUAL(after(0), 1);
        EXPECT_EQUAL(after(3), 4);
    }
    ManagerTestAccess::forceLedgerReset(ledger);

    // Wrong datatype: double vs the stored int (same shape) → UserError.
    ledger.open();
    atlas::Field wrongType("F", atlas::array::make_datatype<double>(), atlas::array::make_shape(4));
    EXPECT_THROWS_AS(data.writeParam<atlas::Field>("F", wrongType), eckit::UserError);
    EXPECT(ledger.hasErrors());

    ManagerTestAccess::forceLedgerReset(ledger);
    data.detachWritebackLedger();
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
