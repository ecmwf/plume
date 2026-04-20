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

    def dispatch(self, method, path, _payload):
        return {"method": method, "path": path, "ok": True}

    def get_state(self):
        return self._state


class _StubRuntime:
    def __init__(self):
        self.touched = 0
        self.launch_calls = []
        self.cleanup_calls = 0
        self.stop_calls = 0

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

    def stop_or_teardown(self):
        self.stop_calls += 1
        return 2


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
        raise ImportError("Failed to import binding module: nwp_emulator_core_dp")

    def build_launch_exception_payload(self, exc, dev_enabled):
        return ui_launcher.LauncherConfig.build_launch_exception_payload(self, exc, dev_enabled)


class _SetupInvalidConfig(_RaisingConfig):
    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        raise ui_launcher.SetupValidationError("setup not ready")


class _StepConfig(_RaisingConfig):
    STEP_LIMIT = 3

    def __init__(self, html_path, base, setup_state):
        super().__init__(html_path, base, setup_state)
        self._step = 1
        self.advance_calls = 0
        self._map_dir = base / "map_fields"
        self._map_dir.mkdir(parents=True, exist_ok=True)
        for step in range(1, self.STEP_LIMIT + 1):
            (self._map_dir / f"step_{step}.json").write_text(
                json.dumps({
                    "step": step,
                    "field_keys": ["t,sfc,0"],
                    "fields": {
                        "t,sfc,0": {
                            "lon": [0.0, 10.0],
                            "lat": [0.0, 5.0],
                            "values": [step * 1.0, step * 2.0],
                        }
                    },
                }),
                encoding="utf-8",
            )
            (self.ui_dir / f"rank_0_step_{step}_fields.json").write_text(
                json.dumps({
                    "rank": 0,
                    "step": step,
                    "fields": {
                        "t,sfc,0": {
                            "lon": [0.0, 10.0],
                            "lat": [0.0, 5.0],
                            "values": [step * 1.0, step * 2.0],
                            "step": step,
                            "rank": 0,
                            "nprocs": 2,
                        }
                    },
                }),
                encoding="utf-8",
            )

    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        return {
            "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
            "dev": bool(dev_enabled),
            "return_code": 0,
            "status": "step-complete",
            "stdout_tail": "",
            "stderr_tail": "",
            "run_id": "run-step",
            "run_dir": "/tmp/session/run-step",
            "run_log": "/tmp/session/run-step/run.log",
            "execution_mode": "step",
            "current_step": 1,
            "total_steps": self.STEP_LIMIT,
            "has_next": True,
            "step_finished": False,
            "field_keys": ["t,sfc,0"],
            "field_snapshot": str(self._map_dir / "step_1.json"),
        }

    def advance_step_from_session(self):
        self.advance_calls += 1
        # At terminal: return complete without incrementing step counter.
        if self._step >= self.STEP_LIMIT:
            return {
                "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
                "dev": False,
                "return_code": 0,
                "status": "complete",
                "stdout_tail": "",
                "stderr_tail": "",
                "run_id": "run-step",
                "run_dir": "/tmp/session/run-step",
                "run_log": "/tmp/session/run-step/run.log",
                "execution_mode": "step",
                "current_step": self._step,
                "total_steps": self.STEP_LIMIT,
                "has_next": False,
                "step_finished": True,
                "field_keys": ["t,sfc,0"],
                "field_snapshot": str(self._map_dir / f"step_{self._step}.json"),
            }
        self._step += 1
        has_next = self._step < self.STEP_LIMIT
        return {
            "command": ["mpirun", "-np", "1", "/tmp/nwp_emulator_run_dp.x"],
            "dev": False,
            "return_code": 0,
            "status": "step-complete" if has_next else "complete",
            "stdout_tail": "",
            "stderr_tail": "",
            "run_id": "run-step",
            "run_dir": "/tmp/session/run-step",
            "run_log": "/tmp/session/run-step/run.log",
            "execution_mode": "step",
            "current_step": self._step,
            "total_steps": self.STEP_LIMIT,
            "has_next": has_next,
            "step_finished": not has_next,
            "field_keys": ["t,sfc,0"],
            "field_snapshot": str(self._map_dir / f"step_{self._step}.json"),
        }

    def get_step_map_snapshot(self, step):
        step_file = self._map_dir / f"step_{int(step)}.json"
        return step_file if step_file.exists() else None

    def get_step_rank_field_snapshot(self, step, rank):
        rank_file = self.ui_dir / f"rank_{int(rank)}_step_{int(step)}_fields.json"
        return rank_file if rank_file.exists() else None


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
        self.config.stop_or_teardown = self.runtime.stop_or_teardown

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
        expected_state = dict(self.setup_state)
        expected_state["execution_mode"] = "full"
        self.assertEqual(self.runtime.launch_calls[0]["state"], expected_state)
        self.assertEqual(self.runtime.launch_calls[0]["dev_enabled"], False)

    def test_run_checks_endpoint_stops_active_session_before_dispatch(self):
        status, payload = self._post_json("/api/setup/checks/run", {})
        self.assertEqual(status, 200)
        self.assertEqual(payload["ok"], True)
        self.assertEqual(payload["path"], "/api/setup/checks/run")
        self.assertEqual(self.runtime.stop_calls, 1)

    def test_launch_step_mode_returns_first_step_payload(self):
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            body = json.dumps({"dev": False, "execution_mode": "step"}).encode("utf-8")
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

        self.assertEqual(payload["execution_mode"], "step")
        self.assertEqual(payload["current_step"], 1)
        self.assertEqual(payload["has_next"], True)
        self.assertIn("field_snapshot", payload)

    def test_step_next_endpoint_advances_step_session(self):
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            launch_body = json.dumps({"dev": False, "execution_mode": "step"}).encode("utf-8")
            launch_req = urllib.request.Request(
                url=base_url + "/launch",
                data=launch_body,
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(launch_req, timeout=5) as response:
                self.assertEqual(response.status, 200)

            next_req = urllib.request.Request(
                url=base_url + "/api/session/step/next",
                data=b"{}",
                method="POST",
                headers={"Content-Type": "application/json"},
            )
            with urllib.request.urlopen(next_req, timeout=5) as response:
                next_payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(next_payload["execution_mode"], "step")
        self.assertEqual(next_payload["current_step"], 2)
        self.assertEqual(next_payload["has_next"], True)
        self.assertIn("field_snapshot", next_payload)

    def test_map_step_endpoint_returns_snapshot_json(self):
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            req = urllib.request.Request(url=base_url + "/api/run/map/step/1", method="GET")
            with urllib.request.urlopen(req, timeout=5) as response:
                payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(payload["step"], 1)
        self.assertIn("t,sfc,0", payload["fields"])

    def test_map_rank_step_endpoint_returns_rank_snapshot_json(self):
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        try:
            req = urllib.request.Request(url=base_url + "/api/run/map/step/1/rank/0", method="GET")
            with urllib.request.urlopen(req, timeout=5) as response:
                payload = json.loads(response.read().decode("utf-8"))
                self.assertEqual(response.status, 200)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(payload["rank"], 0)
        self.assertEqual(payload["step"], 1)
        self.assertIn("t,sfc,0", payload["fields"])

    def test_map_rank_step_endpoint_missing_rank_returns_404(self):
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"

        status = None
        try:
            req = urllib.request.Request(url=base_url + "/api/run/map/step/1/rank/99", method="GET")
            try:
                with urllib.request.urlopen(req, timeout=5) as response:
                    status = response.status
            except urllib.error.HTTPError as exc:
                status = exc.code
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(status, 404)

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
        self.assertIn("ImportError", payload["stderr_tail"])
        self.assertIn("Failed to import binding module", payload["stderr_tail"])

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
        status, _payload = self._post_json("/api/session/heartbeat", {})
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

    def _step_server(self):
        """Spin up a _StepConfig server; return (server, thread, base_url, config)."""
        config = _StepConfig(self.html_path, self.base, self.setup_state)
        handler = ui_launcher.make_handler(config)
        server = ui_launcher.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}"
        return server, thread, base_url, config

    def _step_launch(self, base_url):
        body = json.dumps({"dev": False, "execution_mode": "step"}).encode("utf-8")
        req = urllib.request.Request(
            url=base_url + "/launch",
            data=body,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def _step_next(self, base_url):
        req = urllib.request.Request(
            url=base_url + "/api/session/step/next",
            data=b"{}",
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def test_step_mode_play_full_session_reaches_complete(self):
        """Play in step mode then advance through all steps reaches complete."""
        server, thread, base_url, config = self._step_server()
        try:
            launch_payload = self._step_launch(base_url)
            step2 = self._step_next(base_url)
            step3 = self._step_next(base_url)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        self.assertEqual(launch_payload["current_step"], 1)
        self.assertEqual(launch_payload["has_next"], True)
        self.assertEqual(step2["current_step"], 2)
        self.assertEqual(step2["has_next"], True)
        self.assertEqual(step3["current_step"], 3)
        self.assertEqual(step3["has_next"], False)
        self.assertEqual(step3["status"], "complete")
        self.assertEqual(step3["step_finished"], True)

    def test_step_mode_forward_at_terminal_is_idempotent(self):
        """Calling forward at the last step returns complete without running more steps."""
        server, thread, base_url, config = self._step_server()
        try:
            self._step_launch(base_url)      # step 1
            self._step_next(base_url)         # step 2
            self._step_next(base_url)         # step 3 (terminal)
            advance_calls_after_completion = config.advance_calls
            terminal_payload = self._step_next(base_url)   # extra forward at terminal
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

        # The extra forward at terminal should NOT have incremented the step counter.
        self.assertEqual(terminal_payload["has_next"], False)
        self.assertEqual(terminal_payload["status"], "complete")
        self.assertEqual(terminal_payload["current_step"], _StepConfig.STEP_LIMIT)
        # Advance was called for step-to-terminal and then once more (idempotent call).
        self.assertEqual(config.advance_calls, advance_calls_after_completion + 1)


if __name__ == "__main__":
    unittest.main()

