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
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "eckit/filesystem/PathName.h"
#include "eckit/testing/Test.h"

#include "nwp_emulator/nwp_definitions.h"
#include "nwp_emulator/nwp_emulator_core.h"

using namespace eckit::testing;
using namespace nwp_emulator;

namespace {

eckit::PathName getenv_or_empty(const char* key) {
    const char* value = std::getenv(key);
    return value ? eckit::PathName(value) : eckit::PathName();
}

// Returns the field key for "u" (any levtype/level) from the keys the data provider exposes.
std::string findFieldKey(const std::vector<std::string>& keys, const std::string& shortName) {
    for (const auto& key: keys) {
        if (key.compare(0, shortName.size() + 1, shortName + ",") == 0) {
            return key;
        }
    }
    return {};
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace

namespace plume::test {

// The write-back plugin (NWPEmulatorWriteBackPlugin, see nwp_emulator_writeback_plugin.h) requires write access to
// "u" and, when accepted, overwrites every point/level of that field with a fixed sentinel value (12345.0).
// This single in-process case exercises RunOptions::writableParams directly through NWPEmulatorCore; it is the only
// Plume run in this process because the Plume Manager is a process-level singleton (see NWPEmulatorCore::execute
// docs), so CLI-level acceptance/rejection scenarios below are checked out-of-process instead.

CASE("test_writable_params_reach_plugin_writeback") {
    const eckit::PathName testDataDir  = getenv_or_empty("TEST_DATA_DIR");
    const eckit::PathName plumeCfgPath = getenv_or_empty("PLUME_WRITEBACK_CONFIG");
    EXPECT_NOT_EQUAL(testDataDir.asString(), "/");
    EXPECT_NOT_EQUAL(plumeCfgPath.asString(), "/");

    NWPEmulatorRunOptions options;
    options.dataSourceType  = DataSourceType::CONFIG;
    options.dataSourcePath  = (testDataDir + "valid_config.yml").asString();
    options.plumeConfigPath = plumeCfgPath.asString();
    // Offer only "u" as writable: exercises the "--writable=<subset>" CLI form.
    options.writableParams = {"u"};

    NWPEmulatorCore core;
    EXPECT(core.setupDataProvider(options));
    EXPECT(core.setupPlumeProvider());
    EXPECT(core.runStep());

    const std::string uKey = findFieldKey(core.availableFieldKeys(), "u");
    EXPECT(!uKey.empty());

    const NWPFieldOverlaySnapshot snapshot = core.getFieldOverlaySnapshot(uKey);
    EXPECT(!snapshot.values.empty());
    for (const double value: snapshot.values) {
        EXPECT_EQUAL(value, 12345.0);
    }

    core.finalizeRun();
}

// The next case runs the CLI binary out-of-process (mirroring test_core_parity.cc's parity test) so each invocation
// gets a fresh Plume Manager singleton, letting us check both sides of the negotiation without conflicting
// in-process Manager state.

CASE("test_writable_cli_flag_required_for_writeback") {
    const eckit::PathName testDataDir  = getenv_or_empty("TEST_DATA_DIR");
    const eckit::PathName runBin       = getenv_or_empty("NWP_EMULATOR_RUN_BIN");
    const eckit::PathName plumeCfgPath = getenv_or_empty("PLUME_WRITEBACK_CONFIG");
    EXPECT_NOT_EQUAL(testDataDir.asString(), "/");
    EXPECT_NOT_EQUAL(runBin.asString(), "/");
    EXPECT_NOT_EQUAL(plumeCfgPath.asString(), "/");

    const eckit::PathName configPath = testDataDir + "valid_config.yml";

    // Without --writable, "u" is offered read-only: the plugin's requireWritable("u") must be rejected, and no
    // write-back can happen, but the run itself still succeeds (Plume simply runs with no accepted plugins).
    const eckit::PathName outputPath = eckit::PathName::unique(testDataDir + "writable_cli_output");
    const std::string cmdWithoutFlag = runBin.asString() + " --config-src=" + configPath.asString() +
                                       " --plume-cfg=" + plumeCfgPath.asString() + " > " + outputPath.asString() +
                                       " 2>&1";
    const int statusWithoutFlag = std::system(cmdWithoutFlag.c_str());
    const std::string outputWithoutFlag = readFile(outputPath.asString());
    outputPath.unlink();

    EXPECT_EQUAL(statusWithoutFlag, 0);
    EXPECT(outputWithoutFlag.find("PluginDecision: REJECTED") != std::string::npos);
    EXPECT(outputWithoutFlag.find("wrote back into 'u'") == std::string::npos);

    // With --writable=u, "u" is offered writable: the plugin is accepted and writes back.
    const eckit::PathName outputPath2 = eckit::PathName::unique(testDataDir + "writable_cli_output");
    const std::string cmdWithFlag = runBin.asString() + " --config-src=" + configPath.asString() +
                                    " --plume-cfg=" + plumeCfgPath.asString() + " --writable=u" + " > " +
                                    outputPath2.asString() + " 2>&1";
    const int statusWithFlag = std::system(cmdWithFlag.c_str());
    const std::string outputWithFlag = readFile(outputPath2.asString());
    outputPath2.unlink();

    EXPECT_EQUAL(statusWithFlag, 0);
    EXPECT(outputWithFlag.find("PluginDecision: ACCEPTED") != std::string::npos);
    EXPECT(outputWithFlag.find("wrote back into 'u'") != std::string::npos);
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
