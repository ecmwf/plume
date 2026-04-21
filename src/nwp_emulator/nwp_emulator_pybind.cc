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
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cstdlib>

#include "eckit/runtime/Main.h"

#include "nwp_emulator_core.h"

namespace py = pybind11;

#ifndef NWP_EMULATOR_PYTHON_MODULE_NAME
#define NWP_EMULATOR_PYTHON_MODULE_NAME nwp_emulator_core
#endif

namespace nwp_emulator {

namespace {

void ensure_runtime_initialised() {
    // Keep Atlas/eckit runtime expectations aligned with the CLI path.
    ::setenv("ATLAS_LOG_FILE", "true", 1);

    if (!eckit::Main::ready()) {
        static const int argc = 1;
        static char* argv[]   = {const_cast<char*>("nwp_emulator_pybind"), nullptr};
        eckit::Main::initialise(argc, argv);
    }

    // Note: MPI initialization is handled by eckit::Main::initialise() above.
    // When workers are spawned under mpirun, the MPI environment is already
    // set up by the MPI launcher, and the eckit runtime will use it properly.
}

}  // namespace

/**
 * @brief Register the initial Python-facing bindings for the reusable emulator core.
 *
 * This scaffold exposes the typed execution surface: source-type enum, run options,
 * run result, and execute entrypoints. For multi-rank execution, workers
 * are spawned under mpirun which establishes the MPI context before Python imports
 * this module. The eckit::Main::initialise() call ensures proper runtime setup.
 */
PYBIND11_MODULE(NWP_EMULATOR_PYTHON_MODULE_NAME, module) {
    module.doc() = "Python bindings for the reusable NWP emulator core";

    py::enum_<DataSourceType>(module, "DataSourceType")
        .value("GRIB", DataSourceType::GRIB)
        .value("CONFIG", DataSourceType::CONFIG)
        .value("INVALID", DataSourceType::INVALID);

    py::class_<NWPEmulatorRunOptions>(module, "RunOptions")
        .def(py::init<>())
        .def_readwrite("data_source_type", &NWPEmulatorRunOptions::dataSourceType)
        .def_readwrite("data_source_path", &NWPEmulatorRunOptions::dataSourcePath)
        .def_readwrite("plume_config_path", &NWPEmulatorRunOptions::plumeConfigPath);

    py::class_<NWPEmulatorRunResult>(module, "RunResult")
        .def(py::init<>())
        .def_readonly("return_code", &NWPEmulatorRunResult::returnCode)
        .def_readonly("plume_run", &NWPEmulatorRunResult::plumeRun)
        .def_readonly("last_step_run", &NWPEmulatorRunResult::lastStepRun);

    py::class_<NWPEmulatorCore>(module, "NWPEmulatorCore")
        .def(py::init<>())
        .def(
            "validate_run_options",
            [](NWPEmulatorCore& core, const NWPEmulatorRunOptions& options) {
                ensure_runtime_initialised();
                return core.validateRunOptions(options);
            },
            py::arg("options")
        )
        .def(
            "setup_data_provider",
            [](NWPEmulatorCore& core, const NWPEmulatorRunOptions& options) {
                ensure_runtime_initialised();
                return core.setupDataProvider(options);
            },
            py::arg("options")
        )
        .def(
            "setup_plume_provider",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                return core.setupPlumeProvider();
            }
        )
        .def(
            "run_step",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                return core.runStep();
            }
        )
        .def(
            "finalize_plume",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                core.finalizePlume();
            }
        )
        .def(
            "finalize_run",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                core.finalizeRun();
            }
        )
        .def(
            "current_step",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                return core.currentStep();
            }
        )
        .def(
            "available_field_keys",
            [](NWPEmulatorCore& core) {
                ensure_runtime_initialised();
                return core.availableFieldKeys();
            }
        )
        .def(
            "get_field_overlay_snapshot",
            [](NWPEmulatorCore& core, const std::string& field_key) {
                ensure_runtime_initialised();
                const auto snapshot = core.getFieldOverlaySnapshot(field_key);
                py::dict payload;
                payload["field_key"] = snapshot.fieldKey;
                payload["lon"] = snapshot.lon;
                payload["lat"] = snapshot.lat;
                payload["values"] = snapshot.values;
                payload["step"] = snapshot.step;
                payload["rank"] = snapshot.rank;
                payload["root"] = snapshot.root;
                payload["nprocs"] = snapshot.nprocs;
                return payload;
            },
            py::arg("field_key")
        )
        .def(
            "execute",
            [](NWPEmulatorCore& core, const NWPEmulatorRunOptions& options) {
                ensure_runtime_initialised();
                return core.execute(options);
            },
            py::arg("options"));

    module.def(
        "execute",
        [](const NWPEmulatorRunOptions& options) {
            ensure_runtime_initialised();
            NWPEmulatorCore core;
            return core.execute(options);
        },
        py::arg("options"),
        "Execute a single emulator run using normalized run options.");
}

}  // namespace nwp_emulator