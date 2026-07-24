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
#include <stdexcept>
#include <string>
#include <utility>

#include "eckit/exception/Exceptions.h"
#include "eckit/testing/Test.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackLedger.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"

#include "ManagerTestAccess.h"

#include "atlas/array/Array.h"
#include "atlas/array/ArrayShape.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/DataType.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"

using namespace eckit::testing;

namespace plume::test {

namespace {

/// Builds a model with a single int field "F" seeded 1,2,3,4 and its own atlas-allocated storage.
atlas::Field makeSeededField() {
    atlas::Field field("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    auto view = atlas::array::make_view<int, 1>(field);
    for (int i = 0; i < 4; ++i) {
        view(i) = i + 1;  // 1, 2, 3, 4
    }
    return field;
}

/// Reads back the int field and checks every element against `expected`, element by element.
void expectFieldValues(const atlas::Field& field, std::initializer_list<int> expected) {
    auto view = atlas::array::make_view<int, 1>(field);
    int i     = 0;
    for (int value : expected) {
        EXPECT_EQUAL(view(i), value);
        ++i;
    }
}

/// Common write-back wiring shared by every CASE: a ModelData holding the seeded int field "F", an authorisation
/// granting the listed consumers write access to "F", and a ledger enrolled (and, by default, attached) under the
/// given policy. `ledger.open()` and view filtering stay in each CASE so their ordering remains explicit.
struct WritebackHarness {
    plume::data::ModelData data;
    atlas::Field modelField                       = makeSeededField();
    const atlas::Field::Implementation* modelImpl = modelField.get();
    WriteAuthorisation auth;
    coupling::WriteBackLedger ledger;

    WritebackHarness(std::initializer_list<std::string> consumers,
                     WriteBackPolicy policy = WriteBackPolicy::single_writer, bool attach = true) :
        ledger(auth, policy) {
        data.provideParam("F", &modelField);
        for (const auto& consumer : consumers) {
            auth.grant(consumer, "F");
        }
        data.enrollWritebackParams(ledger, auth);
        if (attach) {
            data.attachWritebackLedger(&ledger);
        }
    }

    void attachLedger() { data.attachWritebackLedger(&ledger); }

    plume::data::ModelDataView view(const std::string& consumer) {
        return data.filter(std::set<std::string>{"F"}, consumer);
    }

    /// Successful teardown: flush the ledger, acknowledge the write-back, detach.
    void flushAndDetach() {
        ledger.flush();
        data.acknowledgeWriteback("F");
        data.detachWritebackLedger();
    }

    /// Failure teardown: force the ledger back to a clean state, detach.
    void resetAndDetach() {
        ManagerTestAccess::forceLedgerReset(ledger);
        data.detachWritebackLedger();
    }
};

}  // namespace

CASE("test writeback scope - manual WriteScope writes in place and preserves the FieldImpl") {
    // Model-facing ModelData carries an empty consumer name; authorise that name to write "F".
    WritebackHarness h({""});
    h.ledger.open();

    {
        plume::data::WriteScope scope   = h.data.writeParam("F");
        plume::data::FieldWriter writer = scope.field();
        auto v = atlas::array::make_view<int, 1>(writer);  // MUTABLE view aliasing the model buffer
        for (int i = 0; i < 4; ++i) {
            v(i) *= 10;  // 10, 20, 30, 40 — in place
        }
        scope.commit();
    }

    // The mutation is observable through the model's original handle, on the same implementation.
    EXPECT_EQUAL(h.modelField.get(), h.modelImpl);
    expectFieldValues(h.modelField, {10, 20, 30, 40});
    EXPECT(!h.ledger.hasErrors());

    h.flushAndDetach();
}

CASE("test writeback scope - callable overload commits on normal return, in place") {
    WritebackHarness h({"pluginA"});
    plume::data::ModelDataView view = h.view("pluginA");
    h.ledger.open();

    view.writeParam("F", [](plume::data::FieldWriter& f) {
        auto v = atlas::array::make_view<int, 1>(f);
        for (int i = 0; i < 4; ++i) {
            v(i) += 100;  // 101, 102, 103, 104
        }
    });

    EXPECT_EQUAL(h.modelField.get(), h.modelImpl);
    expectFieldValues(h.modelField, {101, 102, 103, 104});
    EXPECT(!h.ledger.hasErrors());

    h.flushAndDetach();
}

CASE("test writeback scope - callable overload aborts and reports to the ledger when the body throws") {
    WritebackHarness h({"pluginA"});
    plume::data::ModelDataView view = h.view("pluginA");
    h.ledger.open();

    EXPECT(!h.ledger.hasErrors());
    EXPECT_THROWS_AS(view.writeParam("F",
                                     [](plume::data::FieldWriter& f) {
                                         auto v = atlas::array::make_view<int, 1>(f);
                                         v(0)   = -1;  // partial in-place write before the failure
                                         throw std::runtime_error("author failed mid-write");
                                     }),
                     std::runtime_error);

    // The scope was destroyed without commit() → the abort was reported to the ledger.
    EXPECT(h.ledger.hasErrors());

    EXPECT_EQUAL(h.modelField.get(), h.modelImpl);
    expectFieldValues(h.modelField, {-1, 2, 3, 4});  // index 0 written, seeds 2,3,4 untouched

    h.resetAndDetach();
}

CASE("test writeback scope - FieldWriter used after commit throws BadValue") {
    WritebackHarness h({""});
    h.ledger.open();

    {
        plume::data::WriteScope scope   = h.data.writeParam("F");
        plume::data::FieldWriter writer = scope.field();
        {
            auto v = atlas::array::make_view<int, 1>(writer);  // valid while the scope is open
            v(0)   = 7;
        }
        scope.commit();  // poisons the handle (valid_ = false), scope still alive

        EXPECT_THROWS_AS(([&writer]() {
                             atlas::array::Array& a = writer;
                             (void)a;
                         }()),
                         eckit::BadValue);
    }

    h.flushAndDetach();
}

CASE("test writeback scope - writeParam(name) error branches: no ledger, missing param, non-field") {
    // Do not attach the ledger yet: the first branch checks the pre-attach failure.
    WritebackHarness h({""}, WriteBackPolicy::single_writer, /*attach=*/false);
    h.data.createParam<int>("scalar", 7);

    // No ledger attached yet → BadValue.
    EXPECT_THROWS_AS(h.data.writeParam("F"), eckit::BadValue);

    h.attachLedger();
    h.ledger.open();

    // Unknown parameter → BadParameter.
    EXPECT_THROWS_AS(h.data.writeParam("does-not-exist"), eckit::BadParameter);
    // Non-field parameter → BadValue (in-place scope is atlas::Field-only; use writeParam(name, value) instead).
    EXPECT_THROWS_AS(h.data.writeParam("scalar"), eckit::BadValue);

    h.data.detachWritebackLedger();
}

CASE("test writeback scope - moved-from WriteScope is neutralised") {
    WritebackHarness h({""});
    h.ledger.open();

    {
        plume::data::WriteScope src = h.data.writeParam("F");  // staged
        plume::data::WriteScope dst = std::move(src);          // move-construct; src must be neutralised
        {
            plume::data::FieldWriter writer = dst.field();
            auto v                          = atlas::array::make_view<int, 1>(writer);
            v(0)                            = 5;
        }
        dst.commit();
        // src goes out of scope here uncommitted-by-construction; if the move did not neutralise it, its destructor
        // would report a spurious abort to the ledger.
    }

    EXPECT(!h.ledger.hasErrors());  // proves the moved-from src did not report an abort

    h.flushAndDetach();
}

CASE("test writeback scope - callable overload accepts an Array& body") {
    WritebackHarness h({"pluginA"});
    plume::data::ModelDataView view = h.view("pluginA");
    h.ledger.open();

    view.writeParam("F", [](atlas::array::Array& arr) {
        auto v = atlas::array::make_view<int, 1>(arr);
        for (int i = 0; i < 4; ++i) {
            v(i) = 5;
        }
    });
    expectFieldValues(h.modelField, {5, 5, 5, 5});
    EXPECT(!h.ledger.hasErrors());

    h.flushAndDetach();
}

CASE("test writeback scope - callable overload accepts a generic auto& body") {
    WritebackHarness h({"pluginA"});
    plume::data::ModelDataView view = h.view("pluginA");
    h.ledger.open();

    view.writeParam("F", [](auto& f) {
        auto v = atlas::array::make_view<int, 1>(f);
        for (int i = 0; i < 4; ++i) {
            v(i) += 100;
        }
    });
    EXPECT_EQUAL(h.modelField.get(), h.modelImpl);
    expectFieldValues(h.modelField, {101, 102, 103, 104});
    EXPECT(!h.ledger.hasErrors());

    h.flushAndDetach();
}

CASE("test writeback scope - write-back policy is enforced through the WriteScope path") {
    // Single-writer: the second plugin's scope staging violates the policy and throws.
    {
        WritebackHarness h({"A", "B"}, WriteBackPolicy::single_writer);
        plume::data::ModelDataView viewA = h.view("A");
        plume::data::ModelDataView viewB = h.view("B");
        h.ledger.open();

        plume::data::WriteScope scopeA = viewA.writeParam("F");  // A stages
        EXPECT_THROWS(viewB.writeParam("F"));                    // B violates single-writer during staging
        scopeA.commit();

        h.resetAndDetach();
    }

    // Multi-writer: two authorised plugins may each open and commit an in-place scope.
    {
        WritebackHarness h({"A", "B"}, WriteBackPolicy::multi_writer);
        plume::data::ModelDataView viewA = h.view("A");
        plume::data::ModelDataView viewB = h.view("B");
        h.ledger.open();

        {
            plume::data::WriteScope scopeA = viewA.writeParam("F");
            scopeA.commit();
        }
        {
            plume::data::WriteScope scopeB = viewB.writeParam("F");  // allowed under multi-writer
            scopeB.commit();
        }
        EXPECT(!h.ledger.hasErrors());

        h.flushAndDetach();
    }
}

CASE("test writeback scope - destroying a filtered view does not detach the model ledger") {
    WritebackHarness h({"", "pluginA"});  // model-facing "" consumer + plugin consumer
    h.ledger.open();

    {
        // A filtered view borrows the ledger; when it goes out of scope its base destructor must be a no-op w.r.t.
        // the shared ledger (ownsLedger_ == false).
        plume::data::ModelDataView view = h.view("pluginA");
        EXPECT(view.hasParameter("F"));
    }

    // If the view had wrongly detached, this model-facing write would throw "ledger not attached".
    atlas::Field replacement("F", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
    {
        auto v = atlas::array::make_view<int, 1>(replacement);
        for (int i = 0; i < 4; ++i) {
            v(i) = 42;
        }
    }
    EXPECT_NO_THROW(h.data.writeParam<atlas::Field>("F", replacement));
    expectFieldValues(h.modelField, {42, 42, 42, 42});

    h.flushAndDetach();
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
