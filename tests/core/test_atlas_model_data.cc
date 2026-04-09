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

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"

#include "plume/data/ModelData.h"

#include "atlas/array/ArrayShape.h"
#include "atlas/array/DataType.h"
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
CASE("test model data - atlas fields checks") {

    plume::data::ModelData data;

    atlas::Field paramField("dummy", atlas::array::make_datatype<int>(), atlas::array::make_shape(1, 2, 3));
    atlas::Field ownedParamField("ownedDummy", atlas::array::make_datatype<int>(), atlas::array::make_shape(1, 2, 3));

    data.provideParam("param-field", &paramField);  /// checking field provision
    EXPECT_THROWS(data.getParam<int>("param-field"));
    EXPECT_NO_THROW(data.getParam<atlas::Field>("param-field"));

    /// checking field creation - although not directly usable for Atlas fields, see observation test for expected use
    data.createParam("owned-param-field", std::move(ownedParamField));
    EXPECT_THROWS(data.getParam<int>("owned-param-field"));
    EXPECT_NO_THROW(data.getParam<atlas::Field>("owned-param-field"));
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

    auto oberserverView   = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable;dummy;00"));
    auto oberservableView = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable"));
    EXPECT_EQUAL(oberservableView.shape(0), 4);
    EXPECT_EQUAL(oberservableView(0), 1);
    EXPECT_EQUAL(oberserverView.shape(0), 4);
    EXPECT_EQUAL(oberserverView(0), 2);
    EXPECT_EQUAL(oberservableView(3), 4);
    EXPECT_EQUAL(oberserverView(3), 5);

    for (size_t i = 0; i < oberservableView.shape(0); ++i) {
        oberservableView(i) = 10 * i;
    }
    data.setUpdated({"observable"});
    EXPECT(data.isUpdated("observable;dummy;00"));

    oberserverView   = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable;dummy;00"));
    oberservableView = atlas::array::make_view<int, 1>(data.getParam<atlas::Field>("observable"));
    EXPECT_EQUAL(oberservableView(0), 0);
    EXPECT_EQUAL(oberserverView(0), 1);
    EXPECT_EQUAL(oberservableView(3), 30);
    EXPECT_EQUAL(oberserverView(3), 31);
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}