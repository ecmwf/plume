#!/usr/bin/env python3
"""Minimal local web UI to launch the nwp_emulator command from one button."""

import argparse
import json
import mimetypes
import os
import pathlib
import subprocess
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_TAIL_CHARS = 6000
ALLOWED_BIN_NAMES = {"nwp_emulator_run_sp.x", "nwp_emulator_run_dp.x"}


class LauncherConfig:
    def __init__(self, command, html_path):
        self.command = command
        self.html_path = html_path
        self.ui_dir = html_path.parent


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


def validate_and_build_command(args):
    if bool(args.config_src) == bool(args.grib_src):
        raise ValueError("Exactly one of --config-src or --grib-src must be provided")
    if args.mpi_procs < 1:
        raise ValueError("--mpi-procs must be at least 1")

    emulator_path = pathlib.Path(args.emulator_bin).expanduser().resolve()
    if emulator_path.name not in ALLOWED_BIN_NAMES:
        allowed = ", ".join(sorted(ALLOWED_BIN_NAMES))
        raise ValueError(f"--emulator-bin must point to one of: {allowed}")
    if not emulator_path.exists():
        raise ValueError(f"Emulator binary not found: {emulator_path}")

    command = ["mpirun", "-np", str(args.mpi_procs), str(emulator_path)]
    if args.config_src:
        command.append(f"--config-src={args.config_src}")
    if args.grib_src:
        command.append(f"--grib-src={args.grib_src}")
    if args.plume_cfg:
        command.append(f"--plume-cfg={args.plume_cfg}")

    return command


def make_handler(config):
    class LauncherHandler(BaseHTTPRequestHandler):
        def _json_response(self, status, payload):
            body = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/":
                target = config.html_path
                content_type = "text/html; charset=utf-8"
            else:
                req_path = self.path.split("?", 1)[0].split("#", 1)[0]
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
            if self.path != "/launch":
                self.send_error(HTTPStatus.NOT_FOUND)
                return

            content_length = int(self.headers.get("Content-Length", "0"))
            payload_raw = self.rfile.read(content_length) if content_length > 0 else b"{}"

            try:
                request_payload = json.loads(payload_raw.decode("utf-8"))
            except (json.JSONDecodeError, UnicodeDecodeError):
                self._json_response(HTTPStatus.BAD_REQUEST, {"error": "Invalid JSON payload"})
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
    args = parse_args()

    os.chdir(pathlib.Path(args.workdir).expanduser().resolve())

    try:
        # command = validate_and_build_command(args)
        command = ["echo", "Hello, World!"]
    except ValueError as exc:
        raise SystemExit(f"Invalid launcher configuration: {exc}") from exc

    html_path = pathlib.Path(__file__).with_name("index.html")
    if not html_path.exists():
        raise SystemExit(f"Could not find UI file: {html_path}")

    config = LauncherConfig(command=command, html_path=html_path)
    handler = make_handler(config)

    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        print(f"NWP emulator UI available at http://{args.host}:{args.port}")
        print("Configured command:")
        print(" ".join(config.command))
        server.serve_forever()


if __name__ == "__main__":
    main()
