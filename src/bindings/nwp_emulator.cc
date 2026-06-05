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
#include <string>

#include "eckit/runtime/Main.h"

#include "nwp_emulator_core.h"

namespace py = pybind11;

// Module name matches the pybind11_add_module() target name: nwp_emulator_bindings.
#ifndef NWP_EMULATOR_BINDINGS_MODULE_NAME
#define NWP_EMULATOR_BINDINGS_MODULE_NAME nwp_emulator_bindings
#endif

namespace nwp_emulator {

PYBIND11_MODULE(NWP_EMULATOR_BINDINGS_MODULE_NAME, m) {
    m.doc() = "Native pybind11 bindings for the NWP emulator core (thin layer — no business logic).";

    // Must be called once from the Python package __init__ before using any symbol.
    // Kept as an explicit function (rather than per-method guards) to minimise binding complexity.
    // Note: MPI initialisation is handled by eckit::Main::initialise() below.
    // When workers are spawned under mpirun, the MPI environment is already
    // set up by the MPI launcher, and the eckit runtime will use it properly.
    m.def(
        "init_bindings",
        []() {
            if (!eckit::Main::ready()) {
                static const int argc  = 1;
                static char* argv[]    = {const_cast<char*>("nwp_emulator"), nullptr};
                eckit::Main::initialise(argc, argv);
            }
        },
        "Initialise the eckit/Atlas runtime. Called once by pynwp_emulator.__init__.");

    py::enum_<DataSourceType>(m, "DataSourceType")
        .value("GRIB",    DataSourceType::GRIB)
        .value("CONFIG",  DataSourceType::CONFIG)
        .value("INVALID", DataSourceType::INVALID);

    py::class_<NWPEmulatorRunOptions>(m, "RunOptions")
        .def(py::init<>())
        .def_readwrite("data_source_type", &NWPEmulatorRunOptions::dataSourceType)
        .def_readwrite("data_source_path", &NWPEmulatorRunOptions::dataSourcePath)
        .def_readwrite("plume_config_path", &NWPEmulatorRunOptions::plumeConfigPath)
        .def("__eq__", [](const NWPEmulatorRunOptions& a, const NWPEmulatorRunOptions& b) {
            return a.dataSourceType == b.dataSourceType &&
                   a.dataSourcePath == b.dataSourcePath &&
                   a.plumeConfigPath == b.plumeConfigPath;
        }, py::is_operator())
        .def("__repr__", [](const NWPEmulatorRunOptions& o) {
            py::object dst = py::cast(o.dataSourceType);
            return "RunOptions(data_source_type=" + py::str(dst).cast<std::string>() +
                   ", data_source_path='" + o.dataSourcePath +
                   "', plume_config_path='" + o.plumeConfigPath + "')";
        });

    py::class_<NWPEmulatorRunResult>(m, "RunResult")
        .def(py::init<>())
        .def_readonly("return_code",   &NWPEmulatorRunResult::returnCode)
        .def_readonly("plume_run",     &NWPEmulatorRunResult::plumeRun)
        .def_readonly("last_step_run", &NWPEmulatorRunResult::lastStepRun)
        .def("__eq__", [](const NWPEmulatorRunResult& a, const NWPEmulatorRunResult& b) {
            return a.returnCode == b.returnCode &&
                   a.plumeRun == b.plumeRun &&
                   a.lastStepRun == b.lastStepRun;
        }, py::is_operator())
        .def("__repr__", [](const NWPEmulatorRunResult& r) {
            return "RunResult(return_code=" + std::to_string(r.returnCode) +
                   ", plume_run=" + (r.plumeRun ? "True" : "False") +
                   ", last_step_run=" + std::to_string(r.lastStepRun) + ")";
        });

    // Exposed as a proper class instead of a raw dict so the Python layer can
    // document and type-hint individual fields without duplicating binding logic.
    py::class_<NWPFieldOverlaySnapshot>(m, "FieldOverlaySnapshot")
        .def(py::init<>())
        .def_readonly("field_key", &NWPFieldOverlaySnapshot::fieldKey)
        .def_readonly("lon",       &NWPFieldOverlaySnapshot::lon)
        .def_readonly("lat",       &NWPFieldOverlaySnapshot::lat)
        .def_readonly("values",    &NWPFieldOverlaySnapshot::values)
        .def_readonly("step",      &NWPFieldOverlaySnapshot::step)
        .def_readonly("rank",      &NWPFieldOverlaySnapshot::rank)
        .def_readonly("root",      &NWPFieldOverlaySnapshot::root)
        .def_readonly("nprocs",    &NWPFieldOverlaySnapshot::nprocs)
        .def("__repr__", [](const NWPFieldOverlaySnapshot& s) {
            return "FieldOverlaySnapshot(field_key='" + s.fieldKey +
                   "', step=" + std::to_string(s.step) +
                   ", rank=" + std::to_string(s.rank) +
                   ", nprocs=" + std::to_string(s.nprocs) + ")";
        });

    py::class_<NWPEmulatorCore>(m, "NWPEmulatorCore")
        .def(py::init<>())
        // Note: no py::gil_scoped_release guards are added to execute() or run_step().
        // Plume is designed for MPI environments where each rank is a separate process
        // with its own GIL — multithreading within a single rank is not supported.
        // If single-process multithreading is ever added, release the GIL on long-running
        // calls (execute, run_step) via py::call_guard<py::gil_scoped_release>().
        .def("validate_run_options",      &NWPEmulatorCore::validateRunOptions,      py::arg("options"))
        .def("setup_data_provider",       &NWPEmulatorCore::setupDataProvider,       py::arg("options"))
        .def("setup_plume_provider",      &NWPEmulatorCore::setupPlumeProvider)
        .def("run_step",                  &NWPEmulatorCore::runStep)
        .def("finalize_plume",            &NWPEmulatorCore::finalizePlume)
        .def("finalize_run",              &NWPEmulatorCore::finalizeRun)
        .def("current_step",              &NWPEmulatorCore::currentStep)
        .def("available_field_keys",      &NWPEmulatorCore::availableFieldKeys)
        .def("get_field_overlay_snapshot",&NWPEmulatorCore::getFieldOverlaySnapshot, py::arg("field_key"))
        .def("execute",                   &NWPEmulatorCore::execute,                 py::arg("options"));
}

}  // namespace nwp_emulator
