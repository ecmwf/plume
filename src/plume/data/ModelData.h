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
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"
#include "eckit/value/Value.h"

#include "atlas/field/Field.h"
#include "atlas/field/detail/FieldImpl.h"

#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterType.h"
#include "plume/data/ParameterValue.h"


namespace plume {
namespace data {


// Container class for Values and pointers
class ModelData {
private:
    // Values & Strategies
    std::map<std::string, std::shared_ptr<IParameterValue>> valueMap_;
    std::unordered_map<std::string, std::function<std::unique_ptr<field_provider::UpdateStrategy>(
                                        const field_provider::StrategyArgList&)>>
        strategyRegistry_;
    std::unordered_map<std::string,
                       std::function<field_provider::StrategyArgList(
                           const eckit::Configuration&, const std::map<std::string, std::shared_ptr<IParameterValue>>&,
                           const std::string&, const std::string&)>>
        strategyHelpers_;

    /**
     * @brief Returns the names of all parameters in the value map.
     */
    std::vector<std::string> getAvailableValues() const;

    /**
     * @brief Constructs a concrete strategy but does not attach it yet to a parameter.
     *
     * @param type The identifier of the strategy to create (key in the strategy registry).
     * @param args The vector of constructor arguments required for this strategy.
     *
     * @return A `std::unique_ptr` to the strategy which ownership can then be transferred to an observer parameter.
     */
    std::unique_ptr<field_provider::UpdateStrategy> createStrategy(const std::string& type,
                                                                   const field_provider::StrategyArgList& args);
    /**
     * @brief Creates a dependency between an owned and an observed parameter.
     *
     * The observer will update its parameter value when the observable notifies a change. The update mechanism
     * depends on the strategy provided. See implementation for break down of steps.
     */
    void addDependency(const std::string& observer, const std::string& observable, const std::string& strategyName,
                       const eckit::Configuration& config);

public:
    ModelData();

    ~ModelData() = default;  // Nothing to do here (each parameter destructs its data pointer, as appropriate..)

    /**
     * @brief Creates a new value of type T, and transfer its ownership to a parameter wrapper.
     *
     * @note Atlas fields can be created using this method from the C++ API, but there is no support for updating them
     *       directly at the moment. Use the `createParam` method with the subscription pattern instead.
     * @todo The C API uses an Atlas internal type for field provision and access. Only the `Field` type should be used
     *       by clients like Plume, so field implementation use should be removed.
     */
    template <typename T>
    void createParam(std::string name, T valInit) {
        static_assert(!std::is_base_of_v<atlas::Field::Implementation, T>,
                      "Atlas field implementations are only for observation");
        auto res = valueMap_.try_emplace(name, std::make_shared<ParameterValue<T, IParameterObserver>>(valInit));
        if (!res.second) {
            eckit::Log::warning() << "Parameter '" << name << "' already in Model Data. Not inserted!" << std::endl;
        }
    }

    /**
     * @brief Dispatch method `createParam<T>` method for manager to create parameters based on configured type.
     */
    void dispatchCreateParam(const std::string& strategy, const eckit::Configuration& config);

    /**
     * @brief Creates a new value as above, and subscribe it to one of the publisher parameters.
     *
     * Publisher parameters are added to the Model Data through `provideParam`.
     *
     * @warning The new value can only be successfully created after the publisher and other sources required by the
     *          update strategy have been provided to the Model Data.
     */
    template <typename T>
    void createParam(const std::string& strategy, const eckit::Configuration& config, std::string name = "") {
        static_assert(!std::is_base_of_v<atlas::Field::Implementation, T>,
                      "Atlas field implementations are only for observation");
        // 1. name the param entry using defaults or user instructions
        std::string paramName = name;
        if (paramName.empty()) {
            paramName = IParameterObserver::deriveParamName(config.getString("name"), config.getString("levtype", "hl"),
                                                            config.getString("level"));
        }
        if (hasParameter(paramName)) {
            eckit::Log::warning() << "Parameter '" << paramName << "' already in Model Data. Not inserted!"
                                  << std::endl;
            return;
        }
        // 2. create the observing value valInit (default constructor or clone source field)
        // 3. create the param value and insert it in the map
        if constexpr (std::is_same_v<T, atlas::Field>) {
            atlas::Field fieldInit = getParam<atlas::Field>(config.getString("name")).clone();
            fieldInit.rename(paramName);
            fieldInit.metadata().set("plume-owned", true);
            valueMap_.try_emplace(paramName,
                                  std::make_shared<ParameterValue<atlas::Field, IParameterObserver>>(fieldInit));
        }
        else {
            T valInit{};
            valueMap_.try_emplace(paramName, std::make_shared<ParameterValue<T, IParameterObserver>>(valInit));
        }
        // 4. subscribe the newly created param to the source param & compute its initial value
        addDependency(paramName, config.getString("name"), strategy, config);
    }

    /**
     * @brief Provides an observation-only pointer to an existing value managed by the model.
     *
     * @warning The value should outlive Plume to avoid undefined behaviour.
     */
    template <typename T>
    void provideParam(std::string name, T* ptr) {
        if constexpr (std::is_same_v<T, atlas::Field> || std::is_same_v<T, atlas::Field::Implementation>) {
            ASSERT_MSG(ptr->bytes() >= 0, "Provided Atlas field not readable!");
        }
        auto res = valueMap_.try_emplace(name, std::make_shared<ParameterValue<T, IParameterObservable>>(ptr));
        if (!res.second) {
            eckit::Log::warning() << "Parameter '" << name << "' already in Model Data. Not inserted!" << std::endl;
        }
    }

    /**
     * @brief Update the value of a created parameter, i.e., of an owned parameter that is not an Atlas field.
     *
     * @note Method not implemented for Atlas fields, which can only be updated through the observation pattern for now.
     */
    template <typename T>
    void updateParam(std::string name, T newVal) {
        if (!hasParameter(name)) {
            throw eckit::BadParameter("Parameter '" + name + "' not found in model data!", Here());
        }
        if (auto typedPtr = std::dynamic_pointer_cast<IParameterObserver>(valueMap_.at(name))) {
            if (typedPtr->observes()) {
                throw eckit::AssertionFailed("Observer parameters actively observing can only be updated by strategies",
                                             Here());
            }
        }
        if (auto typedPtr = std::dynamic_pointer_cast<ParameterValueTyped<T>>(valueMap_.at(name))) {
            typedPtr->set(newVal);
        }
        else {
            throw eckit::BadCast("Plume parameter update type mismatch!", Here());
        }
    }

    /**
     * @brief Accesses a value of a parameter. Intended for data users to "update" their local view of the model data.
     *
     * @note This interface can be used for source & derived params if the full name is known.
     */
    template <typename T, typename = std::enable_if_t<!std::is_same<T, atlas::Field::Implementation>::value>>
    T getParam(std::string name) const {
        if (!hasParameter(name)) {
            throw eckit::BadParameter("Parameter '" + name + "' not found in model data!", Here());
        }
        if (auto typedPtr = std::dynamic_pointer_cast<ParameterValueTyped<T>>(valueMap_.at(name))) {
            return typedPtr->get();
        }
        if constexpr (std::is_same_v<T, atlas::Field>) {
            if (auto typedPtr =
                    std::dynamic_pointer_cast<ParameterValueTyped<atlas::Field::Implementation>>(valueMap_.at(name))) {
                return atlas::Field(&(typedPtr->get()));
            }
        }
        throw eckit::BadCast("Plume parameter view update type mismatch!", Here());
    }

    /**
     * @brief Accesses a value of a derived parameter from its source parameter name, levtype and level.
     *
     * @note This signature opens to levtype in case other levtypes, such as 'pl', are introduced in the future.
     */
    template <typename T>
    T getParam(const std::string& name, const std::string& level, const std::string& levtype = "hl") const {
        std::string entryName = IParameterObserver::deriveParamName(name, levtype, level);
        return getParam<T>(entryName);
    }

    // Return a subset of the ModelData
    ModelData filter(std::set<std::string> params) const;

    // Return a subset of the ModelData
    ModelData filter(ParameterCatalogue params) const;

    // check if a parameter is in the data
    bool hasParameter(const std::string& name) const;
    bool hasParameter(const std::string& name, const std::string& level, const std::string& levtype = "hl") const;
    bool hasParameter(const std::string& name, const ParameterType& type) const;

    // Manage parameters updated state
    bool isUpdated(const std::string& name) const;  // for plugins to query
    bool isUpdated(const std::string& name, const std::string& level, const std::string& levtype = "hl") const;
    void setUpdated(const std::vector<std::string>& params);  // for data providers
    void clearUpdated();                                      // for data providers or Plume manager to clear after run

    // list available parameters of a certain type
    std::vector<std::string> listAvailableParameters(std::string type_string) const;

    /// Add a concrete strategy factory to the registries to allow creation of various dependencies between parameters.
    template <typename Strategy>
    void registerStrategy() {
        field_provider::AutoRegister<Strategy> entry{strategyRegistry_, strategyHelpers_};
    }

    void print() const;
};

}  // namespace data
}  // namespace plume