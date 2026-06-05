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

#include <cstdlib>
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

}  // namespace

namespace plume::test {

CASE("test_core_execute_invalid_options") {
    NWPEmulatorCore core;
    NWPEmulatorRunOptions options;

    NWPEmulatorRunResult result = core.execute(options);
    EXPECT_EQUAL(result.returnCode, 1);
    EXPECT(!result.plumeRun);
    EXPECT_EQUAL(result.lastStepRun, -1);
}

CASE("test_core_and_cli_adapter_parity_dry_run") {
    const eckit::PathName testDataDir = getenv_or_empty("TEST_DATA_DIR");
    const eckit::PathName runBin = getenv_or_empty("NWP_EMULATOR_RUN_BIN");

    EXPECT_NOT_EQUAL(testDataDir.asString(), "/");
    EXPECT_NOT_EQUAL(runBin.asString(), "/");

    const eckit::PathName configPath = testDataDir + "valid_config.yml";

    NWPEmulatorCore core;
    NWPEmulatorRunOptions options;
    options.dataSourceType = DataSourceType::CONFIG;
    options.dataSourcePath = configPath.asString();

    NWPEmulatorRunResult coreResult = core.execute(options);

    const std::string cliCommand = runBin.asString() + " --config-src=" + configPath.asString();
    const int cliStatus = std::system(cliCommand.c_str());

    const bool coreSuccess = (coreResult.returnCode == 0);
    const bool cliSuccess = (cliStatus == 0);

    // Parity criterion for this first integration test: both paths should agree
    // on success/failure outcome for the same normalized dry-run configuration.
    EXPECT_EQUAL(coreSuccess, cliSuccess);
    if (coreSuccess) {
        EXPECT(coreResult.lastStepRun >= 0);
    }
}

}  // namespace plume::test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
