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
#include "plume/coupling/WriteBackTracker.h"

#include <utility>

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

// ParameterValue.h brings in IParameterValue (full definition) and WriteBackKey.h (passkey).
#include "plume/data/ParameterValue.h"

namespace plume {
namespace coupling {

// ---------------------------------------------------------------------------

WriteBackTracker::WriteBackTracker(const WriteAuthorisation& auth, WriteBackPolicy policy) :
    auth_(auth), policy_(policy) {}

WriteBackTracker::~WriteBackTracker() {
    // Auto-detach from ModelData before inspecting slots, so ModelData::tracker_ is nulled
    // even if the slot warnings below fire (avoids any use of the dangling pointer).
    if (onDetach_) {
        onDetach_();
        onDetach_ = nullptr;
    }

    // Log a warning for any slot not cleanly in IDLE at destruction — indicates a missing reset()
    // or an unhandled error that was not cleared before teardown.
    for (const auto& [name, slot] : slots_) {
        if (!slot.state->isIdle()) {
            eckit::Log::warning() << "WriteBackTracker: slot '" << name << "' destroyed in state '" << slot.state->name()
                                  << "'" << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------

void WriteBackTracker::attachParam(const std::string& name, data::IParameterValue* param) {
    ASSERT_MSG(param != nullptr, "WriteBackTracker: cannot attach null parameter '" + name + "'");
    auto [_, inserted] = slots_.emplace(name, ParamSlot{param, idle(), {}, ""});
    // Double-attach is a bug in Manager::feedPlugins(), not a recoverable runtime condition —
    // WriteAuthorisation holds a std::set so duplicates should be impossible in correct usage.
    ASSERT_MSG(inserted, "WriteBackTracker: parameter '" + name + "' is already attached");
}

// ---------------------------------------------------------------------------

void WriteBackTracker::open() {
    for (auto& [name, slot] : slots_) {
        const WriteBackState* next = slot.state->onOpen();
        if (!next->isReady()) {
            // Only IDLE slots are valid to set ready in production.
            // If this condition is met, a previous cycle left an error unhandled.
            throw eckit::Exception("WriteBackTracker::open(): slot '" + name + "' is in state '" + slot.state->name() +
                                       "' and cannot be set ready",
                                   Here());
        }
        slot.state = next;
        slot.writingPlugins.clear();
        slot.errorReason.clear();
        slot.param->enableWriteback(WriteBackKey{});
    }
}

// ---------------------------------------------------------------------------

void WriteBackTracker::write(const std::string& paramName, const std::string& pluginName) {
    ParamSlot& slot = getSlot(paramName);

    if (!auth_.isAuthorised(pluginName, paramName)) {
        throw eckit::BadValue(
            "WriteBackTracker: plugin '" + pluginName + "' is not authorised to write parameter '" + paramName + "'",
            Here());
    }

    if (slot.state->isWritten() && !isMultiWriter(policy_)) {
        throw eckit::BadValue("WriteBackTracker: single-writer policy violated — parameter '" + paramName +
                                  "' was already written by '" + slot.writingPlugins.front() + "'; plugin '" +
                                  pluginName + "' cannot write again this cycle",
                              Here());
    }

    slot.state = slot.state->onWrite();  // READY → WRITTEN, or WRITTEN → WRITTEN (multi-writer)
    slot.writingPlugins.push_back(pluginName);
}

// ---------------------------------------------------------------------------

void WriteBackTracker::reportError(const std::string& paramName, const std::string& reason) {
    ParamSlot& slot = getSlot(paramName);
    slot.state      = slot.state->onError();
    // A poisoned slot must not receive further writes this cycle.
    slot.param->disableWriteback(WriteBackKey{});
    // Append rather than replace — multiple plugins may report errors for the same slot.
    if (!slot.errorReason.empty()) {
        slot.errorReason += "; ";
    }
    slot.errorReason += reason;
}

// ---------------------------------------------------------------------------

void WriteBackTracker::submit() {
    for (auto& [name, slot] : slots_) {
        slot.state = slot.state->onSubmit();            // WRITTEN → PENDING; READY → IDLE; ERROR → ERROR
        slot.param->disableWriteback(WriteBackKey{});  // unconditional — always lock writes after submit
    }

    if (hasErrors()) {
        std::string details;
        for (const auto& [name, slot] : slots_) {
            if (slot.state->isError()) {
                details += "\n  '" + name + "': " + slot.errorReason;
            }
        }
        throw eckit::Exception("WriteBackTracker::submit() completed with errors:" + details, Here());
    }
}

// ---------------------------------------------------------------------------

void WriteBackTracker::acknowledgeWriteback(const std::string& paramName) {
    ParamSlot& slot = getSlot(paramName);
    slot.state      = slot.state->onAcknowledge();  // PENDING → ACKNOWLEDGED (throws on invalid transition)
}

// ---------------------------------------------------------------------------

void WriteBackTracker::reset() {
    if (!allAcknowledged()) {
        std::string pending;
        for (const auto& [name, slot] : slots_) {
            if (!slot.state->isAcknowledged() && !slot.state->isIdle()) {
                pending += " '" + name + "' (" + slot.state->name() + ")";
            }
        }
        throw eckit::Exception(
            "WriteBackTracker::reset(): not all slots are acknowledged or idle before the next cycle."
            " Non-conforming slots:" +
                pending,
            Here());
    }
    for (auto& [name, slot] : slots_) {
        if (slot.state->isAcknowledged()) {
            slot.state = slot.state->onReset();  // ACKNOWLEDGED → IDLE
        }
        // IDLE slots were not written this cycle — already in the right state.
    }
}

// ---------------------------------------------------------------------------

void WriteBackTracker::forceReset() {
    for (auto& [name, slot] : slots_) {
        if (!slot.state->isIdle()) {
            eckit::Log::warning() << "WriteBackTracker::forceReset(): slot '" << name << "' in state '"
                                  << slot.state->name() << "' — forcing to IDLE" << std::endl;
            slot.state = idle();
            slot.errorReason.clear();
            slot.writingPlugins.clear();
            // A READY/WRITTEN slot still has writeback enabled (submit() never ran).
            slot.param->disableWriteback(WriteBackKey{});
        }
    }
}

// ---------------------------------------------------------------------------

void WriteBackTracker::clearError(const std::string& paramName) {
    ParamSlot& slot = getSlot(paramName);
    slot.state      = slot.state->onClearError();  // ERROR → IDLE
    slot.errorReason.clear();
    // An ERROR slot may still have writeback enabled if the error was reported mid-cycle before submit() ran.
    slot.param->disableWriteback(WriteBackKey{});
}

// ---------------------------------------------------------------------------

bool WriteBackTracker::allAcknowledged() const {
    for (const auto& [name, slot] : slots_) {
        // IDLE is acceptable — slot was not written this cycle.
        // Any other non-ACKNOWLEDGED state (READY, WRITTEN, PENDING, ERROR) is not.
        if (!slot.state->isAcknowledged() && !slot.state->isIdle()) {
            return false;
        }
    }
    return true;
}

bool WriteBackTracker::hasErrors() const {
    for (const auto& [name, slot] : slots_) {
        if (slot.state->isError()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> WriteBackTracker::pendingWritebacks() const {
    std::vector<std::string> result;
    for (const auto& [name, slot] : slots_) {
        if (slot.state->isPending()) {
            result.push_back(name);
        }
    }
    return result;
}

const char* WriteBackTracker::slotState(const std::string& paramName) const {
    return getSlot(paramName).state->name();
}

// ---------------------------------------------------------------------------

const WriteBackTracker::ParamSlot& WriteBackTracker::getSlot(const std::string& paramName) const {
    auto it = slots_.find(paramName);
    if (it == slots_.end()) {
        throw eckit::BadParameter("WriteBackTracker: no slot registered for parameter '" + paramName + "'", Here());
    }
    return it->second;
}

// Non-const overload delegates to the const version.
WriteBackTracker::ParamSlot& WriteBackTracker::getSlot(const std::string& paramName) {
    return const_cast<ParamSlot&>(std::as_const(*this).getSlot(paramName));
}

}  // namespace coupling
}  // namespace plume
