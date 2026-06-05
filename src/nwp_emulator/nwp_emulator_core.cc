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
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "eckit/config/YAMLConfiguration.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/mpi/Comm.h"

#include "atlas/field/Field.h"

#include "plume/Manager.h"

#include "nwp_emulator_core.h"

namespace nwp_emulator {

NWPEmulatorRunResult NWPEmulatorCore::execute(const NWPEmulatorRunOptions& options) {
    NWPEmulatorRunResult result;
    if (!validateRunOptions(options)) {
        result.returnCode = 1;
        return result;
    }

    if (!setupDataProvider(options)) {
        result.returnCode = 1;
        return result;
    }

    if (!plumeConfigPath_.empty()) {
        if (!setupPlumeProvider()) {
            result.returnCode = 1;
            finalizePlume();
            return result;
        }
        result.plumeRun = true;
    }

    while (runStep()) {
        result.lastStepRun = currentStep();
    }

    finalizeRun();
    return result;
}

bool NWPEmulatorCore::validateRunOptions(const NWPEmulatorRunOptions& options) const {
    if (options.dataSourcePath.empty() || options.dataSourceType == DataSourceType::INVALID) {
        eckit::Log::error() << "Invalid emulator run options" << std::endl;
        return false;
    }
    return true;
}

bool NWPEmulatorCore::setupDataProvider(const NWPEmulatorRunOptions& options) {
    if (!validateRunOptions(options)) {
        return false;
    }

    plumeConfigPath_ = options.plumeConfigPath;
    plumeInitialised_ = false;

    const size_t root   = 0;
    const size_t nprocs = eckit::mpi::comm().size();
    const size_t rank   = eckit::mpi::comm().rank();
    dataProvider_ = std::make_unique<NWPDataProvider>(
        options.dataSourceType,
        eckit::PathName{options.dataSourcePath},
        rank,
        root,
        nprocs
    );
    return true;
}

bool NWPEmulatorCore::setupPlumeProvider() {
    if (!dataProvider_) {
        eckit::Log::error() << "Data provider must be initialized before plume setup" << std::endl;
        return false;
    }

    if (plumeConfigPath_.empty()) {
        return true;
    }

    eckit::Log::info() << "The emulator will run Plume with configuration '" << plumeConfigPath_ << "'"
                       << std::endl;
    plumeInitialised_ = setupPlume(*dataProvider_);
    return plumeInitialised_;
}

bool NWPEmulatorCore::runStep() {
    if (!dataProvider_) {
        return false;
    }

    try {
        if (!dataProvider_->getStepData()) {
            return false;
        }

        if (plumeInitialised_) {
            runPlume(dataProvider_->getStep());
        }

        // Keep ranks in lockstep during healthy execution.
        eckit::mpi::comm().barrier();
        return true;
    }
    catch (const std::exception& ex) {
        eckit::Log::error() << "runStep failed on rank " << eckit::mpi::comm().rank() << ": " << ex.what()
                            << std::endl;
        // Fail all ranks immediately to avoid barrier deadlocks when one rank throws.
        eckit::mpi::comm().abort(1);
        return false;
    }
}

void NWPEmulatorCore::finalizePlume() {
    if (plumeInitialised_) {
        plume::Manager::teardown();
        plumeInitialised_ = false;
    }
}

void NWPEmulatorCore::finalizeRun() {
    finalizePlume();

    const size_t rank = eckit::mpi::comm().rank();
    eckit::mpi::comm().barrier();
    std::cout << "Process " << rank << " finished..." << std::endl;
    eckit::Log::info() << "Emulator run completed..." << std::endl;
}

int NWPEmulatorCore::currentStep() const {
    if (!dataProvider_) {
        return -1;
    }
    return dataProvider_->getStep();
}

std::vector<std::string> NWPEmulatorCore::availableFieldKeys() const {
    if (!dataProvider_) {
        return {};
    }
    return dataProvider_->getFieldKeys();
}

NWPFieldOverlaySnapshot NWPEmulatorCore::getFieldOverlaySnapshot(const std::string& fieldKey) const {
    NWPFieldOverlaySnapshot snapshot;
    snapshot.fieldKey = fieldKey;

    if (!dataProvider_) {
        throw std::runtime_error("Data provider must be initialized before field export");
    }

    std::string error;
    const bool ok = dataProvider_->extractFieldOverlay(
        fieldKey,
        snapshot.lon,
        snapshot.lat,
        snapshot.values,
        error
    );
    if (!ok) {
        throw std::runtime_error("Field overlay export failed for " + fieldKey + ": " + error);
    }

    snapshot.step = currentStep();
    snapshot.rank = dataProvider_->rank();
    snapshot.root = dataProvider_->root();
    snapshot.nprocs = dataProvider_->nprocs();
    return snapshot;
}

bool NWPEmulatorCore::setupPlume(NWPDataProvider& dataProvider) {
    plume::Manager::configure(eckit::YAMLConfiguration(eckit::PathName(plumeConfigPath_)));

    const size_t modelLevels = dataProvider.getLevels();
    if (modelLevels > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("NFLEVG exceeds supported int range");
    }
    const int nflevg = static_cast<int>(modelLevels);

    // Record parameters updated every step so plugins do not need to infer which
    // values have changed between invocations.
    plumeUpdatingParams_.clear();

    plume::Protocol offers;  // Define data offered by the emulator to Plume.
    offers.offer<int>("NSTEP", "always", "Simulation Step");
    plumeUpdatingParams_.push_back("NSTEP");
    offers.offer<double>("TSTEP", "always", "Simulation Time Step");
    offers.offer<int>("NFLEVG", "always", "Number of vertical levels");
    offers.offer<double>("WSTEP", "always", "Wave simulation time");
    plumeUpdatingParams_.push_back("WSTEP");

    // Offer every model field so plugin negotiation can decide which fields are
    // actually needed for the configured plugin set.
    auto& fields = dataProvider.getModelFieldSet();
    for (const auto& field: fields) {
        offers.offer<atlas::Field>(field.name(), "on-request", field.name());
    }
    plume::Manager::negotiate(offers);

    // Scalar parameters are initialised once before the first plugin step.
    plumeData_.createParam("NSTEP", 0);
    plumeData_.createParam("TSTEP", 900.0);
    plumeData_.createParam("NFLEVG", nflevg);
    plumeData_.createParam("WSTEP", 0.0);

    // Only bind fields that plugins explicitly requested during negotiation.
    for (auto& field: fields) {
        if (plume::Manager::isParamRequested(field.name())) {
            plumeData_.provideParam(field.name(), &field);
            plumeUpdatingParams_.push_back(field.name());
        }
    }

    plume::Manager::feedPlugins(plumeData_);
    return true;
}

void NWPEmulatorCore::runPlume(int step) {
    // Update step metadata and mark backing fields as updated only after the
    // provider has populated this step's data.
    plumeData_.updateParam("NSTEP", step);
    plumeData_.updateParam("WSTEP", std::ceil(step * plumeData_.getParam<double>("TSTEP")));
    plumeData_.setUpdated(plumeUpdatingParams_);

    plume::Manager::run();
}

}  // namespace nwp_emulator
