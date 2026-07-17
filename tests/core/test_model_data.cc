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
#include <array>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"

#include "plume/data/FieldProvider.h"
#include "plume/data/ModelData.h"
#include "plume/data/ParameterValue.h"

using namespace eckit::testing;

namespace plume::field_provider {
/**
 * @class DummyStrategy
 * @brief A simple strategy setting the target int to the source int plus one.
 */
class DummyStrategy : public UpdateStrategy {
private:
    IntObservablePtr source_;
    IntObserverPtr target_;

public:
    DummyStrategy(IntObservablePtr source, IntObserverPtr target) : source_(source), target_(target) {}

    void update() override {
        auto source = source_.lock();
        auto target = target_.lock();
        ASSERT(source && target);

        int newVal = source->get() + 1;
        target->set(newVal);

        target->setUpdated(true);
    }
};

/**
 * @brief Specialisation of UpdateStrategyTraits for DummyStrategy.
 *
 * `requiredParams` is left empty because the this strategy does not impose a source name (it is dummy so it does not
 * require a source to be of a specific quantity, e.g. wind, time etc.) Other traits are ommitted because this strategy
 * should only be used for model data testing, not manager.
 *
 * @note This strategy only uses the source and target parameters, no other parameters or inputs from a configuration.
 */
template <>
struct UpdateStrategyTraits<DummyStrategy> {
    static constexpr const char* name = "dummy";
    static constexpr std::array<const char*, 0> configArgs{};
    static constexpr std::array<const char*, 0> paramArgs{};
    static constexpr std::array<std::array<const char*, 0>, 0> requiredParams{{}};
    using Args = std::tuple<IntObservablePtr, IntObserverPtr>;
};

}  // namespace plume::field_provider

namespace plume::test {


CASE("test model data - type checks") {

    plume::data::ModelData data;

    // --------- test type checking ---------
    int paramInt            = 1;
    bool paramBool          = false;
    float paramFloat        = 1.111;
    double paramDouble      = 2.222;
    std::string paramString = "some-text";

    // int param
    data.provideParam("param-i", &paramInt);

    EXPECT_NO_THROW(data.getParam<int>("param-i"));
    EXPECT_THROWS(data.getParam<bool>("param-i"));
    EXPECT_THROWS(data.getParam<float>("param-i"));
    EXPECT_THROWS(data.getParam<double>("param-i"));
    EXPECT_THROWS(data.getParam<std::string>("param-i"));

    // bool param
    data.provideParam("param-b", &paramBool);
    EXPECT_THROWS(data.getParam<int>("param-b"));
    EXPECT_NO_THROW(data.getParam<bool>("param-b"));
    EXPECT_THROWS(data.getParam<float>("param-b"));
    EXPECT_THROWS(data.getParam<double>("param-b"));
    EXPECT_THROWS(data.getParam<std::string>("param-b"));

    // float param
    data.provideParam("param-f", &paramFloat);
    EXPECT_THROWS(data.getParam<int>("param-f"));
    EXPECT_THROWS(data.getParam<bool>("param-f"));
    EXPECT_NO_THROW(data.getParam<float>("param-f"));
    EXPECT_THROWS(data.getParam<double>("param-f"));
    EXPECT_THROWS(data.getParam<std::string>("param-f"));

    // double param
    data.provideParam("param-d", &paramDouble);
    EXPECT_THROWS(data.getParam<int>("param-d"));
    EXPECT_THROWS(data.getParam<bool>("param-d"));
    EXPECT_THROWS(data.getParam<float>("param-d"));
    EXPECT_NO_THROW(data.getParam<double>("param-d"));
    EXPECT_THROWS(data.getParam<std::string>("param-d"));

    // string param
    data.provideParam("param-s", &paramString);
    EXPECT_THROWS(data.getParam<int>("param-s"));
    EXPECT_THROWS(data.getParam<bool>("param-s"));
    EXPECT_THROWS(data.getParam<float>("param-s"));
    EXPECT_THROWS(data.getParam<double>("param-s"));
    EXPECT_NO_THROW(data.getParam<std::string>("param-s"));
}


CASE("test model data - type checks 2") {

    plume::data::ModelData data;

    EXPECT_THROWS(data.getParam<std::string>("not-existant-key"));
    EXPECT_THROWS(data.getParam<atlas::Field>("not-existant-field"));
}

CASE("test model data - owned params") {

    plume::data::ModelData data;

    int paramInt            = 0;
    bool paramBool          = false;
    float paramFloat        = 1.111;
    double paramDouble      = 2.222;
    std::string paramString = "some-text";

    data.createParam("param-i", paramInt);
    EXPECT_EQUAL(data.getParam<int>("param-i"), 0);
    EXPECT_THROWS(data.updateParam("param-i", 1.0f));
    EXPECT_NO_THROW(data.updateParam("param-i", 1));
    EXPECT_EQUAL(data.getParam<int>("param-i"), 1);

    data.createParam("param-b", paramBool);
    EXPECT_NOT(data.getParam<bool>("param-b"));
    data.updateParam("param-b", true);
    EXPECT(data.getParam<bool>("param-b"));

    data.createParam("param-f", paramFloat);
    EXPECT(std::abs(data.getParam<float>("param-f") - 1.111) < 0.0001);
    data.updateParam("param-f", 10.111f);
    EXPECT(std::abs(data.getParam<float>("param-f") - 10.111) < 0.0001);

    data.createParam("param-d", paramDouble);
    EXPECT(std::abs(data.getParam<double>("param-d") - 2.222) < 0.0001);
    data.updateParam("param-d", 20.222);
    EXPECT(std::abs(data.getParam<double>("param-d") - 20.222) < 0.0001);

    data.createParam("param-s", paramString);
    EXPECT_EQUAL(data.getParam<std::string>("param-s"), "some-text");
    data.updateParam("param-s", std::string{"updated-text"});
    EXPECT_EQUAL(data.getParam<std::string>("param-s"), "updated-text");
}

CASE("test model data - update status") {

    plume::data::ModelData data;

    int paramA = 1;
    int paramB = 2;
    data.provideParam("paramA", &paramA);
    data.provideParam("paramB", &paramB);

    EXPECT_NOT(data.isUpdated("paramA"));  // Params are initialised *not* updated

    data.setUpdated({"paramA"});

    EXPECT(data.isUpdated("paramA"));  // Only request param is set as updated
    EXPECT_NOT(data.isUpdated("paramB"));

    data.setUpdated({"paramB"});

    EXPECT_NOT(data.isUpdated("paramA"));  // setting another param as updated resets other params status to not updated
    EXPECT(data.isUpdated("paramB"));

    data.clearUpdated();

    EXPECT_NOT(data.isUpdated("paramA"));  // clearing resets all params updated status
    EXPECT_NOT(data.isUpdated("paramB"));

    data.setUpdated({"paramA", "paramB"});

    EXPECT(data.isUpdated("paramA"));  // all requested params are set as updated
    EXPECT(data.isUpdated("paramB"));
}

CASE("test model data - observing params") {
    plume::data::ModelData data;

    data.registerStrategy<plume::field_provider::DummyStrategy>();

    int observableParam   = 1;
    int unobservableParam = -1;

    // See `test_plugin_params.cc` for examples of parameter configurations
    eckit::LocalConfiguration paramConfig;
    paramConfig.set("name", "unobservable");
    paramConfig.set("type", "int");
    paramConfig.set("levtype", "dummy");
    paramConfig.set("level", "00");

    data.provideParam("observable", &observableParam);
    data.createParam("unobservable", unobservableParam);

    // At the moment Plume does not allow chains of observers
    EXPECT_THROWS(data.createParam<int>("dummy", paramConfig));

    paramConfig.set("name", "observable");  // typically 'u' or 'v' for derived wind fields
    EXPECT_NO_THROW(data.createParam<int>("dummy", paramConfig));

    EXPECT(data.hasParameter("observable;dummy;00"));   // the correct default name is applied to derived param
    EXPECT_NOT(data.isUpdated("observable;dummy;00"));  // the newly created param is not marked as updated

    EXPECT_EQUAL(data.getParam<int>("observable"), 1);
    EXPECT_EQUAL(data.getParam<int>("observable;dummy;00"), 0);  // zero-initialised; strategy not yet run
    EXPECT_EQUAL(data.getParam<int>("observable", "00", "dummy"), 0);  // zero-initialised; strategy not yet run
    observableParam = 5;
    data.setUpdated({"observable"});
    EXPECT(data.isUpdated("observable;dummy;00"));
    EXPECT(data.isUpdated("observable", "00", "dummy"));
    EXPECT_EQUAL(data.getParam<int>("observable"), 5);
    EXPECT_EQUAL(data.getParam<int>("observable;dummy;00"), 6);

    // owning params that actively observe should not allow manual updates
    EXPECT_THROWS(data.updateParam("observable;dummy;00", 3));
}

CASE("filter_subsets_params_and_sets_consumer") {
    data::ModelData data;
    data.createParam<int>("x", 1);
    data.createParam<int>("y", 2);
    data.createParam<int>("z", 3);

    // Model-facing instance has no consumer
    EXPECT(data.consumer().empty());

    // filter with consumer tag
    auto view = data.filter(std::set<std::string>{"x", "z"}, "PluginA");
    EXPECT(view.hasParameter("x"));
    EXPECT(view.hasParameter("z"));
    EXPECT_NOT(view.hasParameter("y"));
    EXPECT_EQUAL(view.consumer(), std::string("PluginA"));

    // filter without consumer tag — consumer stays empty
    auto view2 = data.filter(std::set<std::string>{"y"});
    EXPECT(view2.hasParameter("y"));
    EXPECT(view2.consumer().empty());
}

//----------------------------------------------------------------------------------------------------------------------

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}