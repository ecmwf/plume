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
#include "plume/coupling/WriteBackLedger.h"
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
/// Non-copyable — ledger holds a reference to auth, param slot holds a pointer to x.
struct Fixture {
    WriteAuthorisation auth = oneAuth("PluginA", "x");
    data::ParameterValueTyped<int> x{0};
    std::unique_ptr<coupling::WriteBackLedger> ledgerPtr{
        std::make_unique<coupling::WriteBackLedger>(auth, WriteBackPolicy::single_writer)};

    Fixture() { ledgerPtr->attachParam("x", &x); }
    Fixture(const Fixture&)            = delete;
    Fixture& operator=(const Fixture&) = delete;
};

// ---- State machine — happy path ----

CASE("single_writer_full_cycle") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));

    ledger.open();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("READY"));
    EXPECT(f.x.isWritable());

    ledger.stage("x", "PluginA");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("STAGED"));

    ledger.flush();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("FLUSHED"));
    EXPECT_NOT(f.x.isWritable());

    auto pending = ledger.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string("x"));

    ledger.acknowledgeWriteback("x");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("CONFIRMED"));
    EXPECT(ledger.allConfirmed());

    ledger.reset();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));
}

CASE("slot_not_written_silently_skips") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("READY"));

    ledger.flush();  // no stage() — READY → IDLE silently
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));

    EXPECT(ledger.pendingWritebacks().empty());
    EXPECT(ledger.allConfirmed());
}

CASE("multi_cycle_run") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    for (int cycle = 0; cycle < 3; ++cycle) {
        ledger.reset();  // no-op on first cycle (all IDLE); CONFIRMED → IDLE on subsequent
        ledger.open();
        EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("READY"));

        ledger.stage("x", "PluginA");
        ledger.flush();
        ledger.acknowledgeWriteback("x");
        EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("CONFIRMED"));
    }
}

CASE("multi_writer_allowed") {
    auto auth = twoPluginsOneParam("PluginA", "PluginB", "x");
    data::ParameterValueTyped<int> x{0};
    coupling::WriteBackLedger ledger{auth, WriteBackPolicy::multi_writer};
    ledger.attachParam("x", &x);

    ledger.open();
    ledger.stage("x", "PluginA");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("STAGED"));

    EXPECT_NO_THROW(ledger.stage("x", "PluginB"));
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("STAGED"));

    ledger.flush();
    ledger.acknowledgeWriteback("x");
    ledger.reset();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));
}

// ---- Authorisation enforcement ----

CASE("unauthorised_plugin_throws") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    EXPECT_THROWS(ledger.stage("x", "PluginB"));  // PluginB not authorised
}

CASE("stage_on_closed_slot_throws") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    EXPECT_THROWS(ledger.stage("x", "PluginA"));  // slot still IDLE — open() not called
}

CASE("single_writer_violation_throws") {
    auto auth = twoPluginsOneParam("PluginA", "PluginB", "x");
    data::ParameterValueTyped<int> x{0};
    coupling::WriteBackLedger ledger{auth, WriteBackPolicy::single_writer};
    ledger.attachParam("x", &x);

    ledger.open();
    ledger.stage("x", "PluginA");
    EXPECT_THROWS(ledger.stage("x", "PluginB"));  // second write violates policy
}

// ---- Error path ----

CASE("staged_to_error") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.stage("x", "PluginA");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("STAGED"));

    ledger.reportError("x", "write threw mid-way");  // STAGED → ERROR
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));
    EXPECT(ledger.hasErrors());
}

CASE("error_slot_rejects_further_writes") {
    // A poisoned slot must not receive further updates: writeback is disabled on error, and
    // any subsequent stage() attempt is rejected so no write can land in model memory.
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    EXPECT(f.x.isWritable());

    ledger.reportError("x", "write failed");  // READY → ERROR
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));
    EXPECT_NOT(f.x.isWritable());  // write guard disabled immediately on error

    // A further write attempt on the poisoned slot is rejected (stays ERROR).
    EXPECT_THROWS(ledger.stage("x", "PluginA"));
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));
}

CASE("report_error_transitions_to_error") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.reportError("x", "write failed");

    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));
    EXPECT(ledger.hasErrors());
}

CASE("multiple_errors_append") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.reportError("x", "first error");
    ledger.reportError("x", "second error");

    // flush() throws with the accumulated reason — verify both messages are present
    try {
        ledger.flush();
        EXPECT(false);  // should not reach here
    }
    catch (const std::exception& e) {
        std::string msg{e.what()};
        EXPECT(msg.find("first error") != std::string::npos);
        EXPECT(msg.find("second error") != std::string::npos);
    }
}

CASE("flush_throws_on_error") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.reportError("x", "something went wrong");
    EXPECT_THROWS(ledger.flush());
}

CASE("force_reset_clears_error") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.reportError("x", "bad write");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));
    EXPECT_NOT(f.x.isWritable());  // reportError already disabled the write guard

    EXPECT_NO_THROW(ManagerTestAccess::forceLedgerReset(ledger));
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));
    EXPECT(!ledger.hasErrors());
    EXPECT_NOT(f.x.isWritable());  // stays disabled (IDLE ⟹ not writable)
}

CASE("force_reset_disables_writeback_on_open_slot") {
    // forceReset() must disable writeback even for a slot that is still mid-cycle (STAGED,
    // hence writable) — resetting to IDLE without disabling would leave it writable outside a cycle.
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.stage("x", "PluginA");  // STAGED — still writable, flush() never ran
    EXPECT(f.x.isWritable());

    EXPECT_NO_THROW(ManagerTestAccess::forceLedgerReset(ledger));
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));
    EXPECT_NOT(f.x.isWritable());  // forceReset disabled the guard (IDLE ⟹ not writable)
}

CASE("clear_error_recovers_slot") {
    WriteAuthorisation auth;
    auth.grant("PluginA", "x");
    auth.grant("PluginA", "y");
    data::ParameterValueTyped<int> x{0}, y{0};
    coupling::WriteBackLedger ledger{auth, WriteBackPolicy::single_writer};
    ledger.attachParam("x", &x);
    ledger.attachParam("y", &y);

    ledger.open();
    ledger.reportError("x", "x failed");
    ledger.stage("y", "PluginA");

    EXPECT(ledger.hasErrors());
    EXPECT_NOT(x.isWritable());  // reportError already disabled the write guard on the poisoned slot
    ManagerTestAccess::clearLedgerError(ledger, "x");

    EXPECT_NOT(ledger.hasErrors());
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("IDLE"));
    EXPECT_NOT(x.isWritable());  // clearError keeps it disabled (IDLE ⟹ not writable)
    EXPECT_EQUAL(std::string(ledger.slotState("y")), std::string("STAGED"));  // y unaffected
    EXPECT(y.isWritable());                                                   // y still in-cycle
}

// ---- allConfirmed / reset invariants ----

CASE("reset_throws_on_flushed_slot") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.stage("x", "PluginA");
    ledger.flush();
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("FLUSHED"));

    EXPECT_THROWS(ledger.reset());  // model hasn't acknowledged
}

CASE("all_confirmed_semantics") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    EXPECT(ledger.allConfirmed());  // fresh — all IDLE

    ledger.open();
    EXPECT(!ledger.allConfirmed());  // READY

    ledger.stage("x", "PluginA");
    EXPECT(!ledger.allConfirmed());  // STAGED

    ledger.flush();
    EXPECT(!ledger.allConfirmed());  // FLUSHED

    ledger.acknowledgeWriteback("x");
    EXPECT(ledger.allConfirmed());  // CONFIRMED

    ledger.reset();
    EXPECT(ledger.allConfirmed());  // back to IDLE
}

// ---- Diagnostics ----

CASE("pending_writebacks_correct") {
    WriteAuthorisation auth;
    auth.grant("PluginA", "x");
    auth.grant("PluginA", "y");
    data::ParameterValueTyped<int> x{0}, y{0};
    coupling::WriteBackLedger ledger{auth, WriteBackPolicy::single_writer};
    ledger.attachParam("x", &x);
    ledger.attachParam("y", &y);

    EXPECT(ledger.pendingWritebacks().empty());

    ledger.open();
    EXPECT(ledger.pendingWritebacks().empty());  // READY — not yet pending

    ledger.stage("x", "PluginA");  // only x written
    ledger.flush();                // x → FLUSHED, y → IDLE (skipped)

    auto pending = ledger.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string("x"));

    ledger.acknowledgeWriteback("x");
    EXPECT(ledger.pendingWritebacks().empty());
    ledger.reset();
}

CASE("open_throws_if_not_idle") {
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.stage("x", "PluginA");
    ledger.flush();  // x → FLUSHED

    EXPECT_THROWS(ledger.open());  // cannot open while FLUSHED
}

// ---- Robustness: ERROR slot behaviour ----

CASE("acknowledge_on_error_slot_stays_error") {
    // A model calling acknowledgeWriteback() on an ERROR slot must not crash or advance
    // the state incorrectly. ERROR slots never appear in pendingWritebacks(), so a well-behaved
    // model cannot reach this path — but a misbehaving caller must be safely absorbed.
    Fixture f;
    auto& ledger = *f.ledgerPtr;

    ledger.open();
    ledger.reportError("x", "simulated failure");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));

    // Acknowledging an ERROR slot is silently absorbed — stays ERROR
    ledger.acknowledgeWriteback("x");
    EXPECT_EQUAL(std::string(ledger.slotState("x")), std::string("ERROR"));

    // ERROR slot does not appear in pendingWritebacks()
    EXPECT(ledger.pendingWritebacks().empty());
}

// ---- Auto-detach callback ----

CASE("ledger_destructor_nulls_modeldata_pointer") {
    // Verifies the auto-detach contract: when the ledger is destroyed, the non-owning
    // ledger_ pointer on ModelData is nulled automatically via the onDetach_ callback.
    // Without this guarantee, ModelData::ledger_ would dangle after Manager::teardown().

    WriteAuthorisation auth = oneAuth("PluginA", "x");
    data::ModelData data;
    data.createParam<int>("x", 0);

    auto ledger = std::make_unique<coupling::WriteBackLedger>(auth, WriteBackPolicy::single_writer);
    data.enrollWritebackParams(*ledger, auth);
    data.attachWritebackLedger(ledger.get());

    // Ledger is attached — pendingWritebacks() delegates to it (returns empty, no writes yet)
    EXPECT(data.pendingWritebacks().empty());

    // Destroy the ledger — callback should fire and null ModelData::ledger_
    ledger.reset();

    // ModelData::ledger_ is now null — acknowledgeWriteback throws the "not attached" error
    EXPECT_THROWS_AS(data.acknowledgeWriteback("x"), eckit::BadValue);
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
