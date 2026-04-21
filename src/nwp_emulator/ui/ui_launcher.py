#!/usr/bin/env python3
"""Minimal local web UI to launch the nwp_emulator command from one button."""

import argparse
import atexit
import mimetypes
import os
import pathlib
import traceback
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from backend.api.request_parsers import parse_json_body
from backend.api.response_builders import error_response, json_response
from backend.api.setup_routes import SetupRoutes
from backend.domain.errors import SetupValidationError
from backend.services.checks_service import ChecksService
from backend.services.emulator_runtime import EmulatorRuntime
from backend.services.setup_service import SetupService
from backend.services.source_service import SourceService
from backend.storage.in_memory_store import InMemorySessionStore

ALLOWED_BIN_NAMES = {"nwp_emulator_run_sp.x", "nwp_emulator_run_dp.x"}
ALLOWED_ENGINES = {"python", "subprocess"}


class LauncherConfig:
    def __init__(self, emulator_path, html_path, setup_routes, engine, fallback_subprocess):
        self.emulator_path = emulator_path
        self.html_path = html_path
        self.ui_dir = html_path.parent
        self.setup_routes = setup_routes
        self.engine = engine
        self.fallback_subprocess = bool(fallback_subprocess)
        self.runtime = EmulatorRuntime(emulator_path, module_search_dirs=[self.ui_dir])

    def configure(self, setup_state):
        # Keep this explicit for ownership API parity during rollout.
        return setup_state

    def touch_session(self):
        self.runtime.touch_session()

    def cleanup_session_runs(self):
        return self.runtime.cleanup_session_runs()

    def cleanup_all(self):
        self.runtime.cleanup_all()

    def start_reaper(self):
        self.runtime.start_reaper()

    def stop_reaper(self):
        self.runtime.stop_reaper()

    def status(self):
        return self.runtime.status()

    def get_last_result(self):
        return self.runtime.get_last_result()

    def stop_or_teardown(self):
        return self.runtime.stop_or_teardown()

    def _record_result(self, payload):
        self.runtime.record_result(payload)

    def build_launch_exception_payload(self, exc, dev_enabled):
        trace = "".join(traceback.format_exception(type(exc), exc, exc.__traceback__))
        return {
            "command": [],
            "dev": bool(dev_enabled),
            "return_code": 1,
            "status": "failed",
            "stdout_tail": "",
            "stderr_tail": trace,
            "run_id": "unknown",
            "run_dir": "unknown",
            "run_log": "unknown",
            "engine": getattr(self, "engine", "unknown"),
            "engine_requested": getattr(self, "engine", "unknown"),
            "engine_fallback": False,
        }

    def launch_from_setup_state(self, setup_state, cwd, dev_enabled):
        self.configure(setup_state)
        self.runtime.set_running()

        if self.engine == "subprocess":
            payload = self.runtime.run_from_setup_state(setup_state, cwd=cwd, dev_enabled=dev_enabled)
            payload["engine"] = "subprocess"
            payload["engine_requested"] = "subprocess"
            payload["engine_fallback"] = False
            payload["status"] = "complete" if int(payload.get("return_code", 1)) == 0 else "failed"
            self._record_result(payload)
            return payload

        if self.engine == "python":
            try:
                payload = self.runtime.run_from_setup_state_python(setup_state, cwd=cwd, dev_enabled=dev_enabled)
                payload["engine"] = "python"
                payload["engine_requested"] = "python"
                payload["engine_fallback"] = False
                payload["status"] = "complete" if int(payload.get("return_code", 1)) == 0 else "failed"
                self._record_result(payload)
                return payload
            except (NotImplementedError, ImportError, RuntimeError) as exc:
                if not self.fallback_subprocess:
                    raise

                payload = self.runtime.run_from_setup_state(setup_state, cwd=cwd, dev_enabled=dev_enabled)
                payload["engine"] = "subprocess"
                payload["engine_requested"] = "python"
                payload["engine_fallback"] = True
                payload["engine_fallback_reason"] = str(exc)
                payload["status"] = "complete" if int(payload.get("return_code", 1)) == 0 else "failed"
                self._record_result(payload)
                return payload

        raise ValueError(f"Unsupported launcher engine: {self.engine}")


def parse_args():
    parser = argparse.ArgumentParser(description="Serve a minimal UI to launch nwp_emulator")
    parser.add_argument("--host", default="127.0.0.1", help="Host address for the local web server")
    parser.add_argument("--port", type=int, default=8080, help="Port for the local web server")
    parser.add_argument("--emulator-bin", required=True, help="Path to nwp_emulator executable")
    parser.add_argument("--mpi-procs", type=int, default=1, help="MPI process count")
    parser.add_argument("--workdir", default=os.getcwd(), help="Working directory for emulator execution")
    parser.add_argument(
        "--engine",
        default="python",
        choices=sorted(ALLOWED_ENGINES),
        help="Execution engine for launch requests",
    )
    parser.add_argument(
        "--fallback-subprocess",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Allow subprocess fallback when --engine=python is unavailable",
    )
    return parser.parse_args()


def validate_and_build_command(args):
    if args.mpi_procs < 1:
        raise ValueError("--mpi-procs must be at least 1")

    emulator_path = pathlib.Path(args.emulator_bin).expanduser().resolve()
    if emulator_path.name not in ALLOWED_BIN_NAMES:
        allowed = ", ".join(sorted(ALLOWED_BIN_NAMES))
        raise ValueError(f"--emulator-bin must point to one of: {allowed}")
    if not emulator_path.exists():
        raise ValueError(f"Emulator binary not found: {emulator_path}")

    return emulator_path


def make_handler(config):
    class LauncherHandler(BaseHTTPRequestHandler):
        def _write_response(self, status, headers, body):
            self.send_response(status)
            for key, value in headers.items():
                self.send_header(key, value)
            self.end_headers()
            self.wfile.write(body)

        def _json_response(self, status, payload):
            status_code, headers, body = json_response(int(status), payload)
            self._write_response(status_code, headers, body)

        def _route_setup(self, method, path, payload):
            try:
                response_payload = config.setup_routes.dispatch(method, path, payload)
                status_code, headers, body = json_response(HTTPStatus.OK, response_payload)
            except SetupValidationError as exc:
                status_code, headers, body = error_response(str(exc), HTTPStatus.BAD_REQUEST)

            self._write_response(status_code, headers, body)

        def do_GET(self):
            req_path = self.path.split("?", 1)[0].split("#", 1)[0]
            config.touch_session()

            if req_path == "/api/session/heartbeat":
                self._json_response(HTTPStatus.OK, {"ok": True})
                return

            if req_path == "/api/session/status":
                status_callable = getattr(config, "status", None)
                result_callable = getattr(config, "get_last_result", None)
                status = status_callable() if callable(status_callable) else "idle"
                last_result = result_callable() if callable(result_callable) else None
                self._json_response(HTTPStatus.OK, {"ok": True, "status": status, "last_result": last_result})
                return

            if req_path == "/api/session/last-result":
                result_callable = getattr(config, "get_last_result", None)
                last_result = result_callable() if callable(result_callable) else None
                self._json_response(HTTPStatus.OK, {"ok": True, "last_result": last_result})
                return

            if req_path.startswith("/api/setup/"):
                self._route_setup("GET", req_path, {})
                return

            if req_path == "/":
                target = config.html_path
                content_type = "text/html; charset=utf-8"
            else:
                rel_path = req_path.lstrip("/")
                rel_parts = pathlib.PurePosixPath(rel_path)

                # Block traversal while allowing symlinked files inside the UI dir.
                if rel_parts.is_absolute() or ".." in rel_parts.parts:
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                target = config.ui_dir.joinpath(*rel_parts.parts)

                if not target.is_file():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                guessed_type, _ = mimetypes.guess_type(str(target))
                content_type = guessed_type or "application/octet-stream"

            try:
                body = target.read_bytes()
            except OSError as exc:
                self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR, str(exc))
                return

            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            req_path = self.path.split("?", 1)[0].split("#", 1)[0]
            content_length = int(self.headers.get("Content-Length", "0"))
            config.touch_session()

            if req_path == "/api/session/close":
                if content_length > 0:
                    self.rfile.read(content_length)
                removed = config.cleanup_session_runs()
                self._json_response(HTTPStatus.OK, {"ok": True, "removed_run_dirs": removed})
                return

            if req_path == "/api/session/stop":
                if content_length > 0:
                    self.rfile.read(content_length)
                stop_callable = getattr(config, "stop_or_teardown", None)
                removed = stop_callable() if callable(stop_callable) else config.cleanup_session_runs()
                self._json_response(HTTPStatus.OK, {"ok": True, "removed_run_dirs": removed})
                return

            if req_path == "/api/session/heartbeat":
                if content_length > 0:
                    self.rfile.read(content_length)
                self._json_response(HTTPStatus.OK, {"ok": True})
                return

            payload_raw = self.rfile.read(content_length) if content_length > 0 else b"{}"

            try:
                request_payload = parse_json_body(payload_raw)
            except SetupValidationError as exc:
                self._json_response(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
                return

            if req_path.startswith("/api/setup/"):
                self._route_setup("POST", req_path, request_payload)
                return

            if req_path != "/launch":
                self.send_error(HTTPStatus.NOT_FOUND)
                return

            dev_enabled = request_payload.get("dev", True)
            if not isinstance(dev_enabled, bool):
                dev_enabled = bool(dev_enabled)

            launch_callable = getattr(config, "launch_from_setup_state", None)
            try:
                if callable(launch_callable):
                    payload = launch_callable(
                        config.setup_routes.get_state(),
                        cwd=os.getcwd(),
                        dev_enabled=dev_enabled,
                    )
                else:
                    # Backward-compatible path for tests using a lightweight config stub.
                    payload = config.runtime.run_from_setup_state(
                        config.setup_routes.get_state(),
                        cwd=os.getcwd(),
                        dev_enabled=dev_enabled,
                    )
            except OSError as exc:
                self._json_response(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": str(exc)})
                return
            except (SetupValidationError, ValueError) as exc:
                self._json_response(HTTPStatus.BAD_REQUEST, {"error": str(exc), "status": "not-ready"})
                return
            except Exception as exc:  # noqa: BLE001
                payload = config.build_launch_exception_payload(exc, dev_enabled=dev_enabled)
                record_callable = getattr(config, "_record_result", None)
                if callable(record_callable):
                    record_callable(payload)
                self._json_response(HTTPStatus.OK, payload)
                return
            if not isinstance(payload, dict):
                raise TypeError("launch_from_setup_state must return a dict payload")
            if "status" not in payload:
                payload["status"] = "complete" if int(payload.get("return_code", 1)) == 0 else "failed"
            self._json_response(HTTPStatus.OK, payload)

    return LauncherHandler


def main():
    args = parse_args()

    os.chdir(pathlib.Path(args.workdir).expanduser().resolve())

    try:
        emulator_path = validate_and_build_command(args)
    except ValueError as exc:
        raise SystemExit(f"Invalid launcher configuration: {exc}") from exc

    html_path = pathlib.Path(__file__).with_name("index.html")
    if not html_path.exists():
        raise SystemExit(f"Could not find UI file: {html_path}")

    store = InMemorySessionStore()
    setup_routes = SetupRoutes(
        setup_service=SetupService(store),
        source_service=SourceService(store),
        checks_service=ChecksService(store),
    )

    config = LauncherConfig(
        emulator_path=emulator_path,
        html_path=html_path,
        setup_routes=setup_routes,
        engine=args.engine,
        fallback_subprocess=args.fallback_subprocess,
    )
    atexit.register(config.cleanup_all)
    config.start_reaper()
    handler = make_handler(config)

    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        print(f"NWP emulator UI available at http://{args.host}:{args.port}")
        print("Configured emulator binary:")
        print(str(config.emulator_path))
        print(f"Configured launch engine: {config.engine}")
        if config.engine == "python" and config.fallback_subprocess:
            print("Subprocess fallback enabled")
        try:
            server.serve_forever()
        finally:
            config.stop_reaper()
            config.cleanup_all()


if __name__ == "__main__":
    main()
