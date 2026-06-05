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
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "plume/data/ModelData.h"

#include "nwp_data_provider.h"

namespace nwp_emulator {

/**
 * @struct NWPEmulatorRunOptions
 * @brief Typed execution options for a single emulator run.
 *
 * This is the stable contract that higher-level adapters should populate before
 * delegating to the reusable emulator core. It deliberately mirrors the current
 * CLI semantics: exactly one data source and an optional Plume configuration.
 */
struct NWPEmulatorRunOptions {
    DataSourceType dataSourceType = DataSourceType::INVALID;
    std::string dataSourcePath;
    std::string plumeConfigPath;
};

/**
 * @struct NWPEmulatorRunResult
 * @brief Minimal execution outcome returned by the core.
 *
 * This result is intentionally small for now. It records whether the run
 * succeeded, whether Plume was enabled, and the last completed model step so
 * future adapters can expose richer status without parsing log output.
 */
struct NWPEmulatorRunResult {
    int returnCode  = 0;
    bool plumeRun   = false;
    int lastStepRun = -1;
};

struct NWPFieldOverlaySnapshot {
    std::string fieldKey;
    std::vector<double> lon;
    std::vector<double> lat;
    std::vector<double> values;
    int step = -1;
    size_t rank = 0;
    size_t root = 0;
    size_t nprocs = 1;
};

/**
 * @class NWPEmulatorCore
 * @brief Reusable emulator backend independent from the AtlasTool CLI wrapper.
 *
 * The core owns the actual emulator run orchestration: data-provider setup,
 * optional Plume negotiation, step loop execution, and final teardown. The CLI
 * executable, and later Python bindings, should delegate into this class rather
 * than duplicating execution logic.
 */
class NWPEmulatorCore final {
public:
    /**
     * @brief Execute one emulator run from normalized options.
     *
     * @param options Fully normalized execution options.
     *
     * @return NWPEmulatorRunResult Summary of the completed run.
     */
    NWPEmulatorRunResult execute(const NWPEmulatorRunOptions& options);
    bool validateRunOptions(const NWPEmulatorRunOptions& options) const;
    bool setupDataProvider(const NWPEmulatorRunOptions& options);
    bool setupPlumeProvider();
    bool runStep();
    void finalizePlume();
    void finalizeRun();
    int currentStep() const;
    std::vector<std::string> availableFieldKeys() const;
    NWPFieldOverlaySnapshot getFieldOverlaySnapshot(const std::string& fieldKey) const;

private:
    /**
     * @brief Configure Plume and register all requested fields and scalar params.
     *
     * @param dataProvider Source of model fields exposed by the emulator.
     *
     * @return true when Plume setup completed successfully.
     */
    bool setupPlume(NWPDataProvider& dataProvider);

    /**
     * @brief Run Plume for the current step and update derived scalar values.
     *
     * @param step Current emulator step number.
     */
    void runPlume(int step);

    /// Empty means dry-run mode with emulator data generation only.
    std::string plumeConfigPath_;

    /// Shared param store passed to Plume across all steps of a single run.
    plume::data::ModelData plumeData_;
    std::vector<std::string> plumeUpdatingParams_;
    bool plumeInitialised_{false};
    std::unique_ptr<NWPDataProvider> dataProvider_;
};

}  // namespace nwp_emulator
