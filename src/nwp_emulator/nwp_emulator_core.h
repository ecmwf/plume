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

/**
 * @struct NWPFieldOverlaySnapshot
 * @brief Local slice of a field overlay captured after a model step.
 *
 * Holds the longitudes, latitudes and field values for the portion of the global
 * grid assigned to this MPI rank. The MPI context fields (rank, root, nprocs)
 * allow callers to distinguish per-rank slices and to aggregate results across
 * a distributed run. This struct is intended for use by higher-level adapters, 
 * e.g., applications that want to visualise or analyse the emulator output.
 */
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
     * @brief Tear down the core, finalising Plume before members are destroyed.
     */
    ~NWPEmulatorCore();

    /**
     * @brief Execute one full emulator run from normalized options.
     *
     * Runs the complete lifecycle: data-provider setup, optional Plume
     * negotiation, the step loop, and teardown.
     *
     * @param options Fully normalized execution options.
     *
     * @return Summary of the completed run.
     */
    NWPEmulatorRunResult execute(const NWPEmulatorRunOptions& options);

    /**
     * @brief Check that run options are complete and internally consistent.
     */
    bool validateRunOptions(const NWPEmulatorRunOptions& options) const;

    /**
     * @brief Initialise the data provider from the given options.
     *
     * @note May only be called once per instance; a second call is rejected.
     */
    bool setupDataProvider(const NWPEmulatorRunOptions& options);

    /**
     * @brief Configure Plume and negotiate field registration.
     *
     * @return true when Plume setup completed or was skipped.
     */
    bool setupPlumeProvider();

    /**
     * @brief Advance the emulator by one model step.
     *
     * @return true while more steps remain, false once the source is exhausted.
     */
    bool runStep();

    /**
     * @brief Tear down Plume, releasing all plugin resources.
     *
     * @note Safe to call when Plume was never initialised.
     */
    void finalizePlume();

    /**
     * @brief Finalise the run, tearing down Plume and synchronising all ranks.
     */
    void finalizeRun();

    /**
     * @brief Return the index of the last completed model step.
     *
     * @return The current step, or -1 before the first step.
     */
    int currentStep() const;

    /**
     * @brief List all field keys exposed by the data provider.
     *
     * @return Field keys in "shortName,levtype,level" format.
     */
    std::vector<std::string> availableFieldKeys() const;

    /**
     * @brief Capture the rank-local overlay for a field at the current step, intendended for visualisation or analysis.
     *
     * @return The rank-local field slice and MPI topology metadata.
     */
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
    bool plumeInitialised_{false};

    // dataProvider_ is declared before plumeData_ so it is destroyed after plumeData_.
    // plumeData_ holds non-owning raw pointers into the atlas FieldSet owned by
    // dataProvider_; those pointers must remain valid for the full lifetime of plumeData_.
    std::unique_ptr<NWPDataProvider> dataProvider_;

    /// Shared param store passed to Plume across all steps of a single run.
    plume::data::ModelData plumeData_;
    std::vector<std::string> plumeUpdatingParams_;
};

}  // namespace nwp_emulator
