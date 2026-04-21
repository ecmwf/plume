import json
import pathlib
import sys
import tempfile
import threading
import unittest
import urllib.request
from importlib import import_module


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
ui_launcher = import_module("ui_launcher")


class _StubSetupRoutes:
    def __init__(self, state):
        self._state = state

    def dispatch(self, method, path, payload):
        return {"method": method, "path": path, "ok": True}

    def get_state(self):
        return self._state


class _StubRuntime:
    def __init__(self):
        self.touched = 0
        self.launch_calls = []
        self.cleanup_calls = 0

    def touch_session(self):
        self.touched += 1

    def run_from_setup_state(self, setup_state, cwd, dev_enabled):
        self.launch_calls.append({"state": setup_state, "cwd": cwd, "dev_enabled": dev_enabled})
        return {
            "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
            "dev": bool(dev_enabled),
            "return_code": 0,
            "stdout_tail": "ok",
            "stderr_tail": "",
            "run_id": "run-test",
            "run_dir": "/tmp/session/run-test",
            "run_log": "/tmp/session/run-test/run.log",
        }

    def cleanup_session_runs(self):
        self.cleanup_calls += 1
        return 2


class _EngineRuntimeStub:
    def __init__(self):
        self.python_calls = 0
        self.subprocess_calls = 0
        self.running_calls = 0
        self.recorded = None

    def set_running(self):
        self.running_calls += 1

    def record_result(self, payload):
        self.recorded = dict(payload)

    def run_from_setup_state_python(self, setup_state, cwd, dev_enabled):
        self.python_calls += 1
        raise NotImplementedError("python engine unavailable")

    def run_from_setup_state(self, setup_state, cwd, dev_enabled):
        self.subprocess_calls += 1
        return {
            "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
            "dev": bool(dev_enabled),
            "return_code": 0,
            "stdout_tail": "ok",
            "stderr_tail": "",
            "run_id": "run-fallback",
            "run_dir": "/tmp/session/run-fallback",
            "run_log": "/tmp/session/run-fallback/run.log",
        }


class _RaisingConfig:
    def __init__(self, html_path, base, setup_state):
        self.html_path = html_path
        self.ui_dir = base
        self.setup_routes = _StubSetupRoutes(setup_state)

    def touch_session(self):
        return None

    def cleanup_session_runs(self):
        return 0

    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        raise NotImplementedError("Python engine supports single-rank only")

    def build_launch_exception_payload(self, exc, dev_enabled):
        return ui_launcher.LauncherConfig.build_launch_exception_payload(self, exc, dev_enabled)


class _SetupInvalidConfig(_RaisingConfig):
    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        raise ui_launcher.SetupValidationError("setup not ready")


class _StatefulConfig(_RaisingConfig):
    def __init__(self, html_path, base, setup_state, return_code=0):
        super().__init__(html_path, base, setup_state)
        self._return_code = return_code
        self._status = "idle"
        self._last_result = None

    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        status = "complete" if self._return_code == 0 else "failed"
        payload = {
            "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
            "dev": bool(dev_enabled),
            "return_code": self._return_code,
            "status": status,
            "stdout_tail": "ok" if self._return_code == 0 else "",
            "stderr_tail": "" if self._return_code == 0 else "error: step 2 aborted",
            "run_id": "run-stateful",
            "run_dir": "/tmp/session/run-stateful",
            "run_log": "/tmp/session/run-stateful/run.log",
        }
        self._record_result(payload)
        return payload

    def _record_result(self, payload):
        self._last_result = dict(payload)
        self._status = payload.get("status", "failed")

    def status(self):
        return self._status

    def get_last_result(self):
        return self._last_result


class UiLauncherEndpointsTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory(prefix="launcher_http_test_")
        self.base = pathlib.Path(self.temp_dir.name)
        self.html_path = self.base / "index.html"
        self.html_path.write_text("<html></html>", encoding="utf-8")
        self.setup_state = {
            "options": {"run_mode": "config", "dry_run": True, "mpi_np": 1},
            "emulator_config": {"text": "emulator: {}", "path_display": ""},
            "plume_config": {"text": "", "path_display": ""},
            "grib_source": {"selected_paths": [], "path_display": ""},
        }

        self.runtime = _StubRuntime()
        self.config = type("Config", (), {})()
        self.config.html_path = self.html_path
        self.config.ui_dir = self.base
        self.config.setup_routes = _StubSetupRoutes(self.setup_state)
        self.config.runtime = self.runtime
        self.config.touch_session = self.runtime.touch_session
        self.config.cleanup_session_runs = self.runtime.cleanup_session_runs

        handler = ui_launcher.make_handler(self.config)
        self.server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.temp_dir.cleanup()

    def _post_json(self, path, payload):
        body = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            url=self.base_url + path,
            data=body,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=5) as response:
            data = response.read().decode("utf-8")
            return response.status, json.loads(data)

    def test_heartbeat_endpoint(self):
        status, payload = self._post_json("/api/session/heartbeat", {})
        self.assertEqual(status, 200)
        self.assertEqual(payload["ok"], True)
        self.assertGreaterEqual(self.runtime.touched, 1)

    def test_close_endpoint_returns_removed_count(self):
        status, payload = self._post_json("/api/session/close", {})
        self.assertEqual(status, 200)
        self.assertEqual(payload["ok"], True)
        self.assertEqual(payload["removed_run_dirs"], 2)
        self.assertEqual(self.runtime.cleanup_calls, 1)

    def test_launch_endpoint_delegates_to_runtime(self):
        status, payload = self._post_json("/launch", {"dev": False})
        self.assertEqual(status, 200)
        self.assertEqual(payload["return_code"], 0)
        self.assertEqual(payload["status"], "complete")
        self.assertEqual(payload["run_id"], "run-test")
        self.assertEqual(len(self.runtime.launch_calls), 1)
        self.assertEqual(self.runtime.launch_calls[0]["state"], self.setup_state)
        self.assertEqual(self.runtime.launch_calls[0]["dev_enabled"], False)

    def test_launch_endpoint_returns_traceback_in_stderr_tail(self):
        config = _RaisingConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            body = json.dumps({"dev": False}).encode("utf-8")
            req = urllib.request.Request(
                url=base_url + "/launch",
                data=body,
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=5) as response:
                payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(payload["return_code"], 1)
        self.assertEqual(payload["status"], "failed")
        self.assertEqual(payload["stdout_tail"], "")
        self.assertIn("NotImplementedError", payload["stderr_tail"])
        self.assertIn("Python engine supports single-rank only", payload["stderr_tail"])

    def test_launch_endpoint_setup_invalid_returns_not_ready_status(self):
        config = _SetupInvalidConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            body = json.dumps({"dev": False}).encode("utf-8")
            req = urllib.request.Request(
                url=base_url + "/launch",
                data=body,
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with self.assertRaises(urllib.error.HTTPError) as ctx:
                urllib.request.urlopen(req, timeout=5)
            self.assertEqual(ctx.exception.code, 400)
            payload = json.loads(ctx.exception.read().decode("utf-8"))
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(payload["status"], "not-ready")
        self.assertIn("setup not ready", payload["error"])

    def test_status_endpoint_returns_idle_before_launch(self):
        status, payload = self._post_json("/api/session/heartbeat", {})
        self.assertEqual(status, 200)

        with urllib.request.urlopen(self.base_url + "/api/session/status", timeout=5) as response:
            state_payload = json.loads(response.read().decode("utf-8"))
            self.assertEqual(response.status, 200)

        self.assertEqual(state_payload["ok"], True)
        self.assertEqual(state_payload["status"], "idle")
        self.assertIsNone(state_payload["last_result"])

    def test_status_and_last_result_persist_after_launch(self):
        config = _StatefulConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            body = json.dumps({"dev": False}).encode("utf-8")
            req = urllib.request.Request(
                url=base_url + "/launch",
                data=body,
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(req, timeout=5) as response:
                launch_payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)

            with urllib.request.urlopen(base_url + "/api/session/status", timeout=5) as response:
                status_payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)

            with urllib.request.urlopen(base_url + "/api/session/last-result", timeout=5) as response:
                last_payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(launch_payload["run_id"], "run-stateful")
        self.assertEqual(status_payload["status"], "complete")
        self.assertEqual(status_payload["last_result"]["run_id"], "run-stateful")
        self.assertEqual(last_payload["last_result"]["run_id"], "run-stateful")

        def test_failed_launch_status_persists(self):
            config = _StatefulConfig(self.html_path, self.base, self.setup_state, return_code=1)
            handler = ui_launcher.make_handler(config)
            server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            base_url = f"http://127.0.0.1:{server.server_port}"

            try:
                body = json.dumps({"dev": False}).encode("utf-8")
                req = urllib.request.Request(
                    url=base_url + "/launch",
                    data=body,
                    method="POST",
                    headers={"Content-Type": "application/json"},
                )
                with urllib.request.urlopen(req, timeout=5) as response:
                    launch_payload = json.loads(response.read().decode("utf-8"))
                    self.assertEqual(response.status, 200)

                with urllib.request.urlopen(base_url + "/api/session/status", timeout=5) as response:
                    status_payload = json.loads(response.read().decode("utf-8"))
                    self.assertEqual(response.status, 200)
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

            self.assertEqual(launch_payload["return_code"], 1)
            self.assertEqual(launch_payload["status"], "failed")
            self.assertEqual(status_payload["status"], "failed")
            self.assertEqual(status_payload["last_result"]["return_code"], 1)


class LauncherConfigEngineRoutingTest(unittest.TestCase):
    def test_python_engine_falls_back_to_subprocess_when_enabled(self):
        cfg = ui_launcher.LauncherConfig(
            emulator_path="/tmp/nwp_emulator_run_dp.x",
            html_path=pathlib.Path("/tmp/index.html"),
            setup_routes=_StubSetupRoutes({}),
            engine="python",
            fallback_subprocess=True,
        )
        cfg.runtime = _EngineRuntimeStub()
        payload = cfg.launch_from_setup_state({"options": {"mpi_np": 1}}, cwd="/tmp", dev_enabled=False)

        self.assertEqual(payload["engine"], "subprocess")
        self.assertEqual(payload["engine_requested"], "python")
        self.assertEqual(payload["engine_fallback"], True)
        self.assertEqual(payload["status"], "complete")
        self.assertEqual(cfg.runtime.python_calls, 1)
        self.assertEqual(cfg.runtime.subprocess_calls, 1)
        self.assertEqual(cfg.runtime.running_calls, 1)
        self.assertEqual(cfg.runtime.recorded["run_id"], "run-fallback")

    def test_python_engine_raises_when_fallback_disabled(self):
        cfg = ui_launcher.LauncherConfig(
            emulator_path="/tmp/nwp_emulator_run_dp.x",
            html_path=pathlib.Path("/tmp/index.html"),
            setup_routes=_StubSetupRoutes({}),
            engine="python",
            fallback_subprocess=False,
        )
        cfg.runtime = _EngineRuntimeStub()
        with self.assertRaises(NotImplementedError):
            cfg.launch_from_setup_state({"options": {"mpi_np": 1}}, cwd="/tmp", dev_enabled=False)

        def test_subprocess_engine_runs_subprocess_directly(self):
            """engine=subprocess routes to subprocess with no python attempt."""
            cfg = ui_launcher.LauncherConfig(
                emulator_path="/tmp/nwp_emulator_run_dp.x",
                html_path=pathlib.Path("/tmp/index.html"),
                setup_routes=_StubSetupRoutes({}),
                engine="subprocess",
                fallback_subprocess=False,
            )
            cfg.runtime = _EngineRuntimeStub()
            payload = cfg.launch_from_setup_state({"options": {"mpi_np": 1}}, cwd="/tmp", dev_enabled=False)

            self.assertEqual(payload["engine"], "subprocess")
            self.assertEqual(payload["engine_requested"], "subprocess")
            self.assertEqual(payload["engine_fallback"], False)
            self.assertEqual(payload["status"], "complete")
            self.assertEqual(cfg.runtime.python_calls, 0)
            self.assertEqual(cfg.runtime.subprocess_calls, 1)
            self.assertEqual(cfg.runtime.running_calls, 1)
            self.assertEqual(cfg.runtime.recorded["run_id"], "run-fallback")


if __name__ == "__main__":
    unittest.main()
