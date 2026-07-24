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

// Compile-time enforcement fixture for the plugin/model data-access contract (PLUME-72/75).
//
// ModelDataView privately inherits ModelData and re-publishes (via `using`) only the plugin-legal
// surface. The static_asserts below prove structurally that the model-only mutators are NOT reachable
// through a ModelDataView, while the read/write-back surface IS — so a plugin that only ever holds a
// ModelDataView cannot call updateParam/provideParam/createParam/filter. If the enforcement regresses,
// this translation unit fails to COMPILE (the tightest possible failure signal), long before any run.

#include <set>
#include <string>
#include <type_traits>

#include "eckit/testing/Test.h"

#include "atlas/array/Array.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"

#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"

using namespace eckit::testing;

namespace plume::test {

using data::FieldView;
using data::ModelData;
using data::ModelDataView;

// --- Callability detectors: `can_X<T>::value` is true if the expression X is well-formed (and
//     accessible) for an object of type T. Access violations from private inheritance are substitution
//     failures (C++11 DR1170), so a hidden inherited member yields false rather than a hard error. ------

template <class T, class = void>
struct can_updateParam : std::false_type {};
template <class T>
struct can_updateParam<T, std::void_t<decltype(std::declval<T&>().template updateParam<int>(std::string{}, 0))>>
    : std::true_type {};

template <class T, class = void>
struct can_provideParam : std::false_type {};
template <class T>
struct can_provideParam<
    T, std::void_t<decltype(std::declval<T&>().template provideParam<int>(std::string{}, std::declval<int*>()))>>
    : std::true_type {};

template <class T, class = void>
struct can_createParam : std::false_type {};
template <class T>
struct can_createParam<T, std::void_t<decltype(std::declval<T&>().template createParam<int>(std::string{}, 0))>>
    : std::true_type {};

template <class T, class = void>
struct can_filter : std::false_type {};
template <class T>
struct can_filter<T, std::void_t<decltype(std::declval<const T&>().filter(std::set<std::string>{}, std::string{}))>>
    : std::true_type {};

template <class T, class = void>
struct can_getParam : std::false_type {};
template <class T>
struct can_getParam<T, std::void_t<decltype(std::declval<const T&>().template getParam<int>(std::string{}))>>
    : std::true_type {};

template <class T, class = void>
struct can_writeParam : std::false_type {};
template <class T>
struct can_writeParam<
    T, std::void_t<decltype(std::declval<T&>().template writeParam<int>(std::string{}, std::declval<const int&>()))>>
    : std::true_type {};

template <class T, class = void>
struct can_hasParameter : std::false_type {};
template <class T>
struct can_hasParameter<T, std::void_t<decltype(std::declval<const T&>().hasParameter(std::string{}))>>
    : std::true_type {};

// Sanity: the detectors themselves are correct — the mutators ARE reachable on the full ModelData.
static_assert(can_updateParam<ModelData>::value, "ModelData must expose updateParam");
static_assert(can_provideParam<ModelData>::value, "ModelData must expose provideParam");
static_assert(can_createParam<ModelData>::value, "ModelData must expose createParam");
static_assert(can_filter<ModelData>::value, "ModelData must expose filter");

// Core contract: model-only mutators are NOT reachable through the plugin-facing view.
static_assert(!can_updateParam<ModelDataView>::value, "ModelDataView must NOT expose updateParam");
static_assert(!can_provideParam<ModelDataView>::value, "ModelDataView must NOT expose provideParam");
static_assert(!can_createParam<ModelDataView>::value, "ModelDataView must NOT expose createParam");
static_assert(!can_filter<ModelDataView>::value, "ModelDataView must NOT expose filter");

// Plugin-legal surface is available on BOTH the model and the view.
static_assert(can_getParam<ModelData>::value && can_getParam<ModelDataView>::value,
              "getParam must be reachable on ModelData and ModelDataView");
static_assert(can_writeParam<ModelData>::value && can_writeParam<ModelDataView>::value,
              "writeParam must be reachable on ModelData and ModelDataView");
static_assert(can_hasParameter<ModelData>::value && can_hasParameter<ModelDataView>::value,
              "hasParameter must be reachable on ModelData and ModelDataView");

// Structural: a view cannot be sliced or cast back to its private base to reach the hidden methods.
static_assert(!std::is_convertible<ModelDataView*, ModelData*>::value,
              "ModelDataView* must NOT convert to ModelData* (private inheritance)");
static_assert(!std::is_convertible<ModelDataView&, ModelData&>::value,
              "ModelDataView& must NOT convert to ModelData& (private inheritance)");

// getParam<atlas::Field> yields a read-only FieldView on the view, never a mutable atlas::Field ----------

// Return-type wiring: fields become FieldView on the view; scalars are unchanged; the model path still hands back a
// real atlas::Field.
static_assert(
    std::is_same_v<decltype(std::declval<const ModelDataView&>().getParam<atlas::Field>(std::string{})), FieldView>,
    "ModelDataView::getParam<atlas::Field> must return a FieldView");
static_assert(std::is_same_v<decltype(std::declval<const ModelDataView&>().getParam<int>(std::string{})), int>,
              "scalar getParam on a view must be unchanged (returns by value)");
static_assert(
    std::is_same_v<decltype(std::declval<const ModelData&>().getParam<atlas::Field>(std::string{})), atlas::Field>,
    "ModelData::getParam<atlas::Field> (model path) must still return a real atlas::Field");

// The derived-name (level/levtype) getParam overload on the view must apply the SAME read-only wrapping. This pins the
// second overload so it cannot silently drift from the single-argument one and leak a mutable field.
static_assert(std::is_same_v<decltype(std::declval<const ModelDataView&>().getParam<atlas::Field>(
                                 std::string{}, std::string{}, std::string{})),
                             FieldView>,
              "ModelDataView::getParam<atlas::Field>(name, level, levtype) must return a FieldView");
static_assert(
    std::is_same_v<
        decltype(std::declval<const ModelDataView&>().getParam<int>(std::string{}, std::string{}, std::string{})), int>,
    "scalar getParam(name, level, levtype) on a view must be unchanged (returns by value)");

// FieldView must NOT be convertible to a mutable atlas::Field (so `atlas::Field f = getParam(...)`
// does not compile) nor to a mutable Array&; only the read-only `const Array&` conversion exists.
static_assert(!std::is_convertible_v<FieldView, atlas::Field>, "FieldView must NOT convert to a mutable atlas::Field");
static_assert(!std::is_convertible_v<FieldView, atlas::array::Array&>,
              "FieldView must NOT convert to a mutable Array&");
static_assert(std::is_convertible_v<FieldView, const atlas::array::Array&>,
              "FieldView must convert to a read-only const Array& (so make_view keeps working)");

// clone() is the sanctioned mutable path: it yields an independent, owning atlas::Field (a deep copy).
static_assert(std::is_same_v<decltype(std::declval<const FieldView&>().clone()), atlas::Field>,
              "FieldView::clone() must return an owning atlas::Field deep copy");

// FieldWriter is the scope-bound in-place write handle. It must be non-copyable AND non-movable so a plugin cannot
// stash it by value and use it past the WriteScope that owns its validity flag (`auto stolen = f;` must not compile).
using data::FieldWriter;
static_assert(!std::is_copy_constructible_v<FieldWriter>, "FieldWriter must be non-copyable");
static_assert(!std::is_copy_assignable_v<FieldWriter>, "FieldWriter must be non-copy-assignable");
static_assert(!std::is_move_constructible_v<FieldWriter>, "FieldWriter must be non-movable");
static_assert(!std::is_move_assignable_v<FieldWriter>, "FieldWriter must be non-move-assignable");

// FieldWriter is the write sibling of FieldView: it DOES convert to a mutable Array& (in-place write path), but must
// not leak a copyable atlas::Field handle that would outlive the scope and alias the model buffer unguarded.
static_assert(std::is_convertible_v<FieldWriter&, atlas::array::Array&>,
              "FieldWriter must convert to a mutable Array& (the in-place write path)");
static_assert(!std::is_convertible_v<FieldWriter&, atlas::Field>,
              "FieldWriter must NOT convert to a copyable atlas::Field handle");

// Mutable-view creation is rejected structurally: because the only conversion is to const Array&, make_view<int,Rank>
// over a FieldView resolves to the const-source overload and yields ArrayView<const int,Rank>. A mutable
// ArrayView<int,Rank> is therefore unobtainable from a FieldView — the read-only-ness comes from the source, not the
// caller writing `const`.
static_assert(std::is_same_v<decltype(atlas::array::make_view<int, 1>(std::declval<const FieldView&>())),
                             atlas::array::ArrayView<const int, 1>>,
              "make_view<int,1> over a FieldView must yield a read-only ArrayView<const int,1>");

//----------------------------------------------------------------------------------------------------------------------

// A runtime case so the framework reports a passing test; it also exercises the exposed surface.
CASE("model_data_view_exposes_only_plugin_surface") {
    ModelData data;
    data.createParam<int>("x", 42);

    ModelDataView view = data.filter(std::set<std::string>{"x"}, "PluginA");

    EXPECT(view.hasParameter("x"));
    EXPECT_EQUAL(view.getParam<int>("x"), 42);
    EXPECT_EQUAL(view.consumer(), std::string("PluginA"));
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
