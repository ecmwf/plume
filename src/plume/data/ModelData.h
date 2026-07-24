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

#include "atlas/array/Array.h"
#include "atlas/field/Field.h"
#include "atlas/util/Metadata.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/data/FieldAccess.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterType.h"
#include "plume/data/ParameterValue.h"


namespace plume {
namespace coupling {
class WriteBackLedger;  // forward declaration — ModelData holds a non-owning pointer; Manager manages lifetime
}

namespace data {

class ModelDataView;  // forward declaration — filter() returns a plugin-facing view.


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
     * Non-owning pointer to the active write-back ledger. Set by Manager via attachWritebackLedger().
     * Null when write-back is inactive.
     */
    coupling::WriteBackLedger* ledger_ = nullptr;

    /**
     * @brief True on the instance that attached the write-back ledger (the model-facing ModelData).
     *
     * Set by attachWritebackLedger() — ownership is recorded exactly where it is acquired. Filtered
     * plugin-facing views copy the ledger_ pointer (so plugins can call writeParam) but never attach, so
     * they stay false and their base destructor must NOT detach the model's callback.
     */
    bool ownsLedger_ = false;

    /**
     * @brief The name of the plugin this ModelData instance was filtered for.
     *
     * Set by Manager::feedPlugins() on the filtered view passed to each plugin.
     * Used internally by writeParam() to identify the writer in the ledger, and available
     * for audit logging or verbose diagnostics. Empty on the main model-facing instance.
     */
    std::string consumer_;

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

    /// Stage a write-back slot — checks authorisation and policy.
    void stageWriteback(const std::string& name, const std::string& pluginName);

    /// Report a write failure to the ledger.
    void reportWritebackError(const std::string& name, const std::string& reason);

public:
    ModelData();

    ~ModelData();

    // Copyable but not movable. Copy is explicit (silences -Wdeprecated-copy); move is deliberately not
    // declared because the copies that occur in practice are cheap and this avoids a moved-from ledger
    // double-detach in the destructor. Note: the only copies in normal flow are filtered plugin-facing
    // views (shared_ptr refcount bumps, empty strategy maps); a full model-facing copy would also clone the
    // strategyRegistry_/strategyHelpers_ std::function maps, but that does not happen.
    ModelData(const ModelData&)            = default;
    ModelData& operator=(const ModelData&) = default;

    /**
     * @brief Creates a new value of type T, and transfer its ownership to a parameter wrapper.
     *
     * @note Atlas fields can be created using this method from the C++ API, but there is no support for updating them
     *       directly at the moment. Use the `createParam` method with the subscription pattern instead.
     */
    template <typename T>
    void createParam(std::string name, T valInit) {
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
        auto res = valueMap_.try_emplace(name, std::make_shared<ParameterValue<T, IParameterObservable>>(ptr));
        if (!res.second) {
            eckit::Log::warning() << "Parameter '" << name << "' already in Model Data. Not inserted!" << std::endl;
        }
    }

    /**
     * @brief Update the value of a Plume-owned parameter.
     *
     * Model-facing entry point for mutating data that Plume owns (created via createParam). This is distinct
     * from the plugin-facing writeParam API. Parameters that are actively observing a source are updated by
     * their update strategy and cannot be set directly here.
     *
     * @note For Atlas fields, only Plume-owned fields can be updated. Model-provided fields (provideParam) are
     *       not owned by Plume and cannot be mutated through this method.
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
            if (!typedPtr->owns()) {
                throw eckit::UserError("Parameter '" + name + "' is not owned by Plume and cannot be updated!", Here());
            }
            if constexpr (std::is_same_v<T, atlas::Field>) {
                typedPtr->writeFieldInPlace(newVal);
            }
            else {
                typedPtr->set(newVal);
            }
        }
        else {
            throw eckit::BadCast("Plume parameter update type mismatch!", Here());
        }
    }

    /**
     * @brief Plugin-facing write method for write-back parameters.
     *
     * Authorisation and policy are checked via the ledger (stageWriteback). The write is applied
     * immediately to the underlying storage — there is no intermediate buffer. If the write fails,
     * the error is reported to the ledger and the exception is re-thrown.
     *
     * For Atlas fields using this method, the plugin provides a new atlas::Field whose underlying array is copied into
     * the stored field. The model reads the new field data on acknowledgement and copies it back to its Fortran arrays.
     * @todo This implies a lot of potentially expensive copies. See PLUME-75 for a structural alternative.
     *
     * @note Write-back must have been negotiated and the ledger attached by Manager before calling this.
     *
     * @warning Plugins must use this method and not mutate fields obtained via getParam — doing so bypasses
     *          the ledger and the write-back lifecycle entirely. PLUME-75 tracks a structural fix to prevent this.
     *
     * @throws eckit::BadValue      if the write-back ledger is not attached.
     * @throws eckit::BadParameter  if the parameter is not found.
     * @throws eckit::BadValue      (from ledger) if the plugin is not authorised or policy is violated.
     * @throws eckit::BadCast       if the value type does not match the stored parameter type.
     */
    template <typename T, std::enable_if_t<!std::is_invocable_v<T&, FieldWriter&>, int> = 0>
    void writeParam(const std::string& name, const T& value) {
        // FieldView is a read-only handle and FieldWriter is a scope-bound in-place handle — neither is a value that
        // can be written back. Reject them at compile time with a pointer to the right path instead of failing at
        // runtime with a type-mismatch cast.
        static_assert(!std::is_same_v<T, FieldView> && !std::is_same_v<T, FieldWriter>,
                      "writeParam(name, value) cannot take a FieldView/FieldWriter: they are access handles, not "
                      "values. Call fieldView.clone() to get a mutable atlas::Field to write back, or use the in-place "
                      "writeParam(name, body) form to modify the model buffer directly.");
        if (!ledger_) {
            throw eckit::BadValue(
                "ModelData::writeParam: write-back ledger not attached — "
                "write-back must be negotiated and enabled before calling this method.",
                Here());
        }
        if (!hasParameter(name)) {
            throw eckit::BadParameter("Parameter '" + name + "' not found in model data!", Here());
        }
        // Stage: checks authorisation, single/multi-writer policy, and advances slot READY/STAGED → STAGED.
        stageWriteback(name, consumer_);

        try {
            if (auto typedPtr = std::dynamic_pointer_cast<ParameterValueTyped<T>>(valueMap_.at(name))) {
                if constexpr (std::is_same_v<T, atlas::Field>) {
                    typedPtr->writeFieldInPlace(value);
                }
                else {
                    typedPtr->set(value);
                }
                return;
            }
            throw eckit::BadCast("ModelData::writeParam: type mismatch for parameter '" + name + "'", Here());
        }
        catch (const std::exception& e) {
            reportWritebackError(name, e.what());
            throw;
        }
    }

    /**
     * @brief Plugin-facing in-place write-back: stage a write and return a move-only WriteScope.
     *
     * This arity overload (no value argument) is the copy-free counterpart of writeParam(name, value): instead of
     * copying a whole field into the model buffer, it stages the write with the ledger and hands back a WriteScope
     * whose field() aliases the model's own buffer for in-place read-modify-write. commit() finalises; the scope's
     * destructor aborts+reports if commit() was not called. See FieldAccess.h for the WriteScope/FieldWriter contract.
     *
     * @throws eckit::BadValue      if the write-back ledger is not attached.
     * @throws eckit::BadParameter  if the parameter is not found.
     * @throws eckit::BadValue      if the parameter is not an atlas::Field (use writeParam(name, value) instead).
     * @throws eckit::BadValue      (from ledger, during staging) if the plugin is not authorised or policy is violated.
     */
    WriteScope writeParam(const std::string& name);

    /**
     * @brief Plugin-facing in-place write-back, context-manager style: stage, run a body against the model buffer,
     *        and commit — the everyday plugin-author-facing form of the copy-free path.
     *
     * This callable overload owns a WriteScope internally so the author supplies only the field math and never
     * touches staging or commit(). It runs @p body with a FieldWriter aliasing the model's own buffer, then commits;
     * if @p body throws, the WriteScope destructor aborts and reports the failure to the ledger (so a partial write
     * is not silently kept). The body typically builds a mutable atlas view over the FieldWriter and mutates in place:
     *
     * @code
     *   data.writeParam("swh", [](plume::data::FieldWriter& f) {
     *       auto v = atlas::array::make_view<double, 2>(f);   // mutable view over the model buffer
     *       for (...) v(i, j) *= 1.05;                        // in-place read-modify-write
     *   });
     * @endcode
     *
     * The value and callable overloads are mutually exclusive via std::is_invocable_v<F&, FieldWriter&>, so a field or
     * scalar selects writeParam(name, value) while a callable selects this one. Both `[](FieldWriter&){...}` and, by
     * the FieldWriter → atlas::array::Array& conversion, `[](atlas::array::Array&){...}` and generic `[](auto& f){...}`
     * bodies resolve here.
     *
     * @note C++-only utility. This form takes a C++ callable and hands it a FieldWriter, so it is available only to
     *       C++ plugins holding a ModelData(View). Fortran/C plugins drive the copy-free write-back through the
     *       WriteScope begin→mutate→commit C API instead.
     *
     * @throws eckit::BadValue/BadParameter as writeParam(name), plus anything @p body throws (after the abort report).
     */
    template <typename F, std::enable_if_t<std::is_invocable_v<F&, FieldWriter&>, int> = 0>
    void writeParam(const std::string& name, F&& body) {
        WriteScope scope   = writeParam(name);
        FieldWriter writer = scope.field();
        body(writer);
        scope.commit();
    }

    /**
     * @brief Accesses a value of a parameter. Intended for data users to "update" their local view of the model data.
     *
     * @note This interface can be used for source & derived params if the full name is known.
     * @warning For atlas::Field parameters, the returned handle shares the underlying field data with the
     *          stored parameter. Mutating the field through this handle bypasses the write-back ledger
     *          entirely. Plugins with write-back authorisation must use writeParam() instead. PLUME-75 will
     *          implement a structural fix to prevent plugins from mutating fields obtained via getParam().
     */
    template <typename T>
    T getParam(const std::string& name) const {
        if (!hasParameter(name)) {
            throw eckit::BadParameter("Parameter '" + name + "' not found in model data!", Here());
        }
        if (auto typedPtr = std::dynamic_pointer_cast<ParameterValueTyped<T>>(valueMap_.at(name))) {
            return typedPtr->get();
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

    /// Returns a subset of the ModelData as a plugin-facing view, optionally tagged with the consumer plugin name.
    ModelDataView filter(std::set<std::string> params, const std::string& consumer = "") const;

    /// Returns a subset of the ModelData as a plugin-facing view, optionally tagged with the consumer plugin name.
    ModelDataView filter(ParameterCatalogue params, const std::string& consumer = "") const;

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

    // -------------------------------------------------------------------------
    // Write-back interface — Manager-facing
    // -------------------------------------------------------------------------

    /**
     * @brief Attach the write-back ledger for this session. Called by Manager::feedPlugins().
     *
     * The ledger is non-owning: Manager owns the lifetime and calls detachWritebackLedger() at teardown.
     */
    void attachWritebackLedger(coupling::WriteBackLedger* ledger);

    /// Detach the write-back ledger at end of session. Called by Manager::teardown().
    void detachWritebackLedger();

    /**
     * @brief Register all authorised parameters with the ledger. Called by Manager::feedPlugins().
     *
     * Iterates all plugin/param pairs in @p auth, deduplicates by param name, and calls
     * ledger.attachParam() for each. Throws if an authorised param is not present in the value map.
     */
    void enrollWritebackParams(coupling::WriteBackLedger& ledger, const WriteAuthorisation& auth);

    /// Returns the name of the plugin this ModelData view was filtered for.
    const std::string& consumer() const { return consumer_; }

    // -------------------------------------------------------------------------
    // Write-back interface — Model-facing (called by the model after each run())
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the names of all parameters pending model acknowledgement (FLUSHED state).
     *
     * Returns an empty vector when no write-back ledger is attached.
     */
    std::vector<std::string> pendingWritebacks() const;

    /**
     * @brief Acknowledge that the model has ingested the written value for @p name.
     *
     * Advances the slot from FLUSHED → CONFIRMED. Must be called for every parameter returned by
     * pendingWritebacks() before the next run() cycle.
     *
     * @throws eckit::BadValue if the ledger is not attached.
     */
    void acknowledgeWriteback(const std::string& name);
};

}  // namespace data
}  // namespace plume