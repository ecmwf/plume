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
#include <array>
#include <memory>

#include "atlas/field/Field.h"

#include "eckit/testing/Test.h"

#include "plume/data/FieldProvider.h"
#include "plume/data/ParameterValue.h"

using namespace eckit::testing;

namespace plume::test {

CASE("test update strategies - wind at height") {

    using AtlasObservable = plume::data::ParameterValue<atlas::Field, plume::data::IParameterObservable>;
    using AtlasObserver   = plume::data::ParameterValue<atlas::Field, plume::data::IParameterObserver>;

    atlas::Field geopotential("z", atlas::array::make_datatype<double>(), atlas::array::make_shape(3, 5));
    atlas::Field u("u", atlas::array::make_datatype<double>(), atlas::array::make_shape(3, 5));
    u.set_levels(5);

    // mimick how the model data creates the target field
    atlas::Field u200tmp = u.clone();
    atlas::Field u5000tmp = u.clone();

    EXPECT_EQUAL(u200tmp.levels(), 5);
    EXPECT_EQUAL(u200tmp.shape(), atlas::array::make_shape(3, 5));

    auto zView = atlas::array::make_view<double, 2>(geopotential);
    auto uView = atlas::array::make_view<double, 2>(u);

    // Representative values of z & u over Europe for 300 - 1000 hPa
    // Model levels grow closer to the surface
    std::array<std::array<double, 5>, 3> zValues = {{{{92282.0, 55898.0, 30499.0, 15200.0, 1177.0}},
                                                     {{90221.0, 54917.0, 29420.0, 14710.0, 981.0}},
                                                     {{88260.0, 52956.0, 28439.0, 14220.0, 785.0}}}};
    std::array<std::array<double, 5>, 3> uValues = {
        {{{32.0, 22.0, 12.0, 8.0, 4.0}}, {{38.0, 28.0, 15.0, 9.0, 3.5}}, {{42.0, 30.0, 17.0, 10.0, 4.0}}}};

    for (atlas::idx_t i = 0; i < u.shape(0); ++i) {
        for (atlas::idx_t j = 0; j < u.shape(1); ++j) {
            zView(i, j) = zValues[i][j];
            uView(i, j) = uValues[i][j];
        }
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Check creation
    // -----------------------------------------------------------------------------------------------------------------
    std::shared_ptr<AtlasObservable> zPtr   = std::make_shared<AtlasObservable>(&geopotential);
    std::shared_ptr<AtlasObservable> uPtr   = std::make_shared<AtlasObservable>(&u);
    std::shared_ptr<AtlasObserver> u200Ptr  = std::make_shared<AtlasObserver>(u200tmp);
    std::shared_ptr<AtlasObserver> u5000Ptr = std::make_shared<AtlasObserver>(u5000tmp);
    EXPECT_NOT(u200Ptr->isUpdated());
    EXPECT_NOT(u5000Ptr->isUpdated());

    EXPECT_THROWS(plume::field_provider::WindAtHeight invalidStrategy(80001, zPtr, uPtr, u5000Ptr));
    plume::field_provider::WindAtHeight u200Strategy(200, zPtr, uPtr, u200Ptr);
    plume::field_provider::WindAtHeight u5000Strategy(5000, zPtr, uPtr, u5000Ptr);

    EXPECT_EQUAL(u200Ptr->get().levels(), 1);
    EXPECT_EQUAL(u5000Ptr->get().levels(), 1);
    EXPECT_EQUAL(u200Ptr->get().shape(), atlas::array::make_shape(3, 1));
    EXPECT_EQUAL(u5000Ptr->get().shape(), atlas::array::make_shape(3, 1));

    // -----------------------------------------------------------------------------------------------------------------
    // Check update correctness
    // -----------------------------------------------------------------------------------------------------------------
    EXPECT_NO_THROW(u200Strategy.update());
    EXPECT_NO_THROW(u5000Strategy.update());

    EXPECT(u200Ptr->isUpdated());
    EXPECT(u5000Ptr->isUpdated());

    auto u200View  = atlas::array::make_view<double, 2>(u200Ptr->get());
    auto u5000View = atlas::array::make_view<double, 2>(u5000Ptr->get());
    for (atlas::idx_t i = 0; i < u.shape(0); ++i) {
        EXPECT((u200View(i, 0) < uValues[i][3] && u200View(i, 0) > uValues[i][4]));
        EXPECT((u5000View(i, 0) < uValues[i][1] && u5000View(i, 0) > uValues[i][2]));
    }
    EXPECT(eckit::types::is_approximately_equal(u5000View(1, 0), 25.00, 0.01));

    // -----------------------------------------------------------------------------------------------------------------
    // Check correct behaviour to faulty parameters & field types
    // -----------------------------------------------------------------------------------------------------------------
    atlas::Field badType("bad_type", atlas::array::make_datatype<int>(), atlas::array::make_shape(3, 5));
    std::shared_ptr<AtlasObservable> badTypePtr = std::make_shared<AtlasObservable>(&badType);
    plume::field_provider::WindAtHeight badTypeStrategy(100, zPtr, badTypePtr, u200Ptr);
    EXPECT_THROWS(badTypeStrategy.update());

    zPtr.reset();
    EXPECT_THROWS(u200Strategy.update());
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}