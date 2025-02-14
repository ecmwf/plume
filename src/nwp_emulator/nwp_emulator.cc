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
//#include <unistd.h>
#include <iostream>
#include <stdlib.h>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"

#include "atlas/library.h"
#include "atlas/option/Options.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/runtime/AtlasTool.h"

#include "plume/Manager.h"
#include "plume/data/ModelData.h"
#include "plume/data/ParameterCatalogue.h"

#include "nwp_data_provider.h"
#include "nwp_definitions.h"

using namespace nwp_emulator;

/**
 * @class NWPEmulator
 * @brief Emulates a model run and makes data available at each time step to facilitate Plume and plugins testing.
 */
class NWPEmulator : public atlas::AtlasTool {

    int execute(const Args& args) override;
    std::string briefDescription() override { return "NWP model emulator to facilitate Plume & plugins development"; }
    std::string usage() override {
        return name() +
               " [--grib-src=<path> | --config-src=<path>] [--plume-cfg=<path>] [OPTION]... [--help]\n"
               "        --plume-cfg is optional, pass it to run Plume, else the emulator will do a dry run\n";
    }

    int numberOfPositionalArguments() override { return -1; }
    int minimumPositionalArguments() override { return 0; }

public:
    NWPEmulator(int argc, char** argv) : dataSourceType_(DataSourceType::INVALID), atlas::AtlasTool(argc, argv) {
        add_option(new SimpleOption<std::string>("grib-src", "Path to GRIB files source"));
        add_option(new SimpleOption<std::string>("config-src", "Path to emulator config file"));
        add_option(new SimpleOption<std::string>("plume-cfg", "Path to Plume configuration"));
    }

private:
    std::string dataSourcePath_;
    DataSourceType dataSourceType_;

    /// Plume members if needed
    std::string plumeConfigPath_;
    plume::data::ModelData plumeData_;

    /**
     * @brief Sets up Plume framework and load plugins compatible with model params.
     *
     * @param dataProvider The object that provides the data for the Atlas fields offered by the emulated model.
     *
     * @return true if Plume configuration, negotiation and feeding are successful, false otherwise.
     */
    bool setupPlume(NWPDataProvider& dataProvider);

    /**
     * @brief Update necessary parameters offered to Plume other than Atlas fields, and run the plugins for the step.
     *
     * @param step The internal model step number.
     */
    void runPlume(int step);
};

int NWPEmulator::execute(const Args& args) {
    // Emulator configuration
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

    size_t root   = 0;
    size_t nprocs = eckit::mpi::comm().size();
    size_t rank   = eckit::mpi::comm().rank();

    NWPDataProvider dataProvider(dataSourceType_, eckit::PathName{dataSourcePath_}, rank, root, nprocs);

    // Plume loading if a Plume configuration file has been passed, emulator dry run otherwise
    if (!plumeConfigPath_.empty()) {
        eckit::Log::info() << "The emulator will run Plume with configuration '" << plumeConfigPath_ << "'"
                           << std::endl;
        setupPlume(dataProvider);
    }

    // Run the emulator
    while (dataProvider.getStepData()) {
        // This is a model step
        if (!plumeConfigPath_.empty()) {
            runPlume(dataProvider.getStep());
        }
        eckit::mpi::comm().barrier();
    }

    // Tear down where appropriate and wait for all processes before finishing
    if (!plumeConfigPath_.empty()) {
        plume::Manager::teardown();
    }

    eckit::mpi::comm().barrier();
    std::cout << "Process " << rank << " finished..." << std::endl;
    eckit::Log::info() << "Emulator run completed..." << std::endl;
    return 0;
}

bool NWPEmulator::setupPlume(NWPDataProvider& dataProvider) {
    plume::Manager::configure(eckit::YAMLConfiguration(eckit::PathName(plumeConfigPath_)));

    plume::Protocol offers;  /// Define data offered by Plume
    offers.offerInt("NSTEP", "always", "Simulation Step");
    auto fields = dataProvider.getModelFieldSet();
    for (size_t i = 0; i < fields.size(); ++i) {
        offers.offerAtlasField(fields[i].name(), "on-request", fields[i].name());
    }
    plume::Manager::negotiate(offers);
    plumeData_.createInt("NSTEP", 0);  /// Initialise parameters
    for (size_t i = 0; i < fields.size(); ++i) {
        if (plume::Manager::isParamRequested(fields[i].name())) {
            plumeData_.provideAtlasFieldShared(fields[i].name(), fields[i].get());
        }
    }

    plume::Manager::feedPlugins(plumeData_);
    return true;
}

void NWPEmulator::runPlume(int step) {
    plumeData_.updateInt("NSTEP", step);
    plume::Manager::run();
}

int main(int argc, char** argv) {
    setenv("ATLAS_LOG_FILE", "true", 1);
    NWPEmulator emulator(argc, argv);
    return emulator.start();
}