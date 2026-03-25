#!/usr/bin/env python3
"""Minimal local web UI to launch the nwp_emulator command from one button."""

import argparse
import importlib
import mimetypes
import os
import pathlib
import subprocess
import sys
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from backend.api.request_parsers import parse_json_body
from backend.api.response_builders import error_response, json_response
from backend.api.setup_routes import SetupRoutes
from backend.domain.errors import SetupValidationError
from backend.services.checks_service import ChecksService
from backend.services.setup_service import SetupService
from backend.services.source_service import SourceService
from backend.storage.in_memory_store import InMemorySessionStore

MAX_TAIL_CHARS = 6000
ALLOWED_BIN_NAMES = {"nwp_emulator_run_sp.x", "nwp_emulator_run_dp.x"}


class LauncherConfig:
    def __init__(self, command, html_path, setup_routes):
        self.command = command
        self.html_path = html_path
        self.ui_dir = html_path.parent
        self.setup_routes = setup_routes


def tail_text(text, max_chars=MAX_TAIL_CHARS):
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def parse_args():
    parser = argparse.ArgumentParser(description="Serve a minimal UI to launch nwp_emulator")
    parser.add_argument("--host", default="127.0.0.1", help="Host address for the local web server")
    parser.add_argument("--port", type=int, default=8080, help="Port for the local web server")
    parser.add_argument("--emulator-bin", required=True, help="Path to nwp_emulator executable")
    parser.add_argument("--mpi-procs", type=int, default=1, help="MPI process count")
    parser.add_argument("--workdir", default=os.getcwd(), help="Working directory for emulator execution")
    return parser.parse_args()


def ensure_eccodes_available():
    try:
        importlib.import_module("eccodes")
    except Exception as exc:
        lines = [
            "Missing Python dependency: eccodes",
            f"Interpreter: {sys.executable}",
            f"Import error: {exc}",
            "Install with the same interpreter, for example:",
            f"  {sys.executable} -m pip install eccodes",
            "Current sys.path:",
        ]
        lines.extend([f"  - {entry}" for entry in sys.path])
        raise SystemExit("\n".join(lines)) from exc


def validate_and_build_command(args):
    if args.mpi_procs < 1:
        raise ValueError("--mpi-procs must be at least 1")

    emulator_path = pathlib.Path(args.emulator_bin).expanduser().resolve()
    if emulator_path.name not in ALLOWED_BIN_NAMES:
        allowed = ", ".join(sorted(ALLOWED_BIN_NAMES))
        raise ValueError(f"--emulator-bin must point to one of: {allowed}")
    if not emulator_path.exists():
        raise ValueError(f"Emulator binary not found: {emulator_path}")

    command = ["mpirun", "-np", str(args.mpi_procs), str(emulator_path)]
    return command


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

            try:
                completed = subprocess.run(
                    config.command,
                    cwd=os.getcwd(),
                    check=False,
                    capture_output=True,
                    text=True,
                )
            except OSError as exc:
                self._json_response(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": str(exc)})
                return

            payload = {
                "command": config.command,
                "dev": dev_enabled,
                "return_code": completed.returncode,
                "stdout_tail": tail_text(completed.stdout),
                "stderr_tail": tail_text(completed.stderr),
            }
            self._json_response(HTTPStatus.OK, payload)

    return LauncherHandler


def main():
    ensure_eccodes_available()
    args = parse_args()

    os.chdir(pathlib.Path(args.workdir).expanduser().resolve())

    try:
        command = validate_and_build_command(args)
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

    config = LauncherConfig(command=command, html_path=html_path, setup_routes=setup_routes)
    handler = make_handler(config)

    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        print(f"NWP emulator UI available at http://{args.host}:{args.port}")
        print("Configured command:")
        print(" ".join(config.command))
        server.serve_forever()


if __name__ == "__main__":
    main()
