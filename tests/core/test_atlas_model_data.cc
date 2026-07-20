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
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"

#include "plume/data/ModelData.h"

#include "atlas/array/ArrayShape.h"
#include "atlas/array/ArrayView.h"
#include "atlas/array/DataType.h"
#include "atlas/array/MakeView.h"
#include "atlas/field/Field.h"


using namespace eckit::testing;

namespace plume::field_provider {
/**
 * @class DummyAtlasStrategy
 * @brief A simple strategy populating the target Atlas field with the source values plus one.
 */
class DummyAtlasStrategy : public UpdateStrategy {
private:
    AtlasFieldObservablePtr source_;
    AtlasFieldObserverPtr target_;

public:
    DummyAtlasStrategy(AtlasFieldObservablePtr source, AtlasFieldObserverPtr target) :
        source_(source), target_(target) {}

    void update() override {
        auto source = source_.lock();
        auto target = target_.lock();
        ASSERT(source && target);

        auto sourceArr = atlas::array::make_view<int, 1>(source->get());
        auto targetArr = atlas::array::make_view<int, 1>(target->getSettableField());

        for (size_t i = 0; i < targetArr.shape(0); ++i) {
            targetArr(i) = sourceArr(i) + 1;
        }

        target->setUpdated(true);
    }
};

/**
 * @brief Specialisation of UpdateStrategyTraits for DummyAtlasStrategy.
 *
 * `requiredParams` is left empty because the this strategy does not impose a source name (it is dummy so it does not
 * require a source to be of a specific quantity, e.g. wind, time etc.) Other traits are ommitted because this strategy
 * should only be used for model data testing, not manager.
 *
 * @note This strategy only uses the source and target parameters, no other parameters or inputs from a configuration.
 */
template <>
struct UpdateStrategyTraits<DummyAtlasStrategy> {
    static constexpr const char* name = "atlas_dummy";
    static constexpr std::array<const char*, 0> configArgs{};
    static constexpr std::array<const char*, 0> paramArgs{};
    static constexpr std::array<std::array<const char*, 0>, 0> requiredParams{{}};
    using Args = std::tuple<AtlasFieldObservablePtr, AtlasFieldObserverPtr>;
};

}  // namespace plume::field_provider

namespace plume::test {

// separating these tests so they can be toggled off if atlas is disabled

CASE("test model data - atlas fields creation and provision") {

    plume::data::ModelData data;

    atlas::Field paramField("dummy", atlas::array::make_datatype<int>(), atlas::array::make_shape(1, 2, 3));
    atlas::Field ownedParamField("ownedDummy", atlas::array::make_datatype<int>(), atlas::array::make_shape(1, 2, 3));

    // Provide a model-owned field, then confirm it is retrievable as an atlas::Field but not as another type.
    data.provideParam("param-field", &paramField);
    EXPECT_THROWS(data.getParam<int>("param-field"));
    EXPECT_NO_THROW(data.getParam<atlas::Field>("param-field"));

    // Create a Plume-owned field. Direct creation is not the intended usage for Atlas fields (see the
    // observation test for the expected subscription pattern); this only checks the resulting entry type.
    data.createParam("owned-param-field", std::move(ownedParamField));
    EXPECT_THROWS(data.getParam<int>("owned-param-field"));
    EXPECT_NO_THROW(data.getParam<atlas::Field>("owned-param-field"));

    // updateParam mutates Plume-owned data only: it succeeds on the created field but is rejected on the
    // model-provided field, which Plume does not own.
    atlas::Field source("source", atlas::array::make_datatype<int>(), atlas::array::make_shape(1, 2, 3));
    EXPECT_NO_THROW(data.updateParam<atlas::Field>("owned-param-field", source));
    EXPECT_THROWS(data.updateParam<atlas::Field>("param-field", source));
}

CASE("test model data - observing atlas fields") {
    plume::data::ModelData data;
    data.registerStrategy<plume::field_provider::DummyAtlasStrategy>();

    std::vector<int> values{1, 2, 3, 4};
    atlas::Field observableField("observable", values.data(), atlas::array::make_shape(values.size()));

    eckit::LocalConfiguration paramConfig;
    paramConfig.set("name", "observable");
    paramConfig.set("type", "atlas_field");
    paramConfig.set("levtype", "dummy");
    paramConfig.set("level", "00");

    data.provideParam("observable", &observableField);
    EXPECT_NO_THROW(data.createParam<atlas::Field>("atlas_dummy", paramConfig));

    EXPECT(data.hasParameter("observable;dummy;00"));

    // Before any data change, the observer holds a clone of the source values.
    auto observerView   = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable;dummy;00"));
    auto observableView = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable"));
    EXPECT_EQUAL(observableView.shape(0), 4);
    EXPECT_EQUAL(observableView(0), 1);
    EXPECT_EQUAL(observerView.shape(0), 4);
    EXPECT_EQUAL(observerView(0), 1);
    EXPECT_EQUAL(observableView(3), 4);
    EXPECT_EQUAL(observerView(3), 4);

    // Mutate the source data and mark it updated to trigger the strategy on the observer.
    for (size_t i = 0; i < observableView.shape(0); ++i) {
        observableView(i) = 10 * i;
    }
    data.setUpdated({"observable"});
    EXPECT(data.isUpdated("observable;dummy;00"));

    // After the update, the observer reflects the new source values plus one (per DummyAtlasStrategy).
    observerView   = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable;dummy;00"));
    observableView = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable"));
    EXPECT_EQUAL(observableView(0), 0);
    EXPECT_EQUAL(observerView(0), 1);
    EXPECT_EQUAL(observableView(3), 30);
    EXPECT_EQUAL(observerView(3), 31);
}

/**
 * @brief Ensures a provided Atlas field remains valid after the caller's handle is destroyed.
 *
 * Reproduces the C-API code path of `plume_data_provide_atlas_field_shared`, where a temporary local `atlas::Field`
 * wrapper around the model's `Implementation` pointer is handed to `provideParam` and then destroyed on return.
 * This case pins the contract: a field provided must remain valid (no use-after-free) and carry its original data.
 */
CASE("test model data - atlas::Field provided via a temporary wrapper must not dangle") {
    plume::data::ModelData data;

    // Model-owned storage that outlives Plume, simulating model-managed memory.
    std::vector<int> values{11, 22, 33, 44};
    atlas::Field modelField("modelField", values.data(), atlas::array::make_shape(values.size()));

    // Mirror the C-API path: a local atlas::Field wrapping the model's Implementation pointer is provided
    // by address, then destroyed at the end of this scope.
    {
        atlas::Field wrapper(modelField.get());
        data.provideParam("provided", &wrapper);
    }

    // Poison the freed stack region so a dangling read cannot be masked by stale-but-intact stack bytes.
    volatile char scratch[512];
    for (int i = 0; i < 512; ++i) {
        scratch[i] = static_cast<char>(i);
    }

    // The provided field and its data must still be intact.
    atlas::Field got = data.getParam<atlas::Field>("provided");
    auto view        = atlas::array::make_view<int, 1>(got);
    EXPECT_EQUAL(view.shape(0), 4);
    EXPECT_EQUAL(view(0), 11);
    EXPECT_EQUAL(view(3), 44);
}

CASE("test model data - model-facing updateParam on Plume-owned atlas fields") {
    plume::data::ModelData data;

    // Create a Plume-owned field backed by its own atlas-allocated storage (independent of any external
    // buffer) and seed it with initial values. The field handle is deliberately destroyed at the end of this
    // scope: createParam takes the field by value and shares its reference-counted implementation, so the
    // parameter must keep the data alive on its own without the caller retaining any handle.
    {
        atlas::Field ownedInit("owned", atlas::array::make_datatype<int>(), atlas::array::make_shape(4));
        auto initView = atlas::array::make_view<int, 1>(ownedInit);
        for (int i = 0; i < 4; ++i) {
            initView(i) = i + 1;
        }
        data.createParam("owned", ownedInit);
    }

    // Capture the underlying implementation to assert the handle identity survives the update, and confirm
    // the field starts out carrying the initial values.
    atlas::Field before                            = data.getParam<atlas::Field>("owned");
    auto beforeView                                = atlas::array::make_view<int, 1>(before);
    const atlas::Field::Implementation* implBefore = before.get();
    EXPECT_EQUAL(beforeView(0), 1);
    EXPECT_EQUAL(beforeView(3), 4);

    // Build a source field carrying the new values and update the owned field in place.
    std::vector<int> updated{10, 20, 30, 40};
    atlas::Field source("source", updated.data(), atlas::array::make_shape(updated.size()));
    EXPECT_NO_THROW(data.updateParam<atlas::Field>("owned", source));

    // The data must reflect the update while the handle continues to point at the same implementation.
    atlas::Field after = data.getParam<atlas::Field>("owned");
    EXPECT(after.get() == implBefore);
    auto afterView = atlas::array::make_view<int, 1>(after);
    EXPECT_EQUAL(afterView(0), 10);
    EXPECT_EQUAL(afterView(3), 40);

    // A field the model merely provides is not owned by Plume and cannot be updated.
    std::vector<int> providedValues{5, 6, 7, 8};
    atlas::Field providedField("provided", providedValues.data(), atlas::array::make_shape(providedValues.size()));
    data.provideParam("provided", &providedField);
    EXPECT_THROWS(data.updateParam<atlas::Field>("provided", source));

    // A source field whose shape does not match the owned field is rejected.
    atlas::Field wrongShape("wrong", atlas::array::make_datatype<int>(), atlas::array::make_shape(2));
    EXPECT_THROWS(data.updateParam<atlas::Field>("owned", wrongShape));
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}