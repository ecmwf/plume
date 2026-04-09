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
#include "atlas/array.h"

#include "plume/data/FieldProvider.h"
#include "plume/data/ParameterValue.h"

namespace plume {
namespace field_provider {

std::tuple<std::string, std::vector<std::string>, std::string, std::string> findMatchingStrategy(
    const std::string& source, const eckit::Configuration& config) {
    std::string strategyName;
    std::vector<std::string> requiredParams;
    std::string levtype;
    std::string levelKey;

    std::apply(
        [&](auto... strategyTrait) {
            (([&] {
                 if (!strategyName.empty())
                     return;  // already matched

                 auto [match, requiredParamsArr] = matchesStrategyTraitsImpl<decltype(strategyTrait)>(source, config);
                 if (match) {
                     strategyName   = decltype(strategyTrait)::name;
                     requiredParams = std::move(requiredParamsArr);
                     levtype        = decltype(strategyTrait)::levtype;
                     levelKey       = decltype(strategyTrait)::levelKey;
                 }
             }()),
             ...);
        },
        AllUpdateStrategyTraits{});

    return {strategyName, requiredParams, levtype, levelKey};
}

// ---------------------------------------------------------------------------------------------------------------------
// WindAtHeight strategy
// ---------------------------------------------------------------------------------------------------------------------
WindAtHeight::WindAtHeight(std::size_t height, AtlasFieldObservablePtr geopotential,
                           AtlasFieldObservablePtr windComponent, AtlasFieldObserverPtr windAtHeight) :
    height_(height),
    z_(height_ * gravityConstant_),
    geopotential_(geopotential),
    windComponent_(windComponent),
    windAtHeight_(windAtHeight) {
    if (height_ > 80000) {  // https://confluence.ecmwf.int/display/UDOC/L137+model+level+definitions
        throw eckit::BadValue("Wind cannot be interpolated at heights higher than 80km", Here());
    }

    // swap the target field array (which is a clone of the source) for a 2D array
    auto target = windAtHeight_.lock();

    ASSERT(target);

    const auto& field3d = target->get();
    atlas::Field field2d(field3d.name(), field3d.datatype(), atlas::array::make_shape(field3d.shape(0), 1));
    field2d.metadata() = field3d.metadata();
    field2d.set_levels(1);
    field2d.set_functionspace(field3d.functionspace());
    target->set(field2d);
}

void WindAtHeight::update() {
    auto geopotentialField = geopotential_.lock();
    auto windField         = windComponent_.lock();
    auto windAtHeightField = windAtHeight_.lock();

    ASSERT(geopotentialField && windField && windAtHeightField);

    const auto dt = windField->get().datatype();

    // -----------------------------------------------------------------------------------------------------------------
    // Wind interpolation happens here
    // -----------------------------------------------------------------------------------------------------------------
    auto interpolate = [&](auto make_view_t) {
        using FIELD_TYPE_REAL = decltype(make_view_t);

        auto geopotential = atlas::array::make_view<FIELD_TYPE_REAL, 2>(geopotentialField->get());
        auto wind         = atlas::array::make_view<FIELD_TYPE_REAL, 2>(windField->get());
        auto windAtHeight = atlas::array::make_view<FIELD_TYPE_REAL, 2>(windAtHeightField->getSettableField());

        for (size_t i = 0; i < windAtHeight.shape(0); ++i) {
            bool found = false;
            for (size_t lev = wind.shape(1) - 1; lev > 0; --lev) {
                if (z_ < geopotential(i, lev - 1) && z_ >= geopotential(i, lev)) {
                    windAtHeight(i, 0) = (wind(i, lev - 1) * (z_ - geopotential(i, lev)) +
                                          wind(i, lev) * (geopotential(i, lev - 1) - z_)) /
                                         (geopotential(i, lev - 1) - geopotential(i, lev));
                    found = true;
                    break;
                }
            }
            if (!found) {
                // when z_ is > Z[0] or < Z[N] there is a malformation that should be handled by the user
                throw eckit::BadValue(
                    "Wind interpolation failed: height not within geopotential range",
                    Here());
            }
        }
    };
    // -----------------------------------------------------------------------------------------------------------------

    // Runtime dispatch: no need to compile Plume in both DP/SP
    if (dt == atlas::array::DataType::real32()) {
        interpolate(float{});  // float version
    }
    else if (dt == atlas::array::DataType::real64()) {
        interpolate(double{});  // double version
    }
    else {
        throw eckit::BadValue("Unsupported wind field value type (expected float or double)");
    }

    windAtHeightField->setUpdated(true);
}
// ---------------------------------------------------------------------------------------------------------------------

}  // namespace field_provider
}  // namespace plume