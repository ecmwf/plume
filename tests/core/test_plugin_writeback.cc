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

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"

#include "ManagerTestAccess.h"
#include "plume/Manager.h"
#include "plume/data/ModelData.h"

using namespace eckit::testing;

namespace plume::test {

// Helpers

static const std::string kWriteBackPlugin = R"YAML(
write-back-policy: single-writer
plugins:
- lib: simple_plugins
  name: WriteBackPlugin
  core-config: {}
)YAML";

static const std::string kSkipWritePlugin = R"YAML(
write-back-policy: single-writer
plugins:
- lib: simple_plugins
  name: SkipWritePlugin
  core-config: {}
)YAML";

static const std::string kMultiWriterPlugins = R"YAML(
write-back-policy: multi-writer
plugins:
- lib: simple_plugins
  name: WriteBackPlugin
  core-config: {}
- lib: simple_plugins
  name: SkipWritePlugin
  core-config: {}
)YAML";

static void resetAndConfigure(const std::string& mgrConf) {
    ManagerTestAccess::reset();
    Manager::configure(eckit::YAMLConfiguration(mgrConf));
}

static void negotiate(bool writable = true) {
    std::string offered = R"YAML(offered:
- name: I
  type: INT
  available: always
  comment: none
- name: W_INT
  type: INT
  available: always
  comment: none
)YAML";
    if (writable) {
        offered += "  writable: true\n";
    }
    Manager::negotiate(eckit::YAMLConfiguration(offered));
}

// ---- Happy path ----

CASE("plugin_writes_model_acknowledges") {
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kWriteBackPlugin);
    negotiate();

    Manager::feedPlugins(data);
    Manager::run();

    // W_INT should be pending
    auto pending = data.pendingWritebacks();
    EXPECT_EQUAL(pending.size(), std::size_t(1));
    EXPECT_EQUAL(pending[0], std::string("W_INT"));

    // Value was written by the plugin
    EXPECT_EQUAL(data.getParam<int>("W_INT"), 42);

    // Model acknowledges
    data.acknowledgeWriteback("W_INT");
    EXPECT(data.pendingWritebacks().empty());

    Manager::teardown();
}

CASE("multi_cycle_run_resets_slots") {
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kWriteBackPlugin);
    negotiate();

    Manager::feedPlugins(data);

    for (int cycle = 0; cycle < 3; ++cycle) {
        Manager::run();

        auto pending = data.pendingWritebacks();
        EXPECT_EQUAL(pending.size(), std::size_t(1));
        EXPECT_EQUAL(data.getParam<int>("W_INT"), 42);

        data.acknowledgeWriteback("W_INT");
        EXPECT(data.pendingWritebacks().empty());
    }

    Manager::teardown();
}

CASE("plugin_skips_write_absent_from_pending") {
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kSkipWritePlugin);
    negotiate();

    Manager::feedPlugins(data);

    // Cycle 1: plugin writes (odd call)
    Manager::run();
    EXPECT_EQUAL(data.pendingWritebacks().size(), std::size_t(1));
    data.acknowledgeWriteback("W_INT");

    // Cycle 2: plugin skips (even call) — W_INT absent from pendingWritebacks()
    Manager::run();
    EXPECT(data.pendingWritebacks().empty());

    // Cycle 3: plugin writes again (odd call)
    Manager::run();
    EXPECT_EQUAL(data.pendingWritebacks().size(), std::size_t(1));
    data.acknowledgeWriteback("W_INT");

    Manager::teardown();
}

// ---- Write-back inactive when not negotiated ----

CASE("no_writeback_when_param_not_offered_writable") {
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kWriteBackPlugin);
    negotiate(/*writable=*/false);  // W_INT offered read-only — plugin's writable request is rejected

    // Plugin was not activated — feedPlugins and run should not throw
    Manager::feedPlugins(data);
    Manager::run();

    // No write-back active — pendingWritebacks() empty
    EXPECT(data.pendingWritebacks().empty());

    Manager::teardown();
}

// ---- Multi-writer policy ----

CASE("multi_writer_both_plugins_write") {
    // WriteBackPlugin always writes 42; SkipWritePlugin writes callCount*10 on odd calls.
    // Under multi-writer, last write in execution order wins (sequential composition).
    // Plugin order: WriteBackPlugin first, SkipWritePlugin second.
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kMultiWriterPlugins);
    negotiate();

    Manager::feedPlugins(data);

    // Cycle 1: both write → WriteBackPlugin=42, then SkipWritePlugin=10; last wins: 10
    Manager::run();
    EXPECT_EQUAL(data.pendingWritebacks().size(), std::size_t(1));
    EXPECT_EQUAL(data.getParam<int>("W_INT"), 10);
    data.acknowledgeWriteback("W_INT");

    // Cycle 2: only WriteBackPlugin writes (SkipWritePlugin skips even call) → 42
    Manager::run();
    EXPECT_EQUAL(data.pendingWritebacks().size(), std::size_t(1));
    EXPECT_EQUAL(data.getParam<int>("W_INT"), 42);
    data.acknowledgeWriteback("W_INT");

    // Cycle 3: both write again → WriteBackPlugin=42, SkipWritePlugin=30; last wins: 30
    Manager::run();
    EXPECT_EQUAL(data.pendingWritebacks().size(), std::size_t(1));
    EXPECT_EQUAL(data.getParam<int>("W_INT"), 30);
    data.acknowledgeWriteback("W_INT");

    Manager::teardown();
}

// ---- Teardown with unconfirmed slots ----

CASE("teardown_with_unconfirmed_slots_does_not_throw") {
    // Verifies that teardown() warns but does not throw when the model skips acknowledgeWriteback().
    data::ModelData data;
    data.createParam<int>("I", 0);
    data.createParam<int>("W_INT", 0);

    resetAndConfigure(kWriteBackPlugin);
    negotiate();

    Manager::feedPlugins(data);
    Manager::run();  // plugin writes 42; W_INT is now FLUSHED / pending

    // Deliberately skip acknowledgeWriteback() — teardown must not throw
    EXPECT_NO_THROW(Manager::teardown());
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
