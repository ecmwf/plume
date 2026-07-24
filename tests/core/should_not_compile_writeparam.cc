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

// Compile-time enforcement fixture for the value-based writeParam(name, value) overload (PLUME-75).
//
// FieldView (read-only handle) and FieldWriter (scope-bound in-place handle) are access handles, NOT values to be
// written back.
//
// Each variant type-checks up to the offending writeParam call; only the static_assert should stop compilation.

#include <string>

#include "plume/data/FieldAccess.h"
#include "plume/data/ModelData.h"

#include "atlas/array/ArrayShape.h"
#include "atlas/array/DataType.h"
#include "atlas/field/Field.h"

void trigger() {
    plume::data::ModelData data;

#if defined(CHECK_FIELDVIEW)
    // A FieldView (e.g. obtained from getParam) is read-only and must not be accepted as a writeParam value —
    // the author must clone() it to a mutable atlas::Field first.
    atlas::Field field("x", atlas::array::make_datatype<int>(), atlas::array::make_shape(1));
    plume::data::FieldView fieldView(field);
    data.writeParam("x", fieldView);  // MUST NOT COMPILE
#elif defined(CHECK_FIELDWRITER)
    // A FieldWriter is the in-place handle a WriteScope hands out; feeding it back into the value overload is a
    // category error (it aliases the model buffer, it is not a value to copy in).
    plume::data::WriteScope scope = data.writeParam("x");
    data.writeParam("x", scope.field());  // MUST NOT COMPILE
#else
#error "Define CHECK_FIELDVIEW or CHECK_FIELDWRITER to select the rejection under test"
#endif
}
