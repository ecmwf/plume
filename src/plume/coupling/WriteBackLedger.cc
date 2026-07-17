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
#include "plume/coupling/WriteBackLedger.h"

#include <utility>

#include "eckit/exception/Exceptions.h"
#include "eckit/log/Log.h"

// ParameterValue.h brings in IParameterValue (full definition) and WriteBackKey.h (passkey).
#include "plume/data/ParameterValue.h"

namespace plume {
namespace coupling {

// ---------------------------------------------------------------------------

WriteBackLedger::WriteBackLedger(const WriteAuthorisation& auth, WriteBackPolicy policy) :
    auth_(auth), policy_(policy) {}

WriteBackLedger::~WriteBackLedger() {
    // Auto-detach from ModelData before inspecting slots, so ModelData::ledger_ is nulled
    // even if the slot warnings below fire (avoids any use of the dangling pointer).
    if (onDetach_) {
        onDetach_();
        onDetach_ = nullptr;
    }

    // Log a warning for any slot not cleanly in IDLE at destruction — indicates a missing reset()
    // or an unhandled error that was not cleared before teardown.
    for (const auto& [name, slot] : slots_) {
        if (!slot.state->isIdle()) {
            eckit::Log::warning() << "WriteBackLedger: slot '" << name << "' destroyed in state '" << slot.state->name()
                                  << "'" << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------

void WriteBackLedger::attachParam(const std::string& name, data::IParameterValue* param) {
    ASSERT_MSG(param != nullptr, "WriteBackLedger: cannot attach null parameter '" + name + "'");
    auto [_, inserted] = slots_.emplace(name, ParamSlot{param, idle(), {}, ""});
    // Double-attach is a bug in Manager::feedPlugins(), not a recoverable runtime condition —
    // WriteAuthorisation holds a std::set so duplicates should be impossible in correct usage.
    ASSERT_MSG(inserted, "WriteBackLedger: parameter '" + name + "' is already attached");
}

// ---------------------------------------------------------------------------

void WriteBackLedger::open() {
    for (auto& [name, slot] : slots_) {
        const WriteBackState* next = slot.state->onOpen();
        if (!next->isReady()) {
            // Only IDLE slots are valid to set ready in production.
            // If this condition is met, a previous cycle left an error unhandled.
            throw eckit::Exception("WriteBackLedger::open(): slot '" + name + "' is in state '" + slot.state->name() +
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

void WriteBackLedger::stage(const std::string& paramName, const std::string& pluginName) {
    ParamSlot& slot = getSlot(paramName);

    if (!auth_.isAuthorised(pluginName, paramName)) {
        throw eckit::BadValue(
            "WriteBackLedger: plugin '" + pluginName + "' is not authorised to write parameter '" + paramName + "'",
            Here());
    }

    if (slot.state->isStaged() && !isMultiWriter(policy_)) {
        throw eckit::BadValue("WriteBackLedger: single-writer policy violated — parameter '" + paramName +
                                  "' was already written by '" + slot.writingPlugins.front() + "'; plugin '" +
                                  pluginName + "' cannot write again this cycle",
                              Here());
    }

    slot.state = slot.state->onStage();  // READY → STAGED, or STAGED → STAGED (multi-writer)
    slot.writingPlugins.push_back(pluginName);
}

// ---------------------------------------------------------------------------

void WriteBackLedger::reportError(const std::string& paramName, const std::string& reason) {
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

void WriteBackLedger::flush() {
    for (auto& [name, slot] : slots_) {
        slot.state = slot.state->onFlush();            // STAGED → FLUSHED; READY → IDLE; ERROR → ERROR
        slot.param->disableWriteback(WriteBackKey{});  // unconditional — always lock writes after flush
    }

    if (hasErrors()) {
        std::string details;
        for (const auto& [name, slot] : slots_) {
            if (slot.state->isError()) {
                details += "\n  '" + name + "': " + slot.errorReason;
            }
        }
        throw eckit::Exception("WriteBackLedger::flush() completed with errors:" + details, Here());
    }
}

// ---------------------------------------------------------------------------

void WriteBackLedger::acknowledgeWriteback(const std::string& paramName) {
    ParamSlot& slot = getSlot(paramName);
    slot.state      = slot.state->onAcknowledge();  // FLUSHED → CONFIRMED (throws on invalid transition)
}

// ---------------------------------------------------------------------------

void WriteBackLedger::reset() {
    if (!allConfirmed()) {
        std::string pending;
        for (const auto& [name, slot] : slots_) {
            if (!slot.state->isConfirmed() && !slot.state->isIdle()) {
                pending += " '" + name + "' (" + slot.state->name() + ")";
            }
        }
        throw eckit::Exception(
            "WriteBackLedger::reset(): not all slots are confirmed or idle before the next cycle."
            " Non-conforming slots:" +
                pending,
            Here());
    }
    for (auto& [name, slot] : slots_) {
        if (slot.state->isConfirmed()) {
            slot.state = slot.state->onReset();  // CONFIRMED → IDLE
        }
        // IDLE slots were not written this cycle — already in the right state.
    }
}

// ---------------------------------------------------------------------------

void WriteBackLedger::forceReset() {
    for (auto& [name, slot] : slots_) {
        if (!slot.state->isIdle()) {
            eckit::Log::warning() << "WriteBackLedger::forceReset(): slot '" << name << "' in state '"
                                  << slot.state->name() << "' — forcing to IDLE" << std::endl;
            slot.state = idle();
            slot.errorReason.clear();
            slot.writingPlugins.clear();
            // A READY/STAGED slot still has writeback enabled (flush() never ran).
            slot.param->disableWriteback(WriteBackKey{});
        }
    }
}

// ---------------------------------------------------------------------------

void WriteBackLedger::clearError(const std::string& paramName) {
    ParamSlot& slot = getSlot(paramName);
    slot.state      = slot.state->onClearError();  // ERROR → IDLE
    slot.errorReason.clear();
    // An ERROR slot may still have writeback enabled if the error was reported mid-cycle before flush() ran.
    slot.param->disableWriteback(WriteBackKey{});
}

// ---------------------------------------------------------------------------

bool WriteBackLedger::allConfirmed() const {
    for (const auto& [name, slot] : slots_) {
        // IDLE is acceptable — slot was not written this cycle.
        // Any other non-CONFIRMED state (READY, STAGED, FLUSHED, ERROR) is not.
        if (!slot.state->isConfirmed() && !slot.state->isIdle()) {
            return false;
        }
    }
    return true;
}

bool WriteBackLedger::hasErrors() const {
    for (const auto& [name, slot] : slots_) {
        if (slot.state->isError()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> WriteBackLedger::pendingWritebacks() const {
    std::vector<std::string> result;
    for (const auto& [name, slot] : slots_) {
        if (slot.state->isFlushed()) {
            result.push_back(name);
        }
    }
    return result;
}

const char* WriteBackLedger::slotState(const std::string& paramName) const {
    return getSlot(paramName).state->name();
}

// ---------------------------------------------------------------------------

const WriteBackLedger::ParamSlot& WriteBackLedger::getSlot(const std::string& paramName) const {
    auto it = slots_.find(paramName);
    if (it == slots_.end()) {
        throw eckit::BadParameter("WriteBackLedger: no slot registered for parameter '" + paramName + "'", Here());
    }
    return it->second;
}

// Non-const overload delegates to the const version.
WriteBackLedger::ParamSlot& WriteBackLedger::getSlot(const std::string& paramName) {
    return const_cast<ParamSlot&>(std::as_const(*this).getSlot(paramName));
}

}  // namespace coupling
}  // namespace plume
