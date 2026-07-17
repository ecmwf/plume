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
#include <string>
#include <vector>

#include "eckit/testing/Test.h"

#include "plume/coupling/WriteAuthorisation.h"
#include "plume/coupling/WriteBackTracker.h"
#include "plume/coupling/WriteBackPolicy.h"
#include "plume/data/ModelData.h"
#include "plume/data/ParameterValue.h"

#include "ManagerTestAccess.h"

using namespace eckit::testing;

namespace plume::test {

// Helpers

/// Build a WriteAuthorisation with a single (plugin, param) pair.
static WriteAuthorisation oneAuth(const std::string& plugin, const std::string& param) {
    WriteAuthorisation auth;
    auth.grant(plugin, param);
    return auth;
}

/// Build a WriteAuthorisation with two plugins both authorised for the same param.
static WriteAuthorisation twoPluginsOneParam(const std::string& p1, const std::string& p2, const std::string& param) {
    WriteAuthorisation auth;
    auth.grant(p1, param);
    auth.grant(p2, param);
    return auth;
}

/// Default fixture: single param "x", single plugin "PluginA", single_writer policy.
/// Non-copyable — tracker holds a reference to auth, param slot holds a pointer to x.
struct Fixture {
    WriteAuthorisation auth = oneAuth("PluginA", "x");
    data::ParameterValueTyped<int> x{0};
    std::unique_ptr<coupling::WriteBackTracker> trackerPtr{
        std::make_unique<coupling::WriteBackTracker>(auth, WriteBackPolicy::single_writer)};

    Fixture() { trackerPtr->attachParam("x", &x); }
    Fixture(const Fixture&)            = delete;
    Fixture& operator=(const Fixture&) = delete;
};

// ---- State machine — happy path ----

CASE("single_writer_full_cycle") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));

    tracker.open();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("READY"));
    EXPECT(f.x.isWritable());

    tracker.write("x", "PluginA");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("WRITTEN"));

    tracker.submit();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("PENDING"));
    EXPECT_NOT(f.x.isWritable());

    auto pending = tracker.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string("x"));

    tracker.acknowledgeWriteback("x");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ACKNOWLEDGED"));
    EXPECT(tracker.allAcknowledged());

    tracker.reset();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));
}

CASE("slot_not_written_silently_skips") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("READY"));

    tracker.submit();  // no write() — READY → IDLE silently
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));

    EXPECT(tracker.pendingWritebacks().empty());
    EXPECT(tracker.allAcknowledged());
}

CASE("multi_cycle_run") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    for (int cycle = 0; cycle < 3; ++cycle) {
        tracker.reset();  // no-op on first cycle (all IDLE); ACKNOWLEDGED → IDLE on subsequent
        tracker.open();
        EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("READY"));

        tracker.write("x", "PluginA");
        tracker.submit();
        tracker.acknowledgeWriteback("x");
        EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ACKNOWLEDGED"));
    }
}

CASE("multi_writer_allowed") {
    auto auth = twoPluginsOneParam("PluginA", "PluginB", "x");
    data::ParameterValueTyped<int> x{0};
    coupling::WriteBackTracker tracker{auth, WriteBackPolicy::multi_writer};
    tracker.attachParam("x", &x);

    tracker.open();
    tracker.write("x", "PluginA");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("WRITTEN"));

    EXPECT_NO_THROW(tracker.write("x", "PluginB"));
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("WRITTEN"));

    tracker.submit();
    tracker.acknowledgeWriteback("x");
    tracker.reset();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));
}

// ---- Authorisation enforcement ----

CASE("unauthorised_plugin_throws") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    EXPECT_THROWS(tracker.write("x", "PluginB"));  // PluginB not authorised
}

CASE("write_on_closed_slot_throws") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    EXPECT_THROWS(tracker.write("x", "PluginA"));  // slot still IDLE — open() not called
}

CASE("single_writer_violation_throws") {
    auto auth = twoPluginsOneParam("PluginA", "PluginB", "x");
    data::ParameterValueTyped<int> x{0};
    coupling::WriteBackTracker tracker{auth, WriteBackPolicy::single_writer};
    tracker.attachParam("x", &x);

    tracker.open();
    tracker.write("x", "PluginA");
    EXPECT_THROWS(tracker.write("x", "PluginB"));  // second write violates policy
}

// ---- Error path ----

CASE("written_to_error") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.write("x", "PluginA");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("WRITTEN"));

    tracker.reportError("x", "write threw mid-way");  // WRITTEN → ERROR
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));
    EXPECT(tracker.hasErrors());
}

CASE("error_slot_rejects_further_writes") {
    // A poisoned slot must not receive further updates: writeback is disabled on error, and
    // any subsequent write() attempt is rejected so no write can land in model memory.
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    EXPECT(f.x.isWritable());

    tracker.reportError("x", "write failed");  // READY → ERROR
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));
    EXPECT_NOT(f.x.isWritable());  // write guard disabled immediately on error

    // A further write attempt on the poisoned slot is rejected (stays ERROR).
    EXPECT_THROWS(tracker.write("x", "PluginA"));
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));
}

CASE("report_error_transitions_to_error") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.reportError("x", "write failed");

    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));
    EXPECT(tracker.hasErrors());
}

CASE("multiple_errors_append") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.reportError("x", "first error");
    tracker.reportError("x", "second error");

    // submit() throws with the accumulated reason — verify both messages are present
    try {
        tracker.submit();
        EXPECT(false);  // should not reach here
    }
    catch (const std::exception& e) {
        std::string msg{e.what()};
        EXPECT(msg.find("first error") != std::string::npos);
        EXPECT(msg.find("second error") != std::string::npos);
    }
}

CASE("submit_throws_on_error") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.reportError("x", "something went wrong");
    EXPECT_THROWS(tracker.submit());
}

CASE("force_reset_clears_error") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.reportError("x", "bad write");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));
    EXPECT_NOT(f.x.isWritable());  // reportError already disabled the write guard

    EXPECT_NO_THROW(ManagerTestAccess::forceTrackerReset(tracker));
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));
    EXPECT(!tracker.hasErrors());
    EXPECT_NOT(f.x.isWritable());  // stays disabled (IDLE ⟹ not writable)
}

CASE("force_reset_disables_writeback_on_open_slot") {
    // forceReset() must disable writeback even for a slot that is still mid-cycle (WRITTEN,
    // hence writable) — resetting to IDLE without disabling would leave it writable outside a cycle.
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.write("x", "PluginA");  // WRITTEN — still writable, submit() never ran
    EXPECT(f.x.isWritable());

    EXPECT_NO_THROW(ManagerTestAccess::forceTrackerReset(tracker));
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));
    EXPECT_NOT(f.x.isWritable());  // forceReset disabled the guard (IDLE ⟹ not writable)
}

CASE("clear_error_recovers_slot") {
    WriteAuthorisation auth;
    auth.grant("PluginA", "x");
    auth.grant("PluginA", "y");
    data::ParameterValueTyped<int> x{0}, y{0};
    coupling::WriteBackTracker tracker{auth, WriteBackPolicy::single_writer};
    tracker.attachParam("x", &x);
    tracker.attachParam("y", &y);

    tracker.open();
    tracker.reportError("x", "x failed");
    tracker.write("y", "PluginA");

    EXPECT(tracker.hasErrors());
    EXPECT_NOT(x.isWritable());  // reportError already disabled the write guard on the poisoned slot
    ManagerTestAccess::clearTrackerError(tracker, "x");

    EXPECT_NOT(tracker.hasErrors());
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("IDLE"));
    EXPECT_NOT(x.isWritable());  // clearError keeps it disabled (IDLE ⟹ not writable)
    EXPECT_EQUAL(std::string(tracker.slotState("y")), std::string("WRITTEN"));  // y unaffected
    EXPECT(y.isWritable());                                                   // y still in-cycle
}

// ---- allAcknowledged / reset invariants ----

CASE("reset_throws_on_pending_slot") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.write("x", "PluginA");
    tracker.submit();
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("PENDING"));

    EXPECT_THROWS(tracker.reset());  // model hasn't acknowledged
}

CASE("all_acknowledged_semantics") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    EXPECT(tracker.allAcknowledged());  // fresh — all IDLE

    tracker.open();
    EXPECT(!tracker.allAcknowledged());  // READY

    tracker.write("x", "PluginA");
    EXPECT(!tracker.allAcknowledged());  // WRITTEN

    tracker.submit();
    EXPECT(!tracker.allAcknowledged());  // PENDING

    tracker.acknowledgeWriteback("x");
    EXPECT(tracker.allAcknowledged());  // ACKNOWLEDGED

    tracker.reset();
    EXPECT(tracker.allAcknowledged());  // back to IDLE
}

// ---- Diagnostics ----

CASE("pending_writebacks_correct") {
    WriteAuthorisation auth;
    auth.grant("PluginA", "x");
    auth.grant("PluginA", "y");
    data::ParameterValueTyped<int> x{0}, y{0};
    coupling::WriteBackTracker tracker{auth, WriteBackPolicy::single_writer};
    tracker.attachParam("x", &x);
    tracker.attachParam("y", &y);

    EXPECT(tracker.pendingWritebacks().empty());

    tracker.open();
    EXPECT(tracker.pendingWritebacks().empty());  // READY — not yet pending

    tracker.write("x", "PluginA");  // only x written
    tracker.submit();                // x → PENDING, y → IDLE (skipped)

    auto pending = tracker.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string("x"));

    tracker.acknowledgeWriteback("x");
    EXPECT(tracker.pendingWritebacks().empty());
    tracker.reset();
}

CASE("open_throws_if_not_idle") {
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.write("x", "PluginA");
    tracker.submit();  // x → PENDING

    EXPECT_THROWS(tracker.open());  // cannot open while PENDING
}

// ---- Robustness: ERROR slot behaviour ----

CASE("acknowledge_on_error_slot_stays_error") {
    // A model calling acknowledgeWriteback() on an ERROR slot must not crash or advance
    // the state incorrectly. ERROR slots never appear in pendingWritebacks(), so a well-behaved
    // model cannot reach this path — but a misbehaving caller must be safely absorbed.
    Fixture f;
    auto& tracker = *f.trackerPtr;

    tracker.open();
    tracker.reportError("x", "simulated failure");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));

    // Acknowledging an ERROR slot is silently absorbed — stays ERROR
    tracker.acknowledgeWriteback("x");
    EXPECT_EQUAL(std::string(tracker.slotState("x")), std::string("ERROR"));

    // ERROR slot does not appear in pendingWritebacks()
    EXPECT(tracker.pendingWritebacks().empty());
}

// ---- Auto-detach callback ----

CASE("tracker_destructor_nulls_modeldata_pointer") {
    // Verifies the auto-detach contract: when the tracker is destroyed, the non-owning
    // tracker_ pointer on ModelData is nulled automatically via the onDetach_ callback.
    // Without this guarantee, ModelData::tracker_ would dangle after Manager::teardown().

    WriteAuthorisation auth = oneAuth("PluginA", "x");
    data::ModelData data;
    data.createParam<int>("x", 0);

    auto tracker = std::make_unique<coupling::WriteBackTracker>(auth, WriteBackPolicy::single_writer);
    data.enrollWritebackParams(*tracker, auth);
    data.attachWritebackTracker(tracker.get());

    // Tracker is attached — pendingWritebacks() delegates to it (returns empty, no writes yet)
    EXPECT(data.pendingWritebacks().empty());

    // Destroy the tracker — callback should fire and null ModelData::tracker_
    tracker.reset();

    // ModelData::tracker_ is now null — acknowledgeWriteback throws the "not attached" error
    EXPECT_THROWS_AS(data.acknowledgeWriteback("x"), eckit::BadValue);
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
