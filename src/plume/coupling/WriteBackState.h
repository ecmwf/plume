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

#include <string>

#include "eckit/exception/Exceptions.h"

namespace plume {
namespace coupling {

// Forward-declare all concrete states so transition methods can reference the accessors below.
class WriteBackState;  // base, needed by accessor return types
class Idle;
class Ready;
class Staged;
class Flushed;
class Confirmed;
class WriteBackError;

/**
 * @defgroup writeback_flyweights Flyweight state accessors
 *
 * Each function returns a pointer to a shared, immutable state object (function-local static, initialised once on
 * first call). State classes carry no data, all instances of the same class are identical, so all writable parameters
 * can share one object per class to avoid redundant allocation.
 *
 * Unlike a singleton, nothing prevents creating additional instances of these classes; the accessor functions simply
 * make sharing convenient. In practice, WriteBackLedger obtains states exclusively through these accessors and never
 * constructs state objects directly.
 *
 * @{
 */
const WriteBackState* idle();
const WriteBackState* ready();
const WriteBackState* staged();
const WriteBackState* flushed();
const WriteBackState* confirmed();
const WriteBackState* writebackError();
/** @} */

// =============================================================================

/**
 * @class WriteBackState
 * @brief Abstract base for the write-back per-parameter lifecycle (State pattern).
 *
 * Each subclass represents one lifecycle state. Concrete state objects are shared across all ParamSlots via the
 * flyweight accessors above; ParamSlot swaps its state pointer on each transition. Mutable slot data (error reason,
 * writing plugin list) lives in ParamSlot, not here.
 *
 * Valid transitions:
 *   IDLE      -> READY     via onOpen()
 *   READY     -> STAGED    via onStage()
 *   READY     -> IDLE      via onFlush()        — no plugin chooses to write this cycle (not an error)
 *   READY     -> ERROR     via onError()        — ledger detected a failure
 *   STAGED    -> STAGED    via onStage()        — multi-writer: subsequent plugins accumulate writes
 *   STAGED    -> FLUSHED   via onFlush()
 *   STAGED    -> ERROR     via onError()
 *   FLUSHED   -> CONFIRMED via onAcknowledge()
 *   CONFIRMED -> IDLE      via onReset()
 *   ERROR     -> IDLE      via onClearError()
 *   ERROR     -> ERROR     via any other trigger (stays poisoned; swallows trigger silently to preserve
 *                          the original error, e.g. flush() called after a failed stage() won't mask it)
 *
 * Caller responsibilities:
 *   - Single-writer policy: ledger enforces this before calling onStage() on a STAGED slot.
 *   - Error detection: ledger calls onError() instead of onFlush() when it detects a failure.
 */
class WriteBackState {
public:
    virtual ~WriteBackState() = default;

    virtual const WriteBackState* onOpen() const { invalidTransition("onOpen"); }
    virtual const WriteBackState* onStage() const { invalidTransition("onStage"); }
    virtual const WriteBackState* onFlush() const { invalidTransition("onFlush"); }
    virtual const WriteBackState* onAcknowledge() const { invalidTransition("onAcknowledge"); }
    virtual const WriteBackState* onReset() const { invalidTransition("onReset"); }
    virtual const WriteBackState* onError() const { invalidTransition("onError"); }
    virtual const WriteBackState* onClearError() const { invalidTransition("onClearError"); }

    virtual const char* name() const = 0;

    virtual bool isIdle() const { return false; }
    virtual bool isReady() const { return false; }
    virtual bool isStaged() const { return false; }
    virtual bool isFlushed() const { return false; }
    virtual bool isConfirmed() const { return false; }
    virtual bool isError() const { return false; }

protected:
    [[noreturn]] void invalidTransition(const char* trigger) const {
        throw eckit::BadValue(
            std::string("WriteBackState: invalid transition '") + trigger + "' from state '" + name() + "'", Here());
    }
};

// =============================================================================

/// Slot exists but this parameter cycle has not started. No plugin may write.
class Idle : public WriteBackState {
public:
    const char* name() const override { return "IDLE"; }
    bool isIdle() const override { return true; }

    const WriteBackState* onOpen() const override { return ready(); }
};

// =============================================================================

/**
 * Ledger has opened this slot; an authorised plugin may call stage(). Reaching onFlush() without any write silently
 * resets to IDLE (skipping a write is valid plugin behaviour, e.g., nothing changed this timestep).
 */
class Ready : public WriteBackState {
public:
    const char* name() const override { return "READY"; }
    bool isReady() const override { return true; }

    const WriteBackState* onStage() const override { return staged(); }
    const WriteBackState* onFlush() const override { return idle(); }  // silent reset: not an error
    const WriteBackState* onError() const override { return writebackError(); }
};

// =============================================================================

/**
 * Plugin has written; the write was applied immediately to parameter storage.
 *
 * onStage() returns STAGED again to support multi-writer policy (subsequent plugins accumulate writes sequentially).
 * Single-writer enforcement is the ledger's responsibility.
 * onFlush() is state-only - no data movement (write already applied).
 */
class Staged : public WriteBackState {
public:
    const char* name() const override { return "STAGED"; }
    bool isStaged() const override { return true; }

    const WriteBackState* onStage() const override { return staged(); }  // multi-writer: stays STAGED
    const WriteBackState* onFlush() const override { return flushed(); }
    const WriteBackState* onError() const override { return writebackError(); }
};

// =============================================================================

/// Write complete and visible; parameter appears in pendingWritebacks(). Waiting for model acknowledgement.
class Flushed : public WriteBackState {
public:
    const char* name() const override { return "FLUSHED"; }
    bool isFlushed() const override { return true; }

    const WriteBackState* onAcknowledge() const override { return confirmed(); }
};

// =============================================================================

/// Model has acknowledged the write. Slot may be reset to IDLE for the next cycle.
class Confirmed : public WriteBackState {
public:
    const char* name() const override { return "CONFIRMED"; }
    bool isConfirmed() const override { return true; }

    const WriteBackState* onReset() const override { return idle(); }
};

// =============================================================================

/**
 * A write-back failure was detected; the slot is poisoned for this cycle.
 * Any trigger except clearError() keeps the slot in ERROR without throwing —
 * the error reason is recorded in ParamSlot. Recovery requires explicit clearError().
 * An attempt to write to an ERROR slot throws to prevent further mutation.
 */
class WriteBackError : public WriteBackState {
public:
    const char* name() const override { return "ERROR"; }
    bool isError() const override { return true; }

    const WriteBackState* onOpen() const override { return writebackError(); }
    const WriteBackState* onStage() const override { invalidTransition("onStage"); }
    const WriteBackState* onFlush() const override { return writebackError(); }
    const WriteBackState* onAcknowledge() const override { return writebackError(); }
    const WriteBackState* onReset() const override { return writebackError(); }
    const WriteBackState* onError() const override { return writebackError(); }
    const WriteBackState* onClearError() const override { return idle(); }
};

// =============================================================================
// Flyweight accessor definitions (see @defgroup writeback_flyweights above).
//
// Each accessor holds a function-local static with program lifetime — created once on first call, never deleted.
// Callers (ParamSlot) hold a raw const* to observe the state object. The const qualifier guarantess that state objects
// are immutable, so raw pointer sharing carries no mutation or data race risk. This lazily-initialised flyweight
// implementation follows the Meyer's Singleton pattern.

inline const WriteBackState* idle() {
    static Idle s;
    return &s;
}
inline const WriteBackState* ready() {
    static Ready s;
    return &s;
}
inline const WriteBackState* staged() {
    static Staged s;
    return &s;
}
inline const WriteBackState* flushed() {
    static Flushed s;
    return &s;
}
inline const WriteBackState* confirmed() {
    static Confirmed s;
    return &s;
}
inline const WriteBackState* writebackError() {
    static WriteBackError s;
    return &s;
}

}  // namespace coupling
}  // namespace plume
