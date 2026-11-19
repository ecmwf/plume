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
#include <set>
#include <string>
#include <vector>

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/coupling/WriteBackState.h"

namespace plume {
namespace data {
class IParameterValue;  // forward declaration — avoid pulling in the full ParameterValue header
}
namespace test {
struct ManagerTestAccess;  // forward declaration — grants test-only access to private methods
}

namespace coupling {

/**
 * @class WriteBackTracker
 * @brief Tracks the write-back lifecycle for each authorised writable parameter.
 *
 * One tracker is created by Manager::feedPlugins() and destroyed at teardown().
 * It is the only entity that can enable or disable writeback on IParameterValue objects,
 * enforced at compile time via the WriteBackKey passkey.
 *
 * Lifecycle per run(hook) cycle (one cycle per hook point call):
 *   open(hook)             — slots writable at the hook point (ACKNOWLEDGED → IDLE →) READY; writeback enabled on
 *                            each of them; the other slots stay closed
 *   write() × N            — authorisation + policy check; slot READY/WRITTEN → WRITTEN
 *   submit()                — opened slots WRITTEN → PENDING, READY → IDLE (silent skip);
 *                            writeback disabled unconditionally; throws if any ERROR
 *   acknowledgeWriteback() — model acknowledges each PENDING slot → ACKNOWLEDGED
 *                            guarantees that the model has ingested the changes from the field copies
 *   reset()                — asserts all acknowledged; ACKNOWLEDGED → IDLE for next cycle
 *
 * @see WriteBackKey, WriteBackState
 */
class WriteBackTracker {
private:
    // ParamSlot is an internal implementation detail of the tracker. Keeping it private
    // prevents external code from storing ParamSlot* or ParamSlot& references directly,
    // which would couple callers to the internal layout and break if the struct changes.
    struct ParamSlot {
        data::IParameterValue* param;             ///< Non-owning; must outlive the tracker.
        const WriteBackState* state;              ///< Flyweight pointer; swapped on each transition.
        std::vector<std::string> writingPlugins;  ///< Ordered audit log of plugins that wrote this cycle.
        std::string errorReason;                  ///< Non-empty when state->isError().
    };

    const WriteAuthorisation& auth_;
    WriteBackPolicy policy_;
    std::map<std::string, ParamSlot> slots_;

    // Current write cycle: the hook point being run and the slots opened for it (empty outside a cycle).
    bool inCycle_ = false;
    std::string currentHook_;
    std::set<std::string> openSlots_;
    std::function<void()> onDetach_;  ///< Called in the destructor to auto-detach from ModelData.

    ParamSlot& getSlot(const std::string& paramName);
    const ParamSlot& getSlot(const std::string& paramName) const;

    /**
     * @brief Test-only variant of reset(): logs unacknowledged or errored slots instead of throwing.
     *
     * Resets all non-IDLE slots to IDLE. Accessible only via ManagerTestAccess.
     */
    void forceReset();

    /// Test-only: manually recover a single ERROR slot to IDLE. Accessible only via ManagerTestAccess.
    void clearError(const std::string& paramName);

    friend struct plume::test::ManagerTestAccess;

public:
    WriteBackTracker(const WriteAuthorisation& auth, WriteBackPolicy policy);
    ~WriteBackTracker();

    // Deleted: holds a reference (auth_) and raw pointers (ParamSlot::param) — copying would
    // produce two trackers operating on the same parameters with undefined ownership.
    WriteBackTracker(const WriteBackTracker&)            = delete;
    WriteBackTracker& operator=(const WriteBackTracker&) = delete;

    /**
     * @brief Register a callback to invoke when this tracker is destroyed.
     *
     * Called by ModelData::attachWritebackTracker() to set tracker_ = nullptr on auto-detach.
     */
    void setDetachCallback(std::function<void()> cb) { onDetach_ = std::move(cb); }

    /// Register a writable parameter. Called during feedPlugins() for each authorised param.
    void attachParam(const std::string& name, data::IParameterValue* param);

    /**
     * @brief Open a write cycle for a hook point: the slots of the params writable at that hook point go
     *        (ACKNOWLEDGED → IDLE →) IDLE → READY, writeback enabled on each param. The other slots stay closed.
     *
     * Called by Manager at the start of each run(hook).
     *
     * @throws eckit::Exception if a slot to open is not IDLE or ACKNOWLEDGED, e.g. still PENDING: the model has not
     *         acknowledged the previous write of that param.
     */
    void open(const std::string& hook);

    /// Open a write cycle for the default hook point (plume::DEFAULT_HOOK).
    void open();

    /**
     * @brief Record that pluginName is writing paramName and advance READY/WRITTEN → WRITTEN.
     *
     * @throws eckit::BadValue param not open for writing at the current hook point (or no open cycle), plugin not
     *         authorised at the current hook point, or single-writer policy violated.
     */
    void write(const std::string& paramName, const std::string& pluginName);

    /**
     * @brief Whether pluginName may write paramName now: a cycle is open, the slot is open at the current hook point,
     *        not in ERROR, and the plugin is authorised to write it at the current hook point.
     */
    bool isWritable(const std::string& paramName, const std::string& pluginName) const;

    /// The hook point of the open write cycle (empty outside a cycle).
    const std::string& currentHook() const { return currentHook_; }

    /// Transition a slot to ERROR and record the reason. Called by ModelData when a write throws.
    void reportError(const std::string& paramName, const std::string& reason);

    /**
     * @brief Close the write cycle: advance the opened slots WRITTEN → PENDING, READY → IDLE (silent skip, not an
     *        error). Outside a cycle, all the slots are advanced.
     *
     * State-only — no data movement (writes were applied immediately at write() time), but signals to the
     * model that it potentially has new field copies to ingest. Disables writeback on these params unconditionally.
     *
     * @throws eckit::Exception if any of these slots are in ERROR.
     */
    void submit();

    /// Advance a PENDING slot to ACKNOWLEDGED. Called by the model via ModelData::acknowledgeWriteback().
    void acknowledgeWriteback(const std::string& paramName);

    /**
     * @brief Assert allAcknowledged(), then cycle ACKNOWLEDGED → IDLE for the next run() cycle.
     *
     * @throws eckit::Exception if any PENDING slots remain — model must acknowledge all writes before the next cycle.
     */
    void reset();

    /// Returns true if no PENDING slots remain (all writes acknowledged or not written this cycle).
    bool allAcknowledged() const;

    /// Returns true if any slot is currently in ERROR state.
    bool hasErrors() const;

    /// Returns the names of all PENDING slots awaiting model acknowledgement.
    std::vector<std::string> pendingWritebacks() const;

    /// Returns the current state name of a slot, for diagnostics.
    const char* slotState(const std::string& paramName) const;
};

}  // namespace coupling
}  // namespace plume
