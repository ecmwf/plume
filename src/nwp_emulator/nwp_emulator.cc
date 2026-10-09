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
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <vector>

#include "eckit/log/Log.h"

#include "atlas/library.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/runtime/AtlasTool.h"

#include "nwp_definitions.h"
#include "nwp_emulator_core.h"

using namespace nwp_emulator;

namespace {

/// Split a comma-separated string into trimmed, non-empty tokens.
std::vector<std::string> splitCommaList(const std::string& csv) {
    std::vector<std::string> tokens;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const size_t start = token.find_first_not_of(" \t");
        const size_t end   = token.find_last_not_of(" \t");
        if (start != std::string::npos) {
            tokens.push_back(token.substr(start, end - start + 1));
        }
    }
    return tokens;
}

}  // namespace

/**
 * @class NWPEmulator
 * @brief Emulates a model run and makes data available at each time step to facilitate Plume and plugins testing.
 */
class NWPEmulator final : public atlas::AtlasTool {

    int execute(const Args& args) override;
    std::string briefDescription() override { return "NWP model emulator to facilitate Plume & plugins development"; }
    std::string usage() override {
        return name() +
               " [--grib-src=<path> | --config-src=<path>] [--plume-cfg=<path>] [--writable=all|<name1,name2,...>] "
               "[OPTION]... [--help]\n"
               "        --plume-cfg is optional, pass it to run Plume, else the emulator will do a dry run\n"
               "        --writable is optional: offer all model parameters as writable (\"all\"), or a "
               "comma-separated subset (e.g. \"u,v,msl\"), so plugins may negotiate write-back access. Has no "
               "effect without --plume-cfg.\n";
    }

    int numberOfPositionalArguments() override { return -1; }
    int minimumPositionalArguments() override { return 0; }

public:
    NWPEmulator(int argc, char** argv) : dataSourceType_(DataSourceType::INVALID), atlas::AtlasTool(argc, argv) {
        // Preserve the existing CLI surface while delegating execution to the core.
        add_option(new SimpleOption<std::string>("grib-src", "Path to GRIB files source"));
        add_option(new SimpleOption<std::string>("config-src", "Path to emulator config file"));
        add_option(new SimpleOption<std::string>("plume-cfg", "Path to Plume configuration"));
        add_option(new SimpleOption<std::string>(
            "writable", "Offer parameters as writable: \"all\", or a comma-separated subset (e.g. \"u,v,msl\")"));
    }

private:
    std::string dataSourcePath_;
    DataSourceType dataSourceType_;

    // Empty means dry-run mode (no Plume execution).
    std::string plumeConfigPath_;

    bool writableAll_{false};
    std::vector<std::string> writableParams_;
};

int NWPEmulator::execute(const Args& args) {
    // Parse CLI options and normalize to core run options.
    std::string gribSrcArg;
    if (args.get("grib-src", gribSrcArg)) {
        dataSourcePath_ = gribSrcArg;
        dataSourceType_ = DataSourceType::GRIB;
    }
    std::string configSrcPath;
    if (args.get("config-src", configSrcPath)) {
        if (dataSourceType_ != DataSourceType::INVALID) {
            eckit::Log::error() << "Usage : " << usage() << std::endl;
            return 1;
        }
        dataSourcePath_ = configSrcPath;
        dataSourceType_ = DataSourceType::CONFIG;
    }
    if (dataSourcePath_.empty()) {
        eckit::Log::error() << "Usage : " << usage() << std::endl;
        return 1;
    }
    args.get("plume-cfg", plumeConfigPath_);
    std::string writableArg;
    if (args.get("writable", writableArg)) {
        if (writableArg == "all") {
            writableAll_ = true;
        }
        else {
            writableParams_ = splitCommaList(writableArg);
        }
    }

    // The AtlasTool wrapper is now a thin adapter over the reusable core.
    NWPEmulatorCore core;
    NWPEmulatorRunOptions options{dataSourceType_, dataSourcePath_, plumeConfigPath_, writableAll_, writableParams_};

    NWPEmulatorRunResult result = core.execute(options);
    return result.returnCode;
}

int main(int argc, char** argv) {
    // Atlas logging is expected by the existing emulator toolchain and remains
    // configured at process startup before the AtlasTool lifecycle begins.
    setenv("ATLAS_LOG_FILE", "true", 1);
    NWPEmulator emulator(argc, argv);
    return emulator.start();
}