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
#pragma once
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "atlas/field/Field.h"

#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"

namespace plume {

namespace data {
// forward declarations
template <typename, typename>
class ParameterValue;

class IParameterObservable;
class IParameterObserver;
class IParameterValue;
}  // namespace data

namespace field_provider {

using AtlasFieldObservablePtr = std::weak_ptr<data::ParameterValue<atlas::Field, data::IParameterObservable>>;
using AtlasFieldObserverPtr   = std::weak_ptr<data::ParameterValue<atlas::Field, data::IParameterObserver>>;
using IntObservablePtr        = std::weak_ptr<data::ParameterValue<int, data::IParameterObservable>>;
using IntObserverPtr          = std::weak_ptr<data::ParameterValue<int, data::IParameterObserver>>;

/**
 * @class UpdateStrategy
 * @brief Base class for observer owned parameters update protocol.
 */
class UpdateStrategy {
public:
    virtual ~UpdateStrategy() = default;

    /// Applies the update method to the owned parameter the strategy is attached to.
    virtual void update() = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// Concrete strategies
// ---------------------------------------------------------------------------------------------------------------------

/**
 * @class WindAtHeight
 * @brief Update strategy that populates the target field with the wind component at a given height.
 */
class WindAtHeight : public UpdateStrategy {
private:
    std::size_t height_;
    float gravityConstant_ = 9.80665f;
    float z_;  ///< the potential at height, using gravity scaling factor of 1

    /// Source fields, we cannot write accidentally because these params are not owned
    AtlasFieldObservablePtr geopotential_;
    AtlasFieldObservablePtr windComponent_;

    /// Owned field to update
    AtlasFieldObserverPtr windAtHeight_;

public:
    /**
     * @brief Constructs a wind at given height strategy.
     *
     * Computes the potential at the given height (`z = m * g`), performs some validation.
     * The order of arguments is important. It should match what is set in the strategy type trait, and apply the
     * following order: 1) config args, 2) model data param args, 3) observable, 4) observer.
     */
    WindAtHeight(std::size_t height, AtlasFieldObservablePtr geopotential, AtlasFieldObservablePtr windComponent,
                 AtlasFieldObserverPtr windAtHeight);

    /**
     * @brief Interpolates values of the source wind component to the given height and writes them to target field.
     *
     * Scans the observable (3D), and modifies the observer (2D) (wind components):
     * 1. Lock sources and target.
     * 2. Get atlas array views of geopotential, and wind components (source and target).
     * 3. For each grid point, loop through the levels to interpolate the new wind value, and set it in the target
     * field.
     * 4. Mark the windAtHeight_ field parameter as updated.
     */
    void update() override;
};

// ---------------------------------------------------------------------------------------------------------------------
// Strategy type traits
// ---------------------------------------------------------------------------------------------------------------------

/**
 * @brief Traits class template for update strategies.
 *
 * This primary template should be specialised for each strategy type.
 * It provides compile-time information about how to build a strategy, such as its name and constructor argument types.
 * Each specialisation should at least have the following information:
 * - `name` gives the string identifier of the strategy.
 * - `levtype` Only used for naming purposes to give users an idea of the strategy. Might not always be relevant.
 * - `levelKey` The config key where the level can be found.
 * - `configArgs` is an array of keys to retrieve from an eckit configuration.
 * - `paramArgs` is an array of param names to retrieve from the model data, except for the observable and observer.
 * - `requiredParams` is an array of valid combinations of required params that can be used by the negotiator.
 * - `Args` is a tuple of the argument types expected by the strategy constructor. As highlighted in the WindAtHeight
 *    strategy ctor docstring, the order is important.
 *
 * @tparam T The strategy type to provide traits for.
 *
 * @note This primary template provides a reference of all the configuration options that users can provide to request
 *       a derived parameter. This reference can be used by the manager for the negotiation phase.
 */
template <typename T>
struct UpdateStrategyTraits {
    static constexpr std::array<const char*, 1> allConfigArgs{"height"};
};

template <>
struct UpdateStrategyTraits<WindAtHeight> {
    static constexpr const char* name     = "wind_at_height";
    static constexpr const char* levtype  = "hl";
    static constexpr const char* levelKey = "height";
    static constexpr std::array<const char*, 1> configArgs{"height"};
    static constexpr std::array<const char*, 1> paramArgs{"z"};
    static constexpr std::array<std::array<const char*, 2>, 2> requiredParams{{{"u", "z"}, {"v", "z"}}};
    using Args = std::tuple<std::size_t, AtlasFieldObservablePtr, AtlasFieldObservablePtr, AtlasFieldObserverPtr>;
};

// ---------------------------------------------------------------------------------------------------------------------
// Strategy registry
// ---------------------------------------------------------------------------------------------------------------------
// Add types as needed
using StrategyArgs =
    std::variant<std::size_t, AtlasFieldObservablePtr, AtlasFieldObserverPtr, IntObservablePtr, IntObserverPtr>;
using StrategyArgList = std::vector<StrategyArgs>;

/**
 * @brief Constructs a tuple from a runtime argument list using an index sequence.
 *
 * Internal helper that expands an index sequence to extract and convert elements from a StrategyArgList into a tuple of
 * type `Tuple`. Each element `I` of the tuple is constructed by retrieving the value stored at `args[I]` and
 * converting it to `std::tuple_element_t<I, Tuple>`.
 *
 * @tparam Tuple The tuple type to construct.
 * @tparam I Compile-time indices used to expand the tuple elements.
 * @param args Runtime list of arguments.
 * @return A `Tuple` constructed from the corresponding elements in `args`.
 *
 * @note This function is an implementation detail and should not be called directly. Use `tupleFromArgs()` instead.
 * @warning Behavior is undefined if an element cannot be converted to the corresponding tuple type.
 */
template <typename Tuple, std::size_t... I>
Tuple tupleFromArgsImpl(const StrategyArgList& args, std::index_sequence<I...>) {
    return Tuple{std::get<std::tuple_element_t<I, Tuple>>(args[I])...};
}

/**
 * @brief Constructs a tuple from a runtime argument list.
 *
 * Creates a tuple of type `Tuple` by extracting and converting values from the provided StrategyArgList.
 * The number and types of elements are determined entirely by `Tuple`.
 *
 * @see tupleFromArgsImpl
 */
template <typename Tuple>
Tuple tupleFromArgs(const StrategyArgList& args) {
    return tupleFromArgsImpl<Tuple>(args, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

/**
 * @brief Wraps the call to retrieve the eckit configuration value at the given key.
 *
 * @tparam T The expected type of the configuration value.
 *
 * @throws eckit::ConfigurationNotFound If the key is not found in the configuration.
 * @throws eckit::BadCast If the value cannot be cast to `T`.
 * @throws std::exception If `T` is not default-constructible and default construction fails.
 */
template <typename T>
T getConfigValue(const eckit::Configuration& config, const char* key) {
    T value{};  // we should not have atlas fields there as it's only eckit config options
    config.get(key, value);
    return value;
}

/**
 * @brief Retrieves a weak pointer to a parameter stored in a model data value map.
 *
 * @tparam T A `std::weak_ptr` pointer type whose `element_type` denotes the expected concrete parameter type.
 *
 * @throws std::out_of_range If `key` does not exist in `paramMap`.
 * @throws eckit::BadCast If the parameter associated with `key` cannot be dynamically cast to `T::element_type`.
 */
template <typename T>
T getParamValue(const std::map<std::string, std::shared_ptr<data::IParameterValue>>& paramMap, const char* key) {
    using ParamType = typename T::element_type;
    auto basePtr    = paramMap.at(key);
    auto derivedPtr = std::dynamic_pointer_cast<ParamType>(basePtr);
    if (!derivedPtr) {
        throw eckit::BadCast("Strategy arg type for key: " + std::string(key) + " does not match model data type.",
                             Here());
    }
    return T{derivedPtr};
}

/**
 * @brief Constructs an argument tuple for strategy construction from configuration values and model data parameters.
 *
 * @tparam StrategyTraits A concrete strategy traits type defining members documented in `UpdateStrategyTraits`.
 * @tparam CI Compile-time indices selecting configuration-derived arguments.
 * @tparam PI Compile-time indices selecting parameter-derived arguments.
 *
 * @return An instance of `StrategyTraits::Args` populated with configuration and parameter values in the order
 *         defined by `StrategyTraits::Args`.
 *
 * @note This function is an implementation detail and is intended to be invoked only by higher-level argument
 *       construction utilities. No validation is performed beyond what is enforced by the underlying accessors.
 */
template <typename StrategyTraits, std::size_t... CI, std::size_t... PI>
auto makeStrategyArgsImpl(const eckit::Configuration& config,
                          const std::map<std::string, std::shared_ptr<data::IParameterValue>>& paramMap,
                          const std::array<const char*, sizeof...(PI)>& allParams, std::index_sequence<CI...>,
                          std::index_sequence<PI...>) {
    using Args = typename StrategyTraits::Args;

    return Args{
        // Config args
        getConfigValue<std::tuple_element_t<CI, Args>>(config, StrategyTraits::configArgs[CI])...,
        // Param args
        getParamValue<std::tuple_element_t<sizeof...(CI) + PI, Args>>(paramMap, allParams[PI])...,
    };
}

/**
 * @brief Automatically registers a strategy and its argument builder.
 *
 * Helper type that, upon construction, registers a concrete strategy type in the provided strategy factory
 * and associates it with a function that builds its runtime argument list.
 *
 * @tparam Strategy Concrete strategy type to register.
 */
template <typename Strategy>
struct AutoRegister {
    /**
     * @brief Registers the strategy factory and constructor argument helper.
     *
     * @param registry   Map associating strategy names with construction functions.
     * @param argHelpers Map associating strategy names with argument builder functions.
     */
    AutoRegister(std::unordered_map<std::string,
                                    std::function<std::unique_ptr<UpdateStrategy>(const StrategyArgList&)>>& registry,
                 std::unordered_map<
                     std::string,
                     std::function<StrategyArgList(const eckit::Configuration&,
                                                   const std::map<std::string, std::shared_ptr<data::IParameterValue>>&,
                                                   const std::string&, const std::string&)>>& argHelpers) {
        using ArgsTuple         = typename UpdateStrategyTraits<Strategy>::Args;
        constexpr std::size_t N = std::tuple_size<ArgsTuple>::value;

        // 1. Insert factory function for strategy construction
        registry.emplace(UpdateStrategyTraits<Strategy>::name,
                         [](const StrategyArgList& args) -> std::unique_ptr<UpdateStrategy> {
                             if (args.size() != N)
                                 throw eckit::BadParameter("Invalid argument count for strategy", Here());

                             ArgsTuple extracted = tupleFromArgs<ArgsTuple>(args);

                             return std::apply(
                                 [](auto&&... unpacked) { return std::make_unique<Strategy>(unpacked...); }, extracted);
                         });

        // 2. Insert factory helper that assembles config and params into construction-ready argument vector
        argHelpers.emplace(
            UpdateStrategyTraits<Strategy>::name,
            [](const eckit::Configuration& config,
               const std::map<std::string, std::shared_ptr<data::IParameterValue>>& paramMap,
               const std::string& observable, const std::string& observer) -> StrategyArgList {
                constexpr std::size_t numConfig = UpdateStrategyTraits<Strategy>::configArgs.size();
                constexpr std::size_t numParam  = UpdateStrategyTraits<Strategy>::paramArgs.size();
                StrategyArgList argList;
                argList.reserve(N);

                // Combine extra parameters with observable and observer
                constexpr size_t numParamTotal = numParam + 2;
                std::array<const char*, numParamTotal> allParams{};
                for (std::size_t i = 0; i < numParam; ++i) {
                    allParams[i] = UpdateStrategyTraits<Strategy>::paramArgs[i];
                }
                allParams[numParam]     = observable.c_str();
                allParams[numParam + 1] = observer.c_str();

                auto argsTuple = makeStrategyArgsImpl<UpdateStrategyTraits<Strategy>>(
                    config, paramMap, allParams, std::make_index_sequence<numConfig>{},
                    std::make_index_sequence<numParamTotal>{});

                std::apply(
                    [&argList](auto&&... args) { (argList.emplace_back(std::forward<decltype(args)>(args)), ...); },
                    argsTuple);
                return argList;
            });
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Negotiation utilities for options to strategy name mapping
// ---------------------------------------------------------------------------------------------------------------------
using AllUpdateStrategyTraits = std::tuple<UpdateStrategyTraits<WindAtHeight>>;

/**
 * @brief Checks if a strategy trait matches a given config of options and source parameter.
 *
 * A trait is considered a match if its inner `configArgs` array are fully contained as keys in the provided config,
 * and if the source param (observable) can be found in at least one of the combinations of required parameters.
 *
 * @tparam StrategyTraits The update strategy trait type to check. See `UpdateStrategyTraits` for details.
 *
 * @note This function is an implementation detail and should not be called directly. Use `findMatchingStrategy()`.
 */
template <typename StrategyTraits>
std::tuple<bool, std::vector<std::string>> matchesStrategyTraitsImpl(const std::string& source,
                                                                     const eckit::Configuration& config) {
    // For each arg in StrategyTraits::configArgs, check that it exists in the config
    for (const char* key : StrategyTraits::configArgs) {
        if (!config.has(key))
            return {false, {}};
    }
    // Special case, no required params, any param can be used as source
    if (StrategyTraits::requiredParams.empty()) {
        return {true, {}};
    }
    // Check that the source param is in at least one of the combinations of valid params
    for (const auto& combination : StrategyTraits::requiredParams) {
        for (const auto& param : combination) {
            if (source == param) {
                std::vector<std::string> requiredParams;
                requiredParams.reserve(combination.size());
                for (const char* s : combination) {
                    requiredParams.emplace_back(s);
                }
                return {true, requiredParams};
            }
        }
    }
    return {false, {}};
}

/**
 * @brief Finds the first strategy whose arguments and required params match the provided config and source.
 *
 * Each set of {source, config} should uniquely identify a strategy. If that is not the case, it likely means two
 * strategies are doing the same thing. If no traits match, returns an empty strategy string.
 *
 * @return A tuple with the strategy name, the combination of required params containing the source, the levtype
 *         (might not be applicable to all strategies, may revise in the future, mainly used for naming purposes),
 *         and the options key which should be used as level.
 */
std::tuple<std::string, std::vector<std::string>, std::string, std::string> findMatchingStrategy(
    const std::string& source, const eckit::Configuration& config);

}  // namespace field_provider
}  // namespace plume