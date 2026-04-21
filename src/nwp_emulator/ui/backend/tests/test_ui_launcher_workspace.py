import pathlib
import sys
import tempfile
import time
import unittest
from importlib import import_module


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
runtime_module = import_module("backend.services.emulator_runtime")


class UiLauncherWorkspaceTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="ui_launcher_test_")
        self.base = pathlib.Path(self.temp_dir.name)
        self.emulator_path = self.base / "nwp_emulator_run_dp.x"
        self.emulator_path.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_create_run_workspace_config_mode_stages_yaml_files(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        state = {
            "options": {"run_mode": "config", "dry_run": False, "mpi_np": 4},
            "emulator_config": {"text": "emulator:\n  n_steps: 2\n", "path_display": ""},
            "plume_config": {"text": "plugins: []\n", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        run_workspace = runtime.create_run_workspace(state)

        run_dir = run_workspace["run_dir"]
        self.assertTrue((run_dir / "emulator_config.yaml").exists())
        self.assertTrue((run_dir / "plume_config.yaml").exists())
        self.assertTrue((run_dir / "run_manifest.json").exists())
        self.assertIn("--config-src", run_workspace["command"])
        self.assertIn("--plume-cfg", run_workspace["command"])
        self.assertEqual(run_workspace["command"][0:3], ["mpirun", "-np", "4"])

        runtime.cleanup_all()

    def test_create_run_workspace_grib_mode_copies_selected_files(self):
        grib_source_file = self.base / "input_001.grib"
        grib_source_file.write_bytes(b"grib")

        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        state = {
            "options": {"run_mode": "grib", "dry_run": True, "mpi_np": 2},
            "emulator_config": {"text": "", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [str(grib_source_file)], "path_display": ""},
        }

        run_workspace = runtime.create_run_workspace(state)

        run_grib_dir = run_workspace["run_dir"] / "grib"
        self.assertTrue((run_grib_dir / "input_001.grib").exists())
        self.assertIn("--grib-src", run_workspace["command"])
        self.assertNotIn("--plume-cfg", run_workspace["command"])

        runtime.cleanup_all()

    def test_cleanup_session_runs_removes_run_directories(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)

        state = {
            "options": {"run_mode": "config", "dry_run": True, "mpi_np": 1},
            "emulator_config": {"text": "emulator:\n  n_steps: 1\n", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        run_workspace = runtime.create_run_workspace(state)
        self.assertTrue(run_workspace["run_dir"].exists())

        removed = runtime.cleanup_session_runs()
        self.assertEqual(removed, 1)
        self.assertFalse(run_workspace["run_dir"].exists())

        runtime.cleanup_all()

    def test_reaper_removes_stale_session_run_dirs(self):
        runtime = runtime_module.EmulatorRuntime(
            self.emulator_path,
            session_stale_seconds=0.01,
            reaper_interval_seconds=0.01,
        )

        state = {
            "options": {"run_mode": "config", "dry_run": True, "mpi_np": 1},
            "emulator_config": {"text": "emulator:\n  n_steps: 1\n", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        run_workspace = runtime.create_run_workspace(state)
        self.assertTrue(run_workspace["run_dir"].exists())

        runtime.last_seen = time.monotonic() - 10
        runtime.start_reaper()
        time.sleep(0.1)
        runtime.stop_reaper()

        self.assertFalse(run_workspace["run_dir"].exists())
        runtime.cleanup_all()

    def test_runtime_status_and_last_result_apis(self):
        runtime = runtime_module.EmulatorRuntime(self.emulator_path)
        self.assertEqual(runtime.status(), "idle")
        self.assertIsNone(runtime.get_last_result())

        payload = {"return_code": 0, "status": "complete", "run_id": "run-1"}
        runtime.record_result(payload)
        self.assertEqual(runtime.status(), "complete")
        self.assertEqual(runtime.get_last_result()["run_id"], "run-1")

        removed = runtime.stop_or_teardown()
        self.assertGreaterEqual(removed, 0)
        self.assertEqual(runtime.status(), "idle")
        runtime.cleanup_all()


if __name__ == "__main__":
    unittest.main()
