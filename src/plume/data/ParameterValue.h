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
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "eckit/exception/Exceptions.h"

#include "atlas/field/Field.h"

#include "plume/data/FieldProvider.h"
#include "plume/data/ParameterType.h"

namespace plume {
namespace data {

/**
 * @class IParameterValue
 * @brief Interface for parameter values. Non-typed base class managing value update status.
 */
class IParameterValue {
private:
    bool isUpdated_ = false;

public:
    virtual ~IParameterValue() = default;

    virtual ParameterType type() const { return ParameterType::INVALID; }

    bool isUpdated() const { return isUpdated_; }
    virtual void setUpdated(bool updated) { isUpdated_ = updated; }
};

/**
 * @class ParameterValueTyped
 * @brief Template parameter value with optional ownership.
 *
 * Represents a parameter of type `T` that may either own its value or observe an externally owned value.
 * The parameter exposes its runtime type via `ParameterType` and provides read-only or controlled mutable access
 * depending on ownership, with a special case for Atlas fields.
 *
 * If constructed in non-owning mode, the referenced value must outlive this object. If constructed in owning mode,
 * the value is stored internally and may be modified through the provided setters.
 *
 * @tparam T Underlying parameter value type. Must be supported by `deduceType()`.
 */
template <typename T>
class ParameterValueTyped {
private:
    bool ownsValue_;
    ParameterType type_;

    std::optional<T> ownedValue_;  ///< Backing storage for an owned value, or a shared handle held for lifetime.
    T* valuePtr_;                  ///< Non-owning pointer; the pointee must outlive this object if it is not owned.

public:
    ~ParameterValueTyped() = default;

    /**
     * @brief Constructs an owning parameter with a copied value.
     *
     * Transfers ownership of the passed `value` to the object.
     */
    ParameterValueTyped(T value) :
        ownsValue_(true), ownedValue_(std::move(value)), valuePtr_(&ownedValue_.value()), type_(deduceType<T>()) {}

    /**
     * @brief Constructs a non-owning parameter from a pointer to data owned elsewhere (typically the model).
     *
     * For most types the pointer is stored directly, so the pointee must outlive this object. Atlas fields are the
     * exception: `atlas::Field` is a reference-counted handle around model-owned data, so a copy of the handle is
     * kept as backing storage to keep the field accessible for the session. In both cases the parameter stays
     * non-owning (`ownsValue_ == false`): Plume does not own the underlying data, so `set()`/`getSettableField()`
     * remain disallowed and `updateParam` cannot mutate it.
     *
     * @question: do we need special cases for char, char* ? There is currently no support in the C API.
     */
    ParameterValueTyped(T* ptr) : ownsValue_(false), type_(deduceType<T>()) {
        ASSERT_MSG(ptr != nullptr, "Non-owning ParameterValue constructed with null pointer");
        if constexpr (std::is_same_v<T, atlas::Field>) {
            ASSERT_MSG(ptr->bytes() >= 0, "Provided Atlas field not readable!");
            ownedValue_ = *ptr;  // copy the handle to keep model-owned data alive; ownsValue_ stays false
            valuePtr_   = &ownedValue_.value();
        }
        else {
            valuePtr_ = ptr;
        }
    }

    /// Deleted copy constructor & copy assignment operator.
    ParameterValueTyped(const ParameterValueTyped& other)            = delete;
    ParameterValueTyped& operator=(const ParameterValueTyped& other) = delete;

    /// Returns the parameter type as a string.
    std::string toString() const { return typeToString(type_); }

    ParameterType type() const { return type_; }
    bool owns() const { return ownsValue_; }

    /**
     * @brief Returns a read-only reference to the parameter value.
     *
     * @warning In non-owning mode, the referenced value must still be alive.
     */
    const T& get() const { return *valuePtr_; }

    /**
     * @brief Sets the parameter value.
     *
     * For Atlas fields this replaces the stored handle, which lets update strategies reshape or reinitialise an
     * owned field. Model-facing in-place mutation that preserves the handle identity is done via updateParam.
     *
     * @pre This instance must own its value.
     */
    void set(const T& value) {
        ASSERT(ownsValue_);
        *ownedValue_ = value;
    }

    /**
     * @brief Returns a mutable reference to the stored Atlas field.
     *
     * @pre This instance must own its value. This method can be used by update strategies to change the values of
     *      owned Atlas fields, instead of setting the field from a new one entirely.
     */
    template <typename U = T, typename = std::enable_if_t<std::is_base_of_v<atlas::Field, U>>>
    T& getSettableField() {
        ASSERT(ownsValue_);
        return *valuePtr_;
    }
};

class IParameterObservable;  // forward declaration

/**
 * @class IParameterObserver
 * @brief Interface for observing parameters.
 *
 * Observers can listen to a single subjet, which should be of the observable type. Observers are not necessarily
 * actively listening, in which case, they do not need to have a subject or an update strategy. Active observers must
 * implement a reaction to subject changes, and this reaction can optionally be based on an `UpdateStrategy`.
 */
class IParameterObserver : public std::enable_shared_from_this<IParameterObserver> {
private:
    static const std::string SEP_;
    std::weak_ptr<IParameterObservable> subject_;  ///< weak reference to observed parameter to avoid cyclic ownership.

    /// Optional, a parameter actively observing should have a strategy for reacting to changes in the subject.
    std::unique_ptr<field_provider::UpdateStrategy> updateStrategy_;

public:
    /// The active observer lazily detaches oneself from its subject when it notifies or destroys, nothing to do here.
    virtual ~IParameterObserver() = default;

    virtual bool observes() { return !subject_.expired(); }
    virtual void setSubject(std::shared_ptr<IParameterObservable> subject);
    virtual void setUpdateStrategy(std::unique_ptr<field_provider::UpdateStrategy> strategy) {
        updateStrategy_ = std::move(strategy);
    }
    virtual field_provider::UpdateStrategy* getStrategy() const { return updateStrategy_.get(); }

    /// Each concrete observer that is actively observing must define its own reaction to subject change.
    virtual void onSubjectChanged() {
        ASSERT_MSG(!observes(), "A parameter that observes must implement a reaction to subject change.");
    }

    /**
     * @brief Builds default name for observer parameters.
     *
     * @note In case of Atlas field, this name is decorrelated from the field name in its metadata.
     */
    static std::string deriveParamName(const std::string& source, const std::string& levtype, const std::string& level);
};

/**
 * @class IParameterObservable
 * @brief Interface for observable parameters (also referred to as subjects, or publishers).
 *
 * Multiple observers can listen to the same publisher. The client managing an observable is responsible for triggering
 * a notification to its observers when applicable. It provides an interface for observers to start and stop listening
 * to it.
 */
class IParameterObservable {
private:
    /// Vector of weak references to observers to dispatch notifications to to avoid cyclic ownership.
    std::vector<std::weak_ptr<IParameterObserver>> observers_;

protected:
    /// Calls `onSubjectChanged()` on all non-expired observers.
    virtual void notifyObservers();

public:
    /**
     * @brief Destroys the publisher.
     *
     * Cleans up already destroyed observers, then resets the subject of active observers, and finally clears its
     * list of observers.
     */
    virtual ~IParameterObservable() {
        observers_.erase(std::remove_if(observers_.begin(), observers_.end(), [](auto& w) { return w.expired(); }),
                         observers_.end());

        for (auto& wobs : observers_) {
            if (auto obs = wobs.lock()) {
                obs->setSubject(nullptr);
            }
        }
        observers_.clear();
    }

    /// Adds observer to the list of current observers if it is not already listening.
    virtual void attach(std::shared_ptr<IParameterObserver> observer);

    /// Removes observer from the list of current observers if it is listening.
    virtual void detach(std::shared_ptr<IParameterObserver> observer);
};

// =====================================================================================================================
// Concrete parameter value
// =====================================================================================================================

/**
 * @class ParameterValue
 * @brief A concrete parameter that combines typed storage and observer/publisher behaviour.
 *
 * With the current design, parameter values are assigned either the observer or publisher role. Therefore, observers
 * cannot watch other observers.
 *
 * @tparam T Underlying parameter value type.
 * @tparam Role The role of this template. This header implements observer and publisher roles.
 */
template <typename T, typename Role>
class ParameterValue : public ParameterValueTyped<T>, public Role, public IParameterValue {
public:
    ~ParameterValue() = default;

    /// Inherit constructors from `ParameterValueTyped<T>` for value ownership.
    using ParameterValueTyped<T>::ParameterValueTyped;

    ParameterType type() const override { return ParameterValueTyped<T>::type(); }

    /**
     * @brief Sets the updated flag and triggers notifications if applicable.
     *
     * If `Role` is `IParameterObservable`, this notifies observers when the parameter becomes updated.
     * Otherwise, only sets the updated flag.
     */
    void setUpdated(bool updated) override {
        IParameterValue::setUpdated(updated);
        if constexpr (std::is_same_v<Role, IParameterObservable>) {
            if (isUpdated()) {
                this->Role::notifyObservers();
            }
        }
    }

    /**
     * @brief Reacts to changes in the observed subject.
     *
     * If `Role` is `IParameterObserver`, the strategy is used to update the parameter value.
     */
    void onSubjectChanged() {  // reactions are runtime strategy-based
        if constexpr (std::is_same_v<Role, IParameterObserver>) {
            ASSERT_MSG(this->Role::getStrategy() != nullptr, "Plume parameter missing update strategy");
            this->Role::getStrategy()->update();
        }
    }
};

}  // namespace data
}  // namespace plume