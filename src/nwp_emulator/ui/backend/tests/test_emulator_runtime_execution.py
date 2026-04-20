import pathlib
import sys
import tempfile
import unittest
from importlib import import_module
from unittest.mock import patch
import json
import threading
import time


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

    def test_aggregate_multirank_results_includes_rank_summaries_when_output_empty(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        run_dir = runtime.session_root / "aggregate-test"
        run_dir.mkdir(parents=True, exist_ok=True)
        for rank in (0, 1):
            (run_dir / f"rank_{rank}.json").write_text(
                json.dumps(
                    {
                        "rank": rank,
                        "return_code": 0,
                        "status": "complete",
                        "stdout": "",
                        "stderr": "",
                        "plume_run": False,
                        "last_step_run": 1,
                    }
                ),
                encoding="utf-8",
            )

        aggregated = runtime._aggregate_multirank_results(run_dir, 2, "", "")
        self.assertIn("Rank 0: return_code=0, status=complete", aggregated["stdout"])
        self.assertIn("Rank 1: return_code=0, status=complete", aggregated["stdout"])

        run_log_text = (run_dir / "run.log").read_text(encoding="utf-8")
        self.assertIn("Per-rank results", run_log_text)
        self.assertIn("Rank 0: return_code=0, status=complete", run_log_text)
        runtime.cleanup_all()

    def test_aggregate_multirank_results_ignores_step_and_field_json_files(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        run_dir = runtime.session_root / "aggregate-ignore-step-files"
        run_dir.mkdir(parents=True, exist_ok=True)

        (run_dir / "rank_0.json").write_text(
            json.dumps(
                {
                    "rank": 0,
                    "return_code": 0,
                    "status": "complete",
                    "stdout": "",
                    "stderr": "",
                    "plume_run": False,
                    "last_step_run": 2,
                }
            ),
            encoding="utf-8",
        )
        (run_dir / "rank_1.json").write_text(
            json.dumps(
                {
                    "rank": 1,
                    "return_code": 0,
                    "status": "complete",
                    "stdout": "",
                    "stderr": "",
                    "plume_run": False,
                    "last_step_run": 2,
                }
            ),
            encoding="utf-8",
        )

        # These files must be ignored by rank-result aggregation.
        (run_dir / "rank_0_step_1_done.json").write_text(
            json.dumps({"rank": 0, "step": 1, "status": "ok", "has_step": True}),
            encoding="utf-8",
        )
        (run_dir / "rank_1_step_1_done.json").write_text(
            json.dumps({"rank": 1, "step": 1, "status": "ok", "has_step": True}),
            encoding="utf-8",
        )
        (run_dir / "rank_0_step_1_fields.json").write_text(
            json.dumps({"rank": 0, "step": 1, "fields": {"t,sfc,0": {}}}),
            encoding="utf-8",
        )
        (run_dir / "rank_1_step_1_fields.json").write_text(
            json.dumps({"rank": 1, "step": 1, "fields": {"t,sfc,0": {}}}),
            encoding="utf-8",
        )

        aggregated = runtime._aggregate_multirank_results(run_dir, 2, "", "")

        self.assertEqual(aggregated["return_code"], 0)
        self.assertIn("Rank 0: return_code=0, status=complete", aggregated["stdout"])
        self.assertIn("Rank 1: return_code=0, status=complete", aggregated["stdout"])
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


class _FakeStepCore:
    def __init__(self):
        self._step = 0
        self._limit = 3
        self._plume_enabled = False
        self._finalized = False

    def validate_run_options(self, options):
        return bool(options.data_source_path)

    def setup_data_provider(self, _options):
        return True

    def setup_plume_provider(self):
        self._plume_enabled = True
        return True

    def run_step(self):
        if self._step >= self._limit:
            return False
        self._step += 1
        return True

    def finalize_plume(self):
        self._finalized = True

    def current_step(self):
        return self._step

    def available_field_keys(self):
        return ["t,sfc,0", "u,ml,1"]

    def get_field_overlay_snapshot(self, field_key):
        values = [1.0 + self._step, 2.0 + self._step, 3.0 + self._step]
        if str(field_key).startswith("u,"):
            values = [value * 2.0 for value in values]
        return {
            "field_key": field_key,
            "lon": [0.0, 10.0, 20.0],
            "lat": [0.0, 5.0, 10.0],
            "values": values,
            "step": self._step,
            "rank": 0,
            "root": 0,
            "nprocs": 1,
        }


_FakeModule.NWPEmulatorCore = _FakeStepCore


class _FakeProcStub:
    """Minimal Popen stand-in that satisfies terminate/wait/kill calls."""

    def terminate(self):
        pass

    def kill(self):
        pass

    def wait(self, timeout=None):
        return 0

    @property
    def returncode(self):
        return None


def _extract_run_dir(cmd: list) -> pathlib.Path:
    """Extract run_dir from a worker mpirun command (argument after emulator_worker.py)."""
    for i, arg in enumerate(cmd):
        if "emulator_worker" in str(arg) and i + 1 < len(cmd):
            return pathlib.Path(cmd[i + 1])
    raise ValueError(f"Could not find run_dir in command: {cmd}")


class _MultiRankWorkerThread(threading.Thread):
    """Simulates N MPI worker processes using the step-mode IPC protocol."""

    POLL_INTERVAL = 0.01  # 10 ms poll so tests finish quickly

    def __init__(self, run_dir: pathlib.Path, mpi_np: int, step_limit: int):
        super().__init__(daemon=True)
        self.run_dir = run_dir
        self.mpi_np = mpi_np
        self.step_limit = step_limit

    def _write(self, path: pathlib.Path, data: dict) -> None:
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data), encoding="utf-8")
        tmp.rename(path)

    def run(self) -> None:
        # Small delay to let the launcher call Popen and start polling.
        time.sleep(0.01)
        # Signal all ranks ready.
        for rank in range(self.mpi_np):
            self._write(self.run_dir / f"rank_{rank}_ready.json", {
                "rank": rank, 
                "status": "ready", 
                "stdout": f"[Setup] Rank {rank}: Loading emulator core...\n[Setup] Rank {rank}: Initialized data provider\n[Setup] Rank {rank}: Emulator setup completed",
                "stderr": "",
            })

        step = 0
        while True:
            next_step = step + 1
            advance_file = self.run_dir / f"step_advance_{next_step}"
            abort_file = self.run_dir / "step_abort"
            finalize_file = self.run_dir / "step_finalize"
            while not advance_file.exists() and not abort_file.exists() and not finalize_file.exists():
                time.sleep(self.POLL_INTERVAL)
            if abort_file.exists():
                break
            if finalize_file.exists():
                for rank in range(self.mpi_np):
                    self._write(self.run_dir / f"rank_{rank}_finalized.json", {
                        "rank": rank,
                        "status": "ok",
                        "return_code": 0,
                        "error": None,
                        "stdout": f"Process {rank} finished...\\nEmulator run completed...",
                        "stderr": "",
                    })
                break
            step = next_step
            has_step = step <= self.step_limit
            time.sleep(0.01)  # tiny delay to simulate work
            for rank in range(self.mpi_np):
                self._write(self.run_dir / f"rank_{rank}_step_{next_step}_done.json", {
                    "rank": rank,
                    "step": step,
                    "has_step": has_step,
                    "return_code": 0,
                    "status": "ok",
                    "stdout": f"rank{rank}-step{step}",
                    "stderr": "",
                    "plume_run": False,
                    "last_step_run": step,
                })
            if not has_step:
                break


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
        runtime = self._make_runtime()
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            result = runtime.run_from_setup_state_python(
                self._dry_run_state(), cwd=self.base, dev_enabled=True
            )
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(result["return_code"], 0)
        self.assertIn("run_id", result)
        self.assertIn("run_log", result)
        self.assertEqual(result["dev"], True)
        self.assertFalse(result["plume_run"])  # dry_run -> no plume config
        runtime.cleanup_all()

    def test_python_engine_step_mode_runs_first_step(self):
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["emulator_config"]["text"] = "emulator:\n  n_steps: 3\n"
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            payload = runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(payload["execution_mode"], "step")
        self.assertEqual(payload["current_step"], 1)
        self.assertEqual(payload["has_next"], True)
        self.assertEqual(payload["status"], "step-complete")
        self.assertIn("field_keys", payload)
        self.assertIn("t,sfc,0", payload["field_keys"])
        self.assertTrue(payload.get("field_snapshot"))
        self.assertTrue(pathlib.Path(payload["field_snapshot"]).exists())
        runtime.cleanup_all()

    def test_python_engine_step_mode_advance_progresses_one_step(self):
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["emulator_config"]["text"] = "emulator:\n  n_steps: 3\n"
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            launch_payload = runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
            next_payload = runtime.advance_step_from_session()
            last_payload = runtime.advance_step_from_session()
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(launch_payload["current_step"], 1)
        self.assertEqual(next_payload["current_step"], 2)
        self.assertEqual(next_payload["has_next"], True)
        self.assertTrue(next_payload.get("field_snapshot"))
        self.assertTrue(pathlib.Path(next_payload["field_snapshot"]).exists())
        self.assertEqual(last_payload["current_step"], 3)
        self.assertEqual(last_payload["has_next"], False)
        self.assertEqual(last_payload["status"], "complete")
        runtime.cleanup_all()

    def test_python_engine_step_mode_multirank_runs_first_step(self):
        """Multi-rank step mode: fake worker thread simulates 2 MPI ranks for step 1."""
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["options"]["mpi_np"] = 2
        state["emulator_config"]["text"] = "emulator:\n  n_steps: 3\n"

        fake_proc = _FakeProcStub()
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            with patch.object(runtime_module.subprocess, "Popen") as popen_mock:
                def _fake_popen(cmd, **kwargs):
                    run_dir = _extract_run_dir(cmd)
                    _MultiRankWorkerThread(run_dir, mpi_np=2, step_limit=3).start()
                    return fake_proc

                popen_mock.side_effect = _fake_popen
                payload = runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(payload["execution_mode"], "step")
        self.assertEqual(payload["current_step"], 1)
        self.assertEqual(payload["has_next"], True)
        self.assertEqual(payload["status"], "step-complete")
        runtime.cleanup_all()

    def test_python_engine_step_mode_multirank_advances_step(self):
        """Multi-rank step mode: advance progresses to step 2 and to final step."""
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["options"]["mpi_np"] = 2
        state["emulator_config"]["text"] = "emulator:\n  n_steps: 3\n"

        fake_proc = _FakeProcStub()
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            with patch.object(runtime_module.subprocess, "Popen") as popen_mock:
                def _fake_popen(cmd, **kwargs):
                    run_dir = _extract_run_dir(cmd)
                    _MultiRankWorkerThread(run_dir, mpi_np=2, step_limit=3).start()
                    return fake_proc

                popen_mock.side_effect = _fake_popen
                launch_payload = runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
                step2_payload = runtime.advance_step_from_session()
                step3_payload = runtime.advance_step_from_session()
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        self.assertEqual(launch_payload["current_step"], 1)
        self.assertEqual(step2_payload["current_step"], 2)
        self.assertEqual(step2_payload["has_next"], True)
        self.assertEqual(step3_payload["current_step"], 3)
        self.assertEqual(step3_payload["has_next"], False)
        self.assertEqual(step3_payload["status"], "complete")
        runtime.cleanup_all()

    def test_python_engine_step_mode_multirank_timeout_writes_run_log(self):
        """When multirank step launch times out, diagnostics are still written to run.log."""
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["options"]["mpi_np"] = 2

        fake_proc = _FakeProcStub()
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            with patch.object(runtime_module.subprocess, "Popen", return_value=fake_proc):
                with patch.object(runtime_module.EmulatorRuntime, "_wait_for_rank_files", return_value=False):
                    with self.assertRaises(RuntimeError):
                        runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        run_log_path = runtime.run_dirs[-1] / "run.log"
        self.assertTrue(run_log_path.exists())
        run_log_text = run_log_path.read_text(encoding="utf-8")
        self.assertIn("Engine: python (step mode, multi-rank)", run_log_text)
        self.assertIn("did not signal readiness", run_log_text)
        runtime.cleanup_all()

    def test_python_engine_step_mode_multirank_logs_setup_output(self):
        """Setup output from workers is logged to run.log in SETUP PHASE block."""
        runtime = self._make_runtime()
        state = self._dry_run_state()
        state["execution_mode"] = "step"
        state["options"]["mpi_np"] = 2
        state["emulator_config"]["text"] = "emulator:\n  n_steps: 1\n"

        fake_proc = _FakeProcStub()
        sys.modules["nwp_emulator_core_dp"] = _FakeModule
        try:
            with patch.object(runtime_module.subprocess, "Popen") as popen_mock:
                def _fake_popen(cmd, **kwargs):
                    run_dir = _extract_run_dir(cmd)
                    _MultiRankWorkerThread(run_dir, mpi_np=2, step_limit=1).start()
                    return fake_proc

                popen_mock.side_effect = _fake_popen
                runtime.run_from_setup_state_python(state, cwd=self.base, dev_enabled=False)
        finally:
            sys.modules.pop("nwp_emulator_core_dp", None)

        # Check that setup output is in the run log
        run_log_path = runtime.run_dirs[-1] / "run.log"
        self.assertTrue(run_log_path.exists())
        run_log_text = run_log_path.read_text(encoding="utf-8")
        self.assertIn("SETUP PHASE", run_log_text)
        self.assertIn("Emulator setup completed", run_log_text)
        self.assertIn("[rank 0]", run_log_text)
        self.assertIn("[rank 1]", run_log_text)
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
