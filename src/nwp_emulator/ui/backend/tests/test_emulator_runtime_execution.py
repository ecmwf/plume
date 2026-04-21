import pathlib
import sys
import tempfile
import unittest
from importlib import import_module
from unittest.mock import patch


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
runtime_module = import_module("backend.services.emulator_runtime")


class _CompletedProcess:
    def __init__(self, returncode=0, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


class EmulatorRuntimeExecutionTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="runtime_exec_test_")
        self.base = pathlib.Path(self.temp_dir.name)
        self.emulator_path = self.base / "nwp_emulator_run_dp.x"
        self.emulator_path.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_run_from_setup_state_writes_log_and_returns_payload(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        state = {
            "options": {"run_mode": "config", "dry_run": True, "mpi_np": 2},
            "emulator_config": {"text": "emulator:\n  n_steps: 3\n", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        with patch.object(runtime_module.subprocess, "run") as run_mock:
            run_mock.return_value = _CompletedProcess(returncode=5, stdout="out-data", stderr="err-data")
            result = runtime.run_from_setup_state(state, cwd=self.base, dev_enabled=False)

        self.assertEqual(result["return_code"], 5)
        self.assertEqual(result["dev"], False)
        self.assertIn("--config-src", result["command"])
        self.assertEqual(result["stdout_tail"], "out-data")
        self.assertEqual(result["stderr_tail"], "err-data")

        run_log = pathlib.Path(result["run_log"])
        self.assertTrue(run_log.exists())
        run_log_text = run_log.read_text(encoding="utf-8")
        self.assertIn("Return code: 5", run_log_text)
        self.assertIn("--- stdout ---", run_log_text)
        self.assertIn("out-data", run_log_text)
        self.assertIn("--- stderr ---", run_log_text)
        self.assertIn("err-data", run_log_text)

        runtime.cleanup_all()

    def test_run_from_setup_state_includes_plume_arg_when_not_dry_run(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        state = {
            "options": {"run_mode": "config", "dry_run": False, "mpi_np": 1},
            "emulator_config": {"text": "emulator:\n  n_steps: 1\n", "path_display": ""},
            "plume_config": {"text": "plugins: []\n", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        with patch.object(runtime_module.subprocess, "run") as run_mock:
            run_mock.return_value = _CompletedProcess(returncode=0, stdout="", stderr="")
            result = runtime.run_from_setup_state(state, cwd=self.base, dev_enabled=True)

        self.assertIn("--plume-cfg", result["command"])
        self.assertEqual(result["dev"], True)
        runtime.cleanup_all()


class _FakeRunResult:
    """Minimal stand-in for the pybind RunResult object."""
    def __init__(self, return_code=0, plume_run=False, last_step_run=-1):
        self.return_code = return_code
        self.plume_run = plume_run
        self.last_step_run = last_step_run


class _FakeDataSourceType:
    GRIB = "GRIB"
    CONFIG = "CONFIG"
    INVALID = "INVALID"


class _FakeRunOptions:
    def __init__(self):
        self.data_source_type = _FakeDataSourceType.INVALID
        self.data_source_path = ""
        self.plume_config_path = ""


class _FakeModule:
    """Minimal stand-in for the pybind extension module."""
    DataSourceType = _FakeDataSourceType
    RunOptions = _FakeRunOptions

    @staticmethod
    def execute(opts):
        return _FakeRunResult(return_code=0, plume_run=bool(opts.plume_config_path), last_step_run=3)


class EmulatorRuntimePythonEngineTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="runtime_py_test_")
        self.base = pathlib.Path(self.temp_dir.name)
        self.emulator_path = self.base / "nwp_emulator_run_dp.x"
        self.emulator_path.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")

    def tearDown(self):
        self.temp_dir.cleanup()

    def _make_runtime(self):
        return runtime_module.EmulatorRuntime(self.emulator_path)

    def _dry_run_state(self):
        return {
            "options": {"run_mode": "config", "dry_run": True, "mpi_np": 1},
            "emulator_config": {"text": "emulator:\n  n_steps: 1\n", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

    def test_python_engine_calls_module_execute_and_returns_payload(self):
        """run_from_setup_state_python delegates to mod.execute and shapes response."""
        import sys as _sys
        runtime = self._make_runtime()
        _sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            result = runtime.run_from_setup_state_python(
                self._dry_run_state(), cwd=self.base, dev_enabled=True
            )
        finally:
            _sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(result["return_code"], 0)
        self.assertIn("run_id", result)
        self.assertIn("run_log", result)
        self.assertEqual(result["dev"], True)
        self.assertFalse(result["plume_run"])  # dry_run -> no plume config
        runtime.cleanup_all()

    def test_python_engine_raises_not_implemented_for_mpi(self):
        """mpi_np > 1 must raise NotImplementedError so callers can fall back."""
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["options"]["mpi_np"] = 4
        with self.assertRaises(NotImplementedError):
            runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
        runtime.cleanup_all()

    def test_run_options_from_command_parses_config_src(self):
        """_run_options_from_command maps --config-src to CONFIG source type."""
        command = ["mpirun", "-np", "1", "binary", "--config-src", "/tmp/cfg.yaml"]
        opts = runtime_module.EmulatorRuntime._run_options_from_command(_FakeModule, command)
        self.assertEqual(opts.data_source_type, _FakeDataSourceType.CONFIG)
        self.assertEqual(opts.data_source_path, "/tmp/cfg.yaml")
        self.assertEqual(opts.plume_config_path, "")

    def test_run_options_from_command_parses_grib_src_and_plume(self):
        """_run_options_from_command maps --grib-src and --plume-cfg correctly."""
        command = [
            "mpirun", "-np", "1", "binary",
            "--grib-src", "/tmp/grib",
            "--plume-cfg", "/tmp/plume.yaml",
        ]
        opts = runtime_module.EmulatorRuntime._run_options_from_command(_FakeModule, command)
        self.assertEqual(opts.data_source_type, _FakeDataSourceType.GRIB)
        self.assertEqual(opts.data_source_path, "/tmp/grib")
        self.assertEqual(opts.plume_config_path, "/tmp/plume.yaml")


if __name__ == "__main__":
    unittest.main()
