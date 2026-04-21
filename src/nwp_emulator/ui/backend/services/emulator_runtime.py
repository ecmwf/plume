"""Runtime workspace and execution service for the NWP emulator UI."""

from __future__ import annotations

import datetime
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
import threading
import time
import uuid
from typing import Any

from backend.domain.errors import SetupValidationError

DEFAULT_SESSION_STALE_SECONDS = 120
DEFAULT_REAPER_INTERVAL_SECONDS = 15
MAX_TAIL_CHARS = 6000


def tail_text(text: str, max_chars: int = MAX_TAIL_CHARS) -> str:
    """Return the trailing portion of process output kept for UI responses."""
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def _safe_name(path_str: str) -> str:
    """Normalize a source path into a safe local filename for staged artifacts."""
    return pathlib.Path(path_str).name.replace(" ", "_")


def _write_text_file(target_path: pathlib.Path, text: str) -> None:
    """Create parent directories and write UTF-8 text content to disk."""
    target_path.parent.mkdir(parents=True, exist_ok=True)
    target_path.write_text(text or "", encoding="utf-8")


class EmulatorRuntime:
    """Manages per-session run workspaces, cleanup, and command execution."""

    def __init__(
        self,
        emulator_path: str | pathlib.Path,
        session_id: str | None = None,
        session_stale_seconds: int = DEFAULT_SESSION_STALE_SECONDS,
        reaper_interval_seconds: int = DEFAULT_REAPER_INTERVAL_SECONDS,
        module_search_dirs: list[str | pathlib.Path] | None = None,
    ) -> None:
        self.emulator_path = pathlib.Path(emulator_path).expanduser().resolve()
        self.session_id = session_id or str(uuid.uuid4())
        self.session_stale_seconds = float(session_stale_seconds)
        self.reaper_interval_seconds = float(reaper_interval_seconds)
        self.module_search_dirs = [
            pathlib.Path(path).expanduser().resolve() for path in (module_search_dirs or [])
        ]

        self.temp_root = pathlib.Path(tempfile.mkdtemp(prefix="nwp_emulator_ui_"))
        self.session_root = self.temp_root / self.session_id
        self.session_root.mkdir(parents=True, exist_ok=True)

        self.run_dirs: list[pathlib.Path] = []
        self.last_seen = time.monotonic()

        self._lock = threading.Lock()
        self._stop_event = threading.Event()
        self._reaper_thread: threading.Thread | None = None
        self._status = "idle"
        self._last_result: dict[str, Any] | None = None

    def set_running(self) -> None:
        """Mark runtime as currently executing a launch request."""
        with self._lock:
            self._status = "running"

    def status(self) -> str:
        """Return current runtime status for session ownership endpoints."""
        with self._lock:
            return self._status

    def get_last_result(self) -> dict[str, Any] | None:
        """Return the last recorded launch payload, if any."""
        with self._lock:
            return dict(self._last_result) if self._last_result is not None else None

    def record_result(self, payload: dict[str, Any]) -> None:
        """Persist latest launch result and synchronise status from payload."""
        with self._lock:
            self._last_result = dict(payload)
            self._status = payload.get("status", "failed")

    def touch_session(self) -> None:
        """Record activity for stale-session cleanup decisions."""
        with self._lock:
            self.last_seen = time.monotonic()

    def is_session_stale(self) -> bool:
        """Return whether the session heartbeat exceeded the configured timeout."""
        with self._lock:
            age = time.monotonic() - self.last_seen
        return age >= self.session_stale_seconds

    def cleanup_session_runs(self) -> int:
        """Delete tracked run directories for this session and return the count removed."""
        with self._lock:
            run_dirs = list(self.run_dirs)
            self.run_dirs = []

        removed = 0
        for run_dir in run_dirs:
            if run_dir.exists():
                shutil.rmtree(run_dir, ignore_errors=True)
                removed += 1
        return removed

    def close_session(self) -> int:
        """Compatibility wrapper used by the launcher close endpoint."""
        return self.cleanup_session_runs()

    def stop_or_teardown(self) -> int:
        """Stop active run tracking, cleanup session runs, and reset to idle."""
        removed = self.cleanup_session_runs()
        with self._lock:
            self._status = "idle"
        return removed

    def cleanup_all(self) -> None:
        """Stop background cleanup and remove all session-owned temporary state."""
        self.stop_reaper()
        self.cleanup_session_runs()
        if self.session_root.exists():
            shutil.rmtree(self.session_root, ignore_errors=True)
        if self.temp_root.exists():
            shutil.rmtree(self.temp_root, ignore_errors=True)

    def _reaper_loop(self) -> None:
        """Background loop that cleans stale session runs after inactivity."""
        while not self._stop_event.wait(self.reaper_interval_seconds):
            if self.is_session_stale():
                self.cleanup_session_runs()

    def start_reaper(self) -> None:
        """Start the stale-session cleanup thread if it is not already running."""
        if self._reaper_thread is not None:
            return
        self._stop_event.clear()
        self._reaper_thread = threading.Thread(
            target=self._reaper_loop,
            name="session-run-reaper",
            daemon=True,
        )
        self._reaper_thread.start()

    def stop_reaper(self) -> None:
        """Stop the stale-session cleanup thread and wait briefly for shutdown."""
        self._stop_event.set()
        if self._reaper_thread is not None:
            self._reaper_thread.join(timeout=2)
            self._reaper_thread = None

    def create_run_workspace(self, setup_state: dict[str, Any]) -> dict[str, Any]:
        """Stage one isolated run workspace from the current validated setup state."""
        utc_now = datetime.datetime.now(datetime.timezone.utc)
        run_id = f"run-{utc_now.strftime('%Y%m%dT%H%M%SZ')}-{uuid.uuid4().hex[:8]}"
        run_dir = self.session_root / run_id
        run_dir.mkdir(parents=True, exist_ok=False)

        with self._lock:
            self.run_dirs.append(run_dir)

        options = setup_state.get("options", {})
        run_mode = options.get("run_mode", "config")
        dry_run = bool(options.get("dry_run", False))

        try:
            mpi_np = int(options.get("mpi_np", 1) or 1)
        except (TypeError, ValueError) as exc:
            raise SetupValidationError("mpi_np must be a positive integer") from exc
        if mpi_np < 1:
            mpi_np = 1

        launch_args: list[str] = []
        staged: dict[str, str] = {}

        if run_mode == "grib":
            grib_state = setup_state.get("grib_source", {})
            selected_paths = [str(path) for path in grib_state.get("selected_paths", [])]
            path_display = str(grib_state.get("path_display", "")).strip()
            run_grib_dir = run_dir / "grib"
            run_grib_dir.mkdir(parents=True, exist_ok=True)

            # Stage GRIB inputs into a run-local directory so each launch is
            # self-contained and independent from mutable source locations.
            if selected_paths:
                for path_str in selected_paths:
                    source_path = pathlib.Path(path_str)
                    if not source_path.exists() or not source_path.is_file():
                        raise SetupValidationError(f"Selected GRIB file not found: {source_path}")
                    shutil.copy2(source_path, run_grib_dir / _safe_name(path_str))
            elif path_display:
                source_path = pathlib.Path(path_display)
                if not source_path.exists():
                    raise SetupValidationError(f"GRIB path not found: {source_path}")
                if source_path.is_dir():
                    for item in source_path.iterdir():
                        if item.is_file():
                            shutil.copy2(item, run_grib_dir / item.name)
                elif source_path.is_file():
                    shutil.copy2(source_path, run_grib_dir / source_path.name)
                else:
                    raise SetupValidationError(f"Unsupported GRIB source path: {source_path}")
            else:
                raise SetupValidationError("No GRIB source provided for grib mode")

            launch_args.extend(["--grib-src", str(run_grib_dir)])
            staged["grib_dir"] = str(run_grib_dir)
        else:
            emulator_state = setup_state.get("emulator_config", {})
            emulator_cfg_path = run_dir / "emulator_config.yaml"
            emulator_text = str(emulator_state.get("text", ""))

            # Prefer the in-memory edited text when available so the staged run
            # reflects the exact UI state instead of the last saved file.
            if emulator_text.strip():
                _write_text_file(emulator_cfg_path, emulator_text)
            else:
                path_display = str(emulator_state.get("path_display", "")).strip()
                if not path_display:
                    raise SetupValidationError("No emulator config provided for config mode")
                source_path = pathlib.Path(path_display)
                if not source_path.exists() or not source_path.is_file():
                    raise SetupValidationError(f"Emulator config file not found: {source_path}")
                shutil.copy2(source_path, emulator_cfg_path)

            launch_args.extend(["--config-src", str(emulator_cfg_path)])
            staged["emulator_config"] = str(emulator_cfg_path)

        if not dry_run:
            plume_state = setup_state.get("plume_config", {})
            plume_cfg_path = run_dir / "plume_config.yaml"
            plume_text = str(plume_state.get("text", ""))
            if plume_text.strip():
                _write_text_file(plume_cfg_path, plume_text)
            else:
                path_display = str(plume_state.get("path_display", "")).strip()
                if not path_display:
                    raise SetupValidationError("Plume config is required when dry_run is disabled")
                source_path = pathlib.Path(path_display)
                if not source_path.exists() or not source_path.is_file():
                    raise SetupValidationError(f"Plume config file not found: {source_path}")
                shutil.copy2(source_path, plume_cfg_path)

            launch_args.extend(["--plume-cfg", str(plume_cfg_path)])
            staged["plume_config"] = str(plume_cfg_path)

        run_manifest = {
            "session_id": self.session_id,
            "run_id": run_id,
            "created_at": utc_now.isoformat().replace("+00:00", "Z"),
            "run_mode": run_mode,
            "dry_run": dry_run,
            "mpi_np": mpi_np,
            "staged": staged,
        }
        manifest_path = run_dir / "run_manifest.json"
        manifest_path.write_text(json.dumps(run_manifest, indent=2), encoding="utf-8")

        run_log_path = run_dir / "run.log"
        command = ["mpirun", "-np", str(mpi_np), str(self.emulator_path)] + launch_args

        return {
            "run_id": run_id,
            "run_dir": run_dir,
            "manifest_path": manifest_path,
            "run_log_path": run_log_path,
            "command": command,
        }

    def run_from_setup_state(
        self,
        setup_state: dict[str, Any],
        cwd: str | pathlib.Path,
        dev_enabled: bool,
    ) -> dict[str, Any]:
        """Execute the emulator from setup state and return the UI response payload."""
        self.touch_session()
        run_workspace = self.create_run_workspace(setup_state)

        completed = subprocess.run(
            run_workspace["command"],
            cwd=str(pathlib.Path(cwd).expanduser().resolve()),
            check=False,
            capture_output=True,
            text=True,
        )

        # Persist the complete subprocess transcript to the run workspace even
        # though the UI only displays a tail for compactness.
        run_log_text = "\n".join(
            [
                f"Command: {' '.join(run_workspace['command'])}",
                f"Return code: {completed.returncode}",
                "",
                "--- stdout ---",
                completed.stdout or "",
                "",
                "--- stderr ---",
                completed.stderr or "",
            ]
        )
        run_workspace["run_log_path"].write_text(run_log_text, encoding="utf-8")

        return {
            "command": run_workspace["command"],
            "dev": bool(dev_enabled),
            "return_code": completed.returncode,
            "stdout_tail": tail_text(completed.stdout),
            "stderr_tail": tail_text(completed.stderr),
            "run_id": run_workspace["run_id"],
            "run_dir": str(run_workspace["run_dir"]),
            "run_log": str(run_workspace["run_log_path"]),
        }

    def run_from_setup_state_python(
        self,
        setup_state: dict[str, Any],
        cwd: str | pathlib.Path,
        dev_enabled: bool,
    ) -> dict[str, Any]:
        """Execute through the Python-bound C++ core (single-rank only for now).

        Discovers the pybind11 extension module installed alongside the launcher,
        stages the same isolated run workspace as the subprocess path, then
        translates the staged args into typed ``RunOptions`` and calls
        ``mod.execute()`` directly — no child process involved.

        Raises ``NotImplementedError`` when MPI parallelism is requested (more
        than one rank is not yet supported by the Python engine) or when the
        precision cannot be derived from the binary name, so callers can fall
        back to the subprocess path transparently.

        Raises ``ImportError`` when the pybind module cannot be located or
        loaded, again allowing callers to fall back gracefully.
        """

        self.touch_session()

        options = setup_state.get("options", {})
        try:
            mpi_np = int(options.get("mpi_np", 1) or 1)
        except (TypeError, ValueError):
            mpi_np = 1

        if mpi_np > 1:
            raise NotImplementedError(
                f"Python engine supports single-rank only; "
                f"mpi_np={mpi_np} requires the subprocess engine"
            )

        # Derive the precision suffix from the binary name, e.g.
        # "nwp_emulator_run_dp.x" -> "dp", "nwp_emulator_run_sp.x" -> "sp".
        stem = self.emulator_path.stem
        if stem.endswith("_dp") or stem.endswith("_sp"):
            prec = stem[-2:]
        else:
            raise NotImplementedError(
                f"Cannot determine precision from binary name: {self.emulator_path.name}"
            )

        module_name = f"nwp_emulator_core_{prec}"
        mod = self._import_bound_module(module_name, cwd)

        run_workspace = self.create_run_workspace(setup_state)

        # Translate the staged CLI args embedded in the command list into typed
        # RunOptions, avoiding any re-parsing of setup_state text.
        run_options = self._run_options_from_command(mod, run_workspace["command"])

        # Execute through the C++ core directly (no subprocess, single-rank).
        result, captured_stdout, captured_stderr = self._call_with_captured_fds(mod.execute, run_options)

        run_log_text = "\n".join([
            f"Engine: python ({module_name})",
            f"Return code: {result.return_code}",
            f"Plume run: {result.plume_run}",
            f"Last step run: {result.last_step_run}",
            "",
            "--- stdout ---",
            captured_stdout or "",
            "",
            "--- stderr ---",
            captured_stderr or "",
        ])
        run_workspace["run_log_path"].write_text(run_log_text, encoding="utf-8")

        return {
            "command": run_workspace["command"],
            "dev": bool(dev_enabled),
            "return_code": result.return_code,
            "stdout_tail": tail_text(captured_stdout),
            "stderr_tail": tail_text(captured_stderr),
            "run_id": run_workspace["run_id"],
            "run_dir": str(run_workspace["run_dir"]),
            "run_log": str(run_workspace["run_log_path"]),
            "plume_run": result.plume_run,
            "last_step_run": result.last_step_run,
        }

    def _import_bound_module(self, module_name: str, cwd: str | pathlib.Path) -> Any:
        """Import the pybind extension from known build/UI locations.

        The launcher may execute from a build tree while the Python source files
        are symlinked back to the source checkout. Therefore ``__file__`` for
        this service is not a reliable locator for the built extension module.
        Search explicit runtime paths first, then derive likely build-tree paths
        from the working directory and emulator binary location.
        """
        import sys  # noqa: PLC0415

        cwd_path = pathlib.Path(cwd).expanduser().resolve()
        candidate_dirs: list[pathlib.Path] = []

        def add_candidate(path: pathlib.Path) -> None:
            path = path.expanduser().resolve()
            if path not in candidate_dirs:
                candidate_dirs.append(path)

        for path in self.module_search_dirs:
            add_candidate(path)

        add_candidate(cwd_path / "plume" / "bin")
        add_candidate(cwd_path / "bin")
        add_candidate(self.emulator_path.parent.parent / "plume" / "bin")
        add_candidate(self.emulator_path.parent)
        add_candidate(pathlib.Path(__file__).parents[2].resolve())

        errors: list[str] = []
        for module_dir in candidate_dirs:
            if not module_dir.exists():
                continue
            module_dir_str = str(module_dir)
            if module_dir_str not in sys.path:
                sys.path.insert(0, module_dir_str)
            try:
                return __import__(module_name)
            except ImportError as exc:
                errors.append(f"{module_dir}: {exc}")

        joined = "; ".join(errors) if errors else "no candidate directories existed"
        raise ImportError(f"Could not import {module_name}. Tried: {joined}")

    @staticmethod
    def _call_with_captured_fds(func: Any, *args: Any, **kwargs: Any) -> tuple[Any, str, str]:
        """Call a function while capturing process-level stdout/stderr.

        This captures native writes from C/C++ code (e.g. std::cout/stderr)
        which are not intercepted by Python's text-stream redirection alone.
        """
        import sys  # noqa: PLC0415

        stdout_fd = 1
        stderr_fd = 2
        saved_stdout = os.dup(stdout_fd)
        saved_stderr = os.dup(stderr_fd)

        with tempfile.TemporaryFile(mode="w+b") as out_tmp, tempfile.TemporaryFile(mode="w+b") as err_tmp:
            try:
                os.dup2(out_tmp.fileno(), stdout_fd)
                os.dup2(err_tmp.fileno(), stderr_fd)

                result = func(*args, **kwargs)

                sys.stdout.flush()
                sys.stderr.flush()
            finally:
                os.dup2(saved_stdout, stdout_fd)
                os.dup2(saved_stderr, stderr_fd)
                os.close(saved_stdout)
                os.close(saved_stderr)

            out_tmp.seek(0)
            err_tmp.seek(0)
            stdout_text = out_tmp.read().decode("utf-8", errors="replace")
            stderr_text = err_tmp.read().decode("utf-8", errors="replace")

        return result, stdout_text, stderr_text

    @staticmethod
    def _run_options_from_command(mod: Any, command: list[str]) -> Any:
        """Translate the staged CLI arg list into a bound ``RunOptions`` object.

        The command layout produced by ``create_run_workspace`` is:
        ``["mpirun", "-np", N, binary, *launch_args]``
        so launch args start at index 4.
        """
        args = command[4:]  # skip: mpirun, -np, N, binary
        data_source_type = mod.DataSourceType.INVALID
        data_source_path = ""
        plume_config_path = ""
        it = iter(args)
        for flag in it:
            val = next(it, "")
            if flag == "--grib-src":
                data_source_type = mod.DataSourceType.GRIB
                data_source_path = val
            elif flag == "--config-src":
                data_source_type = mod.DataSourceType.CONFIG
                data_source_path = val
            elif flag == "--plume-cfg":
                plume_config_path = val
        opts = mod.RunOptions()
        opts.data_source_type = data_source_type
        opts.data_source_path = data_source_path
        opts.plume_config_path = plume_config_path
        return opts
