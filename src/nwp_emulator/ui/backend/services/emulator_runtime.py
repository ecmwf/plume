"""Runtime workspace and execution service for the NWP emulator UI.

ARCHITECTURE (Option B: Full-MPI Python-Only Launcher)
======================================================

This module implements Option B execution model: Python-only launcher that
spawns multi-rank Python workers for MPI parallelism.

## Python-Only Runtime Contract
- All /launch requests route through the Python binding path (nwp_emulator_core_{sp|dp}).
- No subprocess emulator fallback; Python binding is the exclusive execution engine.
- The launcher process itself remains single-rank (rank 0 equivalent).
- For mpi_np=1: execute binding directly in launcher process (in-process).
- For mpi_np>1: launcher spawns N Python worker processes via `mpirun -np N python <worker_entrypoint>`.

## Multi-Rank Execution Model
For mpi_np > 1, EmulatorRuntime orchestrates workers as follows:
1. Launcher invokes: `mpirun -np N python -m nwp_emulator.worker <run_dir> <rank>`
2. Each worker process:
   - Initializes MPI context (eckit::mpi::Comm::initialise called within pybind).
   - Loads RunOptions from run_dir/run_options.json (staged by launcher).
   - Executes binding module once (mod.execute(run_options)).
   - Emits result to run_dir/rank_{rank}.json (structured JSON outcome).
3. Launcher (after mpirun completes):
   - Collects per-rank JSON outputs from run_dir/rank_*.json.
   - Aggregates diagnostics and return codes per failure policy.
   - Writes unified run.log with per-rank summaries and merged output.
   - Returns UI payload with deterministic status and aggregated outcome.

## Result Aggregation Contract
For mpi_np > 1 (multi-rank) runs:
- Return code: 0 if all ranks succeed (return_code == 0); non-zero if any rank fails.
- Failure policy: Collect all rank outcomes, then fail with diagnostics (not fail-fast).
  Rationale: Debugging multi-rank runs requires knowledge of which ranks failed and why.
- Per-rank diagnostics: run_dir/rank_*.json files preserved for post-run inspection.
- Unified run.log: Aggregates per-rank command, return code, step completion, and output.

## Response Shape
launcher.launch_from_setup_state() always returns normalized payload:
{
  "command": [...],              # mpirun invocation for multi-rank, direct binding for single-rank
  "dev": bool,                   # dev mode flag
  "return_code": int,            # aggregated: 0 if all ranks=0, else non-zero
  "stdout_tail": str,            # tail of aggregated stdout
  "stderr_tail": str,            # tail of aggregated stderr
  "run_id": str,                 # unique run identifier
  "run_dir": str,                # workspace directory
  "run_log": str,                # path to unified run.log
  "plume_run": str,              # (from binding) plume run value
  "last_step_run": str,          # (from binding) last step execution
}
"""

from __future__ import annotations

import datetime
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from typing import Any

from backend.domain.errors import SetupValidationError

DEFAULT_SESSION_STALE_SECONDS = 120
DEFAULT_REAPER_INTERVAL_SECONDS = 15
MAX_TAIL_CHARS = 6000
_STEP_POLL_INTERVAL = 0.05   # 50 ms between file-existence polls
_STEP_SETUP_TIMEOUT = 30.0   # seconds to wait for all workers to complete setup
_STEP_ADVANCE_TIMEOUT = 120.0  # seconds to wait for each step advance


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
        self._step_session: dict[str, Any] | None = None

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
        self._clear_step_session()
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
            self._last_result = None
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

    def get_active_run_dir(self) -> pathlib.Path | None:
        """Return the most recent run directory that still exists on disk."""
        with self._lock:
            for run_dir in reversed(self.run_dirs):
                if run_dir.exists():
                    return run_dir
        return None

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
        """Execute through the Python-bound C++ core (single or multi-rank).

        For mpi_np=1: Discovers the pybind11 extension module, stages the run
        workspace, translates args to typed RunOptions, and executes directly
        (in-process, no subprocess).

        For mpi_np>1: Orchestrates multi-rank execution by:
        1. Staging run options to JSON in the workspace.
        2. Invoking mpirun with the worker entrypoint.
        3. Collecting per-rank outcomes and aggregating results.
        4. Writing unified run.log and returning aggregated payload.

        Raises ImportError if the pybind module cannot be located, allowing
        callers to fall back gracefully (if fallback is still enabled).
        """

        self.touch_session()

        execution_mode = str(setup_state.get("execution_mode", "full") or "full").lower()
        options = setup_state.get("options", {})
        try:
            mpi_np = int(options.get("mpi_np", 1) or 1)
        except (TypeError, ValueError):
            mpi_np = 1

        # Derive the precision suffix from the binary name, e.g.
        # "nwp_emulator_run_dp.x" -> "dp", "nwp_emulator_run_sp.x" -> "sp".
        stem = self.emulator_path.stem
        if stem.endswith("_dp") or stem.endswith("_sp"):
            prec = stem[-2:]
        else:
            raise RuntimeError(
                f"Cannot determine precision from binary name: {self.emulator_path.name}"
            )

        module_name = f"nwp_emulator_core_{prec}"
        mod = self._import_bound_module(module_name, cwd)

        if execution_mode == "step":
            if mpi_np > 1:
                return self._start_step_session_multirank_python(
                    setup_state, cwd, dev_enabled, mod, module_name, mpi_np
                )
            return self._start_step_session_python(setup_state, dev_enabled, mod, module_name)

        # Route to single-rank or multi-rank path based on mpi_np.
        if mpi_np > 1:
            return self._run_multirank_from_setup_state_python(
                setup_state, cwd, dev_enabled, mpi_np, mod
            )
        else:
            return self._run_single_rank_python(setup_state, cwd, dev_enabled, mod)

    def advance_step_from_session(self) -> dict[str, Any]:
        """Advance an active step session by one step and return updated payload."""
        with self._lock:
            session = self._step_session
            if session is None:
                raise SetupValidationError("No active step session. Press Play to start step mode.")
            run_id = session["run_id"]

        self.touch_session()
        with self._lock:
            session = self._step_session
            if session is None:
                raise SetupValidationError("No active step session. Press Play to start step mode.")

            if session.get("status") == "failed":
                raise SetupValidationError("Current step session is failed. Use Replay to reset.")

            # Multi-rank sessions are advanced via subprocess IPC outside the lock.
            if session.get("proc") is not None:
                return self._advance_step_multirank(session)

            current_step = int(session.get("current_step") or 0)
            total_steps = session.get("total_steps")
            if isinstance(total_steps, int) and total_steps > 0 and current_step >= total_steps:
                stdout_text = str(session.get("finalize_stdout") or "")
                stderr_text = str(session.get("finalize_stderr") or "")
                has_next = False
                session["status"] = "step-finished"
                self._finalize_step_session_locked(session)
                payload = self._build_step_payload_locked(session, stdout_text, stderr_text, has_next)
                self._last_result = dict(payload)
                self._status = payload.get("status", "complete")
                return payload

            core = session["core"]
            dev_enabled = bool(session.get("dev_enabled", False))
            session["status"] = "step-running"

            def _advance_once() -> bool:
                return bool(core.run_step())

            has_step, captured_stdout, captured_stderr = self._with_plugin_dev_capture(_advance_once, dev_enabled)
            if has_step:
                session["current_step"] = int(core.current_step())
                snapshot_path, field_keys = self._capture_single_rank_step_field_snapshot(
                    pathlib.Path(session["run_dir"]),
                    core,
                    int(session["current_step"]),
                )
                if field_keys:
                    session["field_keys"] = field_keys
                if snapshot_path is not None:
                    step_field_files = session.get("step_field_files")
                    if not isinstance(step_field_files, dict):
                        step_field_files = {}
                    step_field_files[str(int(session["current_step"]))] = str(snapshot_path)
                    session["step_field_files"] = step_field_files
                has_next = self._step_has_next(session)
                if has_next:
                    session["status"] = "step-complete"
                else:
                    session["status"] = "step-finished"
                    self._finalize_step_session_locked(session)
            else:
                has_next = False
                session["status"] = "step-finished"
                self._finalize_step_session_locked(session)

            if not has_next:
                finalize_stdout = str(session.get("finalize_stdout") or "").strip()
                finalize_stderr = str(session.get("finalize_stderr") or "").strip()
                if finalize_stdout:
                    if captured_stdout:
                        captured_stdout += "\n\n"
                    captured_stdout += "=== FINALIZATION ===\n" + finalize_stdout
                if finalize_stderr:
                    if captured_stderr:
                        captured_stderr += "\n\n"
                    captured_stderr += "=== FINALIZATION STDERR ===\n" + finalize_stderr

            self._append_step_log_locked(
                session,
                captured_stdout,
                captured_stderr,
                action="advance",
                has_step=has_step,
            )
            payload = self._build_step_payload_locked(session, captured_stdout, captured_stderr, has_next)
            self._last_result = dict(payload)
            self._status = payload.get("status", "complete")
            return payload

        raise SetupValidationError(f"No active step session for run: {run_id}")

    def _start_step_session_python(
        self,
        setup_state: dict[str, Any],
        dev_enabled: bool,
        mod: Any,
        module_name: str,
    ) -> dict[str, Any]:
        """Initialize step mode and execute exactly one step for Play."""
        self._clear_step_session()
        run_workspace = self.create_run_workspace(setup_state)
        run_options = self._run_options_from_command(mod, run_workspace["command"])

        core_ctor = getattr(mod, "NWPEmulatorCore", None)
        if core_ctor is None:
            raise RuntimeError("Binding module does not expose NWPEmulatorCore")
        core = core_ctor()

        total_steps = self._infer_total_steps(setup_state, run_workspace)

        def _launch_first_step() -> tuple[bool, bool, bool]:
            valid = bool(core.validate_run_options(run_options))
            if not valid:
                return False, False, False

            setup_ok = bool(core.setup_data_provider(run_options))
            if not setup_ok:
                return False, False, False

            plume_run = False
            if getattr(run_options, "plume_config_path", ""):
                plume_ok = bool(core.setup_plume_provider())
                if not plume_ok:
                    return True, False, False
                plume_run = True

            first_step = bool(core.run_step())
            return True, plume_run, first_step

        launch_result, captured_stdout, captured_stderr = self._with_plugin_dev_capture(_launch_first_step, dev_enabled)
        valid_opts, plume_run, first_step = launch_result
        if not valid_opts:
            raise SetupValidationError("Invalid emulator run options")
        if not first_step:
            if hasattr(core, "finalize_run"):
                core.finalize_run()
            else:
                core.finalize_plume()

        current_step = int(core.current_step()) if first_step else 0
        step_field_files: dict[str, str] = {}
        field_keys: list[str] = []
        if first_step and current_step > 0:
            snapshot_path, field_keys = self._capture_single_rank_step_field_snapshot(
                run_workspace["run_dir"],
                core,
                current_step,
            )
            if snapshot_path is not None:
                step_field_files[str(current_step)] = str(snapshot_path)

        session = {
            "session_id": self.session_id,
            "run_id": run_workspace["run_id"],
            "execution_mode": "step",
            "run_dir": run_workspace["run_dir"],
            "run_log_path": run_workspace["run_log_path"],
            "command": run_workspace["command"],
            "dev_enabled": bool(dev_enabled),
            "core": core,
            "plume_run": bool(plume_run),
            "current_step": current_step,
            "total_steps": total_steps,
            "status": "step-running",
            "is_finalized": not first_step,
            "module_name": module_name,
            "field_keys": field_keys,
            "step_field_files": step_field_files,
        }

        has_next = first_step and self._step_has_next(session)
        if first_step and has_next:
            session["status"] = "step-complete"
            with self._lock:
                self._step_session = session
        else:
            session["status"] = "step-finished"
            self._finalize_step_session_locked(session)

        if not has_next:
            finalize_stdout = str(session.get("finalize_stdout") or "").strip()
            finalize_stderr = str(session.get("finalize_stderr") or "").strip()
            if finalize_stdout:
                if captured_stdout:
                    captured_stdout += "\n\n"
                captured_stdout += "=== FINALIZATION ===\n" + finalize_stdout
            if finalize_stderr:
                if captured_stderr:
                    captured_stderr += "\n\n"
                captured_stderr += "=== FINALIZATION STDERR ===\n" + finalize_stderr

        with self._lock:
            self._append_step_log_locked(
                session,
                captured_stdout,
                captured_stderr,
                action="launch",
                has_step=first_step,
            )
            payload = self._build_step_payload_locked(session, captured_stdout, captured_stderr, has_next)
            self._last_result = dict(payload)
            self._status = payload.get("status", "failed")
        return payload

    @staticmethod
    def _infer_total_steps(setup_state: dict[str, Any], run_workspace: dict[str, Any]) -> int | None:
        """Infer total steps from setup state when available."""
        options = setup_state.get("options", {})
        run_mode = str(options.get("run_mode", "config") or "config")

        if run_mode == "grib":
            grib_state = setup_state.get("grib_source", {})
            selected = grib_state.get("selected_paths", []) or []
            if selected:
                return len(selected)
            grib_dir = run_workspace["run_dir"] / "grib"
            if grib_dir.exists():
                return sum(1 for item in grib_dir.iterdir() if item.is_file())
            return None

        emulator_text = str((setup_state.get("emulator_config") or {}).get("text", ""))
        match = re.search(r"^\s*n_steps\s*:\s*(\d+)\s*$", emulator_text, flags=re.MULTILINE)
        if match:
            try:
                return int(match.group(1))
            except ValueError:
                return None

        emulator_cfg = run_workspace["run_dir"] / "emulator_config.yaml"
        if emulator_cfg.exists():
            text = emulator_cfg.read_text(encoding="utf-8")
            match = re.search(r"^\s*n_steps\s*:\s*(\d+)\s*$", text, flags=re.MULTILINE)
            if match:
                try:
                    return int(match.group(1))
                except ValueError:
                    return None
        return None

    @staticmethod
    def _step_has_next(session: dict[str, Any]) -> bool:
        total_steps = session.get("total_steps")
        current_step = int(session.get("current_step") or 0)
        if isinstance(total_steps, int) and total_steps > 0:
            return current_step < total_steps
        return True

    def _build_step_payload_locked(
        self,
        session: dict[str, Any],
        stdout_text: str,
        stderr_text: str,
        has_next: bool,
    ) -> dict[str, Any]:
        finished = not has_next
        status = "complete" if finished else "step-complete"
        current_step = int(session.get("current_step") or 0)
        step_field_files_raw = session.get("step_field_files")
        step_field_files: dict[str, str] = step_field_files_raw if isinstance(step_field_files_raw, dict) else {}
        return {
            "command": session.get("command", []),
            "dev": bool(session.get("dev_enabled", False)),
            "return_code": 0,
            "stdout_tail": tail_text(stdout_text),
            "stderr_tail": tail_text(stderr_text),
            "run_id": session.get("run_id", "unknown"),
            "run_dir": str(session.get("run_dir", "unknown")),
            "run_log": str(session.get("run_log_path", "unknown")),
            "plume_run": bool(session.get("plume_run", False)),
            "last_step_run": current_step,
            "execution_mode": "step",
            "current_step": current_step,
            "total_steps": session.get("total_steps"),
            "has_next": bool(has_next),
            "step_finished": finished,
            "status": status,
            "field_keys": session.get("field_keys", []),
            "field_snapshot": step_field_files.get(str(current_step)),
            "mpi_np": int(str(session.get("command", ["mpirun", "-np", "1"])[2])),
        }

    @staticmethod
    def _append_step_log_locked(
        session: dict[str, Any],
        stdout_text: str,
        stderr_text: str,
        action: str,
        has_step: bool,
    ) -> None:
        run_log_path = session.get("run_log_path")
        if not isinstance(run_log_path, pathlib.Path):
            return

        step_value = int(session.get("current_step") or 0)
        section = "\n".join(
            [
                f"[{action}] step_executed={has_step}",
                f"Current step: {step_value}",
                f"Status: {session.get('status', 'unknown')}",
                "",
                "--- stdout ---",
                stdout_text or "",
                "",
                "--- stderr ---",
                stderr_text or "",
                "",
            ]
        )

        if run_log_path.exists():
            with run_log_path.open("a", encoding="utf-8") as handle:
                handle.write(section)
        else:
            header = "\n".join(
                [
                    "Engine: python (step mode)",
                    f"Run ID: {session.get('run_id', 'unknown')}",
                    "",
                ]
            )
            run_log_path.write_text(header + section, encoding="utf-8")

    @staticmethod
    def _with_plugin_dev_capture(func: Any, dev_enabled: bool) -> tuple[Any, str, str]:
        prev = os.environ.get("PLUME_PLUGIN_DEV")
        os.environ["PLUME_PLUGIN_DEV"] = "true" if dev_enabled else "false"
        try:
            return EmulatorRuntime._call_with_captured_fds(func)
        finally:
            if prev is None:
                os.environ.pop("PLUME_PLUGIN_DEV", None)
            else:
                os.environ["PLUME_PLUGIN_DEV"] = prev

    @staticmethod
    def _read_text_delta(path: pathlib.Path, offset: int) -> tuple[str, int]:
        """Read text appended after offset from a log file and return new offset."""
        if not path.exists():
            return "", offset
        try:
            with path.open("rb") as handle:
                handle.seek(max(0, offset))
                chunk = handle.read()
                new_offset = handle.tell()
        except OSError:
            return "", offset
        return chunk.decode("utf-8", errors="replace"), new_offset

    @classmethod
    def _consume_worker_output_deltas(cls, session: dict[str, Any]) -> tuple[str, str]:
        """Consume only new worker stdout/stderr since the last step payload."""
        stdout_path = session.get("worker_stdout_path")
        stderr_path = session.get("worker_stderr_path")

        stdout_delta = ""
        stderr_delta = ""

        if isinstance(stdout_path, pathlib.Path):
            current_offset = int(session.get("worker_stdout_offset") or 0)
            stdout_delta, next_offset = cls._read_text_delta(stdout_path, current_offset)
            session["worker_stdout_offset"] = next_offset

        if isinstance(stderr_path, pathlib.Path):
            current_offset = int(session.get("worker_stderr_offset") or 0)
            stderr_delta, next_offset = cls._read_text_delta(stderr_path, current_offset)
            session["worker_stderr_offset"] = next_offset

        return stdout_delta.strip(), stderr_delta.strip()

    @staticmethod
    def _aggregate_finalize_rank_results(run_dir: pathlib.Path, mpi_np: int) -> tuple[str, str]:
        """Collect per-rank finalization outputs written by workers."""
        stdout_parts: list[str] = []
        stderr_parts: list[str] = []

        for r in range(mpi_np):
            fname = run_dir / f"rank_{r}_finalized.json"
            try:
                data = json.loads(fname.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                stderr_parts.append(f"[rank {r}] missing finalization result")
                continue

            stdout_parts.append(
                f"[rank {r}] finalization status={data.get('status', 'unknown')} "
                f"return_code={data.get('return_code', 'n/a')}"
            )
            if data.get("stdout"):
                stdout_parts.append(f"[rank {r}] {data['stdout']}")
            if data.get("stderr"):
                stderr_parts.append(f"[rank {r}] {data['stderr']}")

        return "\n".join(stdout_parts), "\n".join(stderr_parts)

    def _finalize_step_session_locked(self, session: dict[str, Any]) -> None:
        if session.get("is_finalized"):
            return
        core = session.get("core")
        if core is not None:
            def _finalize_single_rank() -> None:
                if hasattr(core, "finalize_run"):
                    core.finalize_run()
                else:
                    core.finalize_plume()

            try:
                _, finalize_stdout, finalize_stderr = self._with_plugin_dev_capture(
                    _finalize_single_rank,
                    bool(session.get("dev_enabled", False)),
                )
                session["finalize_stdout"] = (session.get("finalize_stdout") or "") + finalize_stdout
                session["finalize_stderr"] = (session.get("finalize_stderr") or "") + finalize_stderr
            except (AttributeError, RuntimeError, TypeError, ValueError):
                pass

        # For multi-rank sessions, request worker finalization and then terminate subprocess.
        proc = session.get("proc")
        if proc is not None:
            run_dir = session.get("run_dir")
            if isinstance(run_dir, pathlib.Path):
                try:
                    (run_dir / "step_finalize").touch()
                except OSError:
                    pass

                mpi_np = int(session.get("mpi_np") or 1)
                finalized = self._wait_for_rank_files(
                    run_dir, mpi_np, "rank_{rank}_finalized.json", _STEP_ADVANCE_TIMEOUT
                )
                if finalized:
                    finalize_stdout, finalize_stderr = self._aggregate_finalize_rank_results(run_dir, mpi_np)
                    session["finalize_stdout"] = finalize_stdout
                    session["finalize_stderr"] = finalize_stderr

                worker_stdout_delta, worker_stderr_delta = self._consume_worker_output_deltas(session)
                if worker_stdout_delta:
                    prior = str(session.get("finalize_stdout") or "")
                    session["finalize_stdout"] = (
                        f"{prior}\n\n=== WORKER PROCESS STDOUT ===\n{worker_stdout_delta}".strip()
                    )
                if worker_stderr_delta:
                    prior = str(session.get("finalize_stderr") or "")
                    session["finalize_stderr"] = (
                        f"{prior}\n\n=== WORKER PROCESS STDERR ===\n{worker_stderr_delta}".strip()
                    )

                try:
                    (run_dir / "step_abort").touch()
                except OSError:
                    pass
            self._terminate_proc(proc)
        session["is_finalized"] = True

    @staticmethod
    def _terminate_proc(proc: Any) -> None:
        """Terminate a subprocess cleanly, killing it if it does not stop promptly."""
        try:
            proc.terminate()
            proc.wait(timeout=2)
        except (ProcessLookupError, OSError):
            pass
        except subprocess.TimeoutExpired:
            try:
                proc.kill()
            except (ProcessLookupError, OSError):
                pass

    def _clear_step_session(self) -> None:
        with self._lock:
            session = self._step_session
            self._step_session = None
        if session is None:
            return
        self._finalize_step_session_locked(session)

    # ------------------------------------------------------------------
    # Multi-rank step mode helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _wait_for_rank_files(
        run_dir: pathlib.Path,
        mpi_np: int,
        filename_template: str,
        timeout: float,
    ) -> bool:
        """Poll until all rank files exist or timeout expires.

        filename_template must contain ``{rank}`` which is substituted with the
        rank index, e.g. ``"rank_{rank}_ready.json"``.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if all(
                (run_dir / filename_template.format(rank=r)).exists()
                for r in range(mpi_np)
            ):
                return True
            time.sleep(_STEP_POLL_INTERVAL)
        return False

    @staticmethod
    def _aggregate_step_rank_results(
        run_dir: pathlib.Path,
        mpi_np: int,
        step: int,
    ) -> dict[str, Any]:
        """Collect per-rank step-done JSON files and merge into one result."""
        rank_results = []
        for r in range(mpi_np):
            fname = run_dir / f"rank_{r}_step_{step}_done.json"
            try:
                data = json.loads(fname.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                data = {"rank": r, "has_step": False, "return_code": 1, "status": "failed"}
            rank_results.append(data)

        has_step = all(d.get("has_step", False) for d in rank_results)
        plume_run = any(d.get("plume_run", False) for d in rank_results)

        stdout_parts: list[str] = []
        stderr_parts: list[str] = []
        for data in rank_results:
            r = data.get("rank", "?")
            stdout_parts.append(
                f"[rank {r}] step={step} has_step={bool(data.get('has_step', False))} "
                f"status={data.get('status', 'unknown')} return_code={data.get('return_code', 'n/a')}"
            )
            if data.get("stdout"):
                stdout_parts.append(f"[rank {r}] {data['stdout']}")
            if data.get("stderr"):
                stderr_parts.append(f"[rank {r}] {data['stderr']}")

        return {
            "has_step": has_step,
            "plume_run": plume_run,
            "stdout": "\n".join(stdout_parts),
            "stderr": "\n".join(stderr_parts),
            "return_code": 0 if has_step else 1,
        }

    @staticmethod
    def _archive_full_resolution_fields(run_dir: pathlib.Path) -> pathlib.Path | None:
        """Archive full-resolution field data before sampled data overwrites it.
        
        Creates a tar.gz archive containing all per-step field JSON files for
        post-run analysis. Returns path to archive, or None if no fields exist.
        """
        map_fields_dir = run_dir / "map_fields"
        if not map_fields_dir.exists() or not list(map_fields_dir.glob("step_*.json")):
            return None
        
        archive_path = run_dir / "map_fields_full_resolution.tar.gz"
        try:
            import tarfile
            with tarfile.open(archive_path, "w:gz") as tar:
                for step_file in sorted(map_fields_dir.glob("step_*.json")):
                    tar.add(step_file, arcname=step_file.name)
            return archive_path
        except (OSError, tarfile.TarError) as e:
            # Non-fatal: log error but don't fail the run
            print(f"Warning: Failed to archive full-resolution fields: {e}", file=sys.stderr)
            return None

    @staticmethod
    def _persist_step_field_snapshot(
        run_dir: pathlib.Path,
        step: int,
        snapshot: dict[str, Any],
    ) -> pathlib.Path:
        """Persist field snapshot to compact JSON.

        Frontend point-capping is applied at render time when enabled.
        """
        field_dir = run_dir / "map_fields"
        field_dir.mkdir(parents=True, exist_ok=True)

        out_path = field_dir / f"step_{step}.json"
        # Compact JSON avoids overhead from pretty-printing huge arrays.
        out_path.write_text(json.dumps(snapshot), encoding="utf-8")
        return out_path

    @staticmethod
    def _capture_single_rank_step_field_snapshot(
        run_dir: pathlib.Path,
        core: Any,
        step: int,
    ) -> tuple[pathlib.Path | None, list[str]]:
        if not hasattr(core, "available_field_keys") or not hasattr(core, "get_field_overlay_snapshot"):
            return None, []

        try:
            field_keys = [str(k) for k in list(core.available_field_keys())]
        except (TypeError, ValueError, RuntimeError, AttributeError):
            field_keys = []

        fields: dict[str, Any] = {}
        for field_key in field_keys:
            try:
                snap = core.get_field_overlay_snapshot(field_key)
            except (TypeError, ValueError, RuntimeError, AttributeError):
                continue
            fields[field_key] = {
                "lon": snap.get("lon", []),
                "lat": snap.get("lat", []),
                "values": snap.get("values", []),
                "step": int(snap.get("step", step)),
                "rank": int(snap.get("rank", 0)),
                "nprocs": int(snap.get("nprocs", 1)),
            }

        if not fields:
            return None, field_keys

        snapshot = {
            "step": int(step),
            "field_keys": field_keys,
            "fields": fields,
            "aggregated": True,
            "source": "single-rank",
        }
        return EmulatorRuntime._persist_step_field_snapshot(run_dir, step, snapshot), field_keys

    @staticmethod
    def _aggregate_step_rank_field_snapshots(
        run_dir: pathlib.Path,
        mpi_np: int,
        step: int,
    ) -> tuple[pathlib.Path | None, list[str]]:
        aggregated_fields: dict[str, dict[str, Any]] = {}
        field_keys_set: set[str] = set()

        for r in range(mpi_np):
            rank_file = run_dir / f"rank_{r}_step_{step}_fields.json"
            try:
                payload = json.loads(rank_file.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue

            fields = payload.get("fields") if isinstance(payload.get("fields"), dict) else {}
            for key, data in fields.items():
                field_key = str(key)
                field_keys_set.add(field_key)
                target = aggregated_fields.setdefault(field_key, {
                    "lon": [],
                    "lat": [],
                    "values": [],
                    "step": int(step),
                    "rank": -1,
                    "nprocs": int(mpi_np),
                })
                target["lon"].extend(data.get("lon", []))
                target["lat"].extend(data.get("lat", []))
                target["values"].extend(data.get("values", []))

        if not aggregated_fields:
            return None, sorted(field_keys_set)

        field_keys = sorted(field_keys_set)
        snapshot = {
            "step": int(step),
            "field_keys": field_keys,
            "fields": aggregated_fields,
            "aggregated": True,
            "source": "multi-rank",
        }
        return EmulatorRuntime._persist_step_field_snapshot(run_dir, step, snapshot), field_keys

    def _start_step_session_multirank_python(
        self,
        setup_state: dict[str, Any],
        cwd: str | pathlib.Path,
        dev_enabled: bool,
        mod: Any,
        module_name: str,
        mpi_np: int,
    ) -> dict[str, Any]:
        """Initialize a multi-rank step session and execute the first step.

        Spawns mpirun workers running in step mode.  Workers use file-based IPC
        (see emulator_worker._run_step_mode) to coordinate step execution with
        this launcher process.
        """
        self._clear_step_session()
        run_workspace = self.create_run_workspace(setup_state)
        run_dir = run_workspace["run_dir"]
        run_log_path: pathlib.Path = run_workspace["run_log_path"]
        cwd_path = pathlib.Path(cwd).expanduser().resolve()

        # Stage run options so workers can reconstruct RunOptions without setup_state.
        run_options = self._run_options_from_command(mod, run_workspace["command"])
        data_source_type_name = getattr(run_options.data_source_type, "name", None)
        if not isinstance(data_source_type_name, str) or not data_source_type_name:
            data_source_type_name = str(run_options.data_source_type)
        options_json = {
            "data_source_type": data_source_type_name,
            "data_source_path": run_options.data_source_path,
            "plume_config_path": run_options.plume_config_path,
            "module_name": module_name,
        }
        (run_dir / "run_options.json").write_text(
            json.dumps(options_json, indent=2), encoding="utf-8"
        )

        # Assemble mpirun command pointing to the step-mode worker.
        worker_script = pathlib.Path(__file__).resolve().parent / "emulator_worker.py"
        msd_candidates: list[pathlib.Path] = [
            *self.module_search_dirs,
            cwd_path / "plume" / "bin",
            cwd_path / "bin",
            self.emulator_path.parent.parent / "plume" / "bin",
            self.emulator_path.parent,
        ]
        msd_args = [str(p) for p in msd_candidates if p.exists()]

        worker_cmd = [
            "mpirun", "-np", str(mpi_np),
            "-x", "PLUME_PLUGIN_DEV",
            "python", str(worker_script),
            str(run_dir),
            "--step-mode",
            "--module-search-dirs",
            *msd_args,
        ]

        # Always write a run log for multi-rank step mode, even when setup fails.
        run_log_path.write_text(
            "\n".join(
                [
                    "Engine: python (step mode, multi-rank)",
                    f"Command: {' '.join(worker_cmd)}",
                    f"mpi_np: {mpi_np}",
                    f"Run ID: {run_workspace['run_id']}",
                    "",
                ]
            ),
            encoding="utf-8",
        )

        def _append_launch_diagnostic(message: str) -> None:
            with run_log_path.open("a", encoding="utf-8") as handle:
                handle.write(message + "\n")

        # Create output files to capture worker logs (enables C++ logging to be captured).
        worker_stdout_path = run_dir / "worker_stdout.log"
        worker_stderr_path = run_dir / "worker_stderr.log"

        _prev_dev = os.environ.get("PLUME_PLUGIN_DEV")
        os.environ["PLUME_PLUGIN_DEV"] = "true" if dev_enabled else "false"
        try:
            with open(worker_stdout_path, "w", encoding="utf-8") as stdout_file, \
                  open(worker_stderr_path, "w", encoding="utf-8") as stderr_file:
                proc = subprocess.Popen(
                    worker_cmd,
                    cwd=str(cwd_path),
                    text=True,
                    stdout=stdout_file,
                    stderr=stderr_file,
                )
        finally:
            if _prev_dev is None:
                os.environ.pop("PLUME_PLUGIN_DEV", None)
            else:
                os.environ["PLUME_PLUGIN_DEV"] = _prev_dev

        # Wait for all workers to complete provider setup.
        ready = self._wait_for_rank_files(
            run_dir, mpi_np, "rank_{rank}_ready.json", _STEP_SETUP_TIMEOUT
        )
        if not ready:
            self._terminate_proc(proc)
            _append_launch_diagnostic(
                f"ERROR: workers did not signal readiness within {_STEP_SETUP_TIMEOUT}s"
            )
            raise RuntimeError(
                f"Step-mode workers did not signal readiness within {_STEP_SETUP_TIMEOUT}s"
            )

        # Abort if any rank failed during setup.
        # Also collect setup output from all ready files.
        setup_outputs: list[str] = []
        for r in range(mpi_np):
            data = json.loads(
                (run_dir / f"rank_{r}_ready.json").read_text(encoding="utf-8")
            )
            if data.get("status") != "ready":
                (run_dir / "step_abort").touch()
                self._terminate_proc(proc)
                _append_launch_diagnostic(
                    f"ERROR: rank {r} setup failed: {data.get('error', 'unknown')}"
                )
                raise SetupValidationError(
                    f"Rank {r} failed during step-mode setup: {data.get('error', 'unknown')}"
                )
            
            # Collect setup output from this rank.
            setup_stdout = data.get("stdout", "").strip()
            setup_stderr = data.get("stderr", "").strip()
            # Always record setup status, with output if available.
            if setup_stdout:
                setup_outputs.append(f"[rank {r}] Setup stdout: {setup_stdout}")
            if setup_stderr:
                setup_outputs.append(f"[rank {r}] Setup stderr: {setup_stderr}")
            if not setup_stdout and not setup_stderr:
                setup_outputs.append(f"[rank {r}] Setup completed (no diagnostic output)")

        # Always append setup phase block to run log.
        _append_launch_diagnostic("")
        _append_launch_diagnostic("=== SETUP PHASE (Emulator Core + Data Provider Initialization) ===")
        for line in setup_outputs:
            _append_launch_diagnostic(line)
        _append_launch_diagnostic("=== END SETUP PHASE ===")
        _append_launch_diagnostic("")

        # Append captured worker process output (stdout/stderr from mpirun workers)
        worker_stdout_path = run_dir / "worker_stdout.log"
        worker_stderr_path = run_dir / "worker_stderr.log"
        if worker_stdout_path.exists():
            worker_stdout = worker_stdout_path.read_text(encoding="utf-8").strip()
            if worker_stdout:
                _append_launch_diagnostic("=== WORKER PROCESS OUTPUT (stdout) ===")
                _append_launch_diagnostic(worker_stdout)
                _append_launch_diagnostic("=== END WORKER STDOUT ===")
                _append_launch_diagnostic("")
        if worker_stderr_path.exists():
            worker_stderr = worker_stderr_path.read_text(encoding="utf-8").strip()
            if worker_stderr:
                _append_launch_diagnostic("=== WORKER PROCESS OUTPUT (stderr) ===")
                _append_launch_diagnostic(worker_stderr)
                _append_launch_diagnostic("=== END WORKER STDERR ===")
                _append_launch_diagnostic("")

        # Signal workers to execute step 1.
        (run_dir / "step_advance_1").touch()

        done = self._wait_for_rank_files(
            run_dir, mpi_np, "rank_{rank}_step_1_done.json", _STEP_ADVANCE_TIMEOUT
        )
        if not done:
            (run_dir / "step_abort").touch()
            self._terminate_proc(proc)
            _append_launch_diagnostic(
                f"ERROR: workers did not complete step 1 within {_STEP_ADVANCE_TIMEOUT}s"
            )
            raise RuntimeError(
                f"Step-mode workers did not complete step 1 within {_STEP_ADVANCE_TIMEOUT}s"
            )

        aggregated = self._aggregate_step_rank_results(run_dir, mpi_np, step=1)
        has_step = aggregated["has_step"]
        plume_run = bool(aggregated.get("plume_run", False))
        total_steps = self._infer_total_steps(setup_state, run_workspace)

        current_step = 1 if has_step else 0
        step_field_files: dict[str, str] = {}
        field_keys: list[str] = []
        if has_step:
            snapshot_path, field_keys = self._aggregate_step_rank_field_snapshots(run_dir, mpi_np, step=1)
            if snapshot_path is not None:
                step_field_files[str(current_step)] = str(snapshot_path)

        session: dict[str, Any] = {
            "session_id": self.session_id,
            "run_id": run_workspace["run_id"],
            "execution_mode": "step",
            "run_dir": run_dir,
            "run_log_path": run_workspace["run_log_path"],
            "command": worker_cmd,
            "dev_enabled": bool(dev_enabled),
            "core": None,       # multi-rank: step execution is in worker processes
            "proc": proc,
            "mpi_np": mpi_np,
            "plume_run": plume_run,
            "current_step": current_step,
            "total_steps": total_steps,
            "status": "step-running",
            "is_finalized": False,
            "module_name": module_name,
            "worker_stdout_path": worker_stdout_path,
            "worker_stderr_path": worker_stderr_path,
            "worker_stdout_offset": 0,
            "worker_stderr_offset": 0,
            "finalize_stdout": "",
            "finalize_stderr": "",
            "field_keys": field_keys,
            "step_field_files": step_field_files,
        }

        has_next = has_step and self._step_has_next(session)
        if has_step and has_next:
            session["status"] = "step-complete"
            with self._lock:
                self._step_session = session
        else:
            session["status"] = "step-finished"
            self._finalize_step_session_locked(session)

        stdout_text = aggregated.get("stdout", "")
        stderr_text = aggregated.get("stderr", "")

        worker_stdout_delta, worker_stderr_delta = self._consume_worker_output_deltas(session)
        if worker_stdout_delta:
            if stdout_text:
                stdout_text += "\n\n"
            stdout_text += "=== WORKER PROCESS STDOUT ===\n" + worker_stdout_delta
        if worker_stderr_delta:
            if stderr_text:
                stderr_text += "\n\n"
            stderr_text += "=== WORKER PROCESS STDERR ===\n" + worker_stderr_delta

        if not has_next:
            finalize_stdout = str(session.get("finalize_stdout") or "").strip()
            finalize_stderr = str(session.get("finalize_stderr") or "").strip()
            if finalize_stdout:
                if stdout_text:
                    stdout_text += "\n\n"
                stdout_text += "=== FINALIZATION ===\n" + finalize_stdout
            if finalize_stderr:
                if stderr_text:
                    stderr_text += "\n\n"
                stderr_text += "=== FINALIZATION STDERR ===\n" + finalize_stderr

        with self._lock:
            self._append_step_log_locked(
                session, stdout_text, stderr_text, action="launch", has_step=has_step
            )
            payload = self._build_step_payload_locked(session, stdout_text, stderr_text, has_next)
            self._last_result = dict(payload)
            self._status = payload.get("status", "failed")
        return payload

    def _advance_step_multirank(self, session: dict[str, Any]) -> dict[str, Any]:
        """Advance a multi-rank step session by signaling workers and waiting."""
        current_step = int(session.get("current_step") or 0)
        total_steps = session.get("total_steps")
        mpi_np = int(session.get("mpi_np") or 1)
        run_dir: pathlib.Path = session["run_dir"]

        # Already at terminal: return without running more.
        if isinstance(total_steps, int) and total_steps > 0 and current_step >= total_steps:
            session["status"] = "step-finished"
            self._finalize_step_session_locked(session)
            payload = self._build_step_payload_locked(
                session,
                str(session.get("finalize_stdout") or ""),
                str(session.get("finalize_stderr") or ""),
                False,
            )
            self._last_result = dict(payload)
            self._status = payload.get("status", "complete")
            return payload

        next_step = current_step + 1
        session["status"] = "step-running"

        (run_dir / f"step_advance_{next_step}").touch()

        done = self._wait_for_rank_files(
            run_dir, mpi_np, f"rank_{{rank}}_step_{next_step}_done.json", _STEP_ADVANCE_TIMEOUT
        )
        if not done:
            session["status"] = "failed"
            self._append_step_log_locked(
                session,
                "",
                f"Step-mode workers did not complete step {next_step} within {_STEP_ADVANCE_TIMEOUT}s",
                action="advance-timeout",
                has_step=False,
            )
            raise RuntimeError(
                f"Step-mode workers did not complete step {next_step} within {_STEP_ADVANCE_TIMEOUT}s"
            )

        aggregated = self._aggregate_step_rank_results(run_dir, mpi_np, step=next_step)
        has_step = aggregated["has_step"]

        if has_step:
            session["current_step"] = next_step
            snapshot_path, field_keys = self._aggregate_step_rank_field_snapshots(run_dir, mpi_np, step=next_step)
            if field_keys:
                session["field_keys"] = field_keys
            if snapshot_path is not None:
                step_field_files = session.get("step_field_files")
                if not isinstance(step_field_files, dict):
                    step_field_files = {}
                step_field_files[str(next_step)] = str(snapshot_path)
                session["step_field_files"] = step_field_files

        has_next = has_step and self._step_has_next(session)
        if not has_next:
            session["status"] = "step-finished"
            self._finalize_step_session_locked(session)
        else:
            session["status"] = "step-complete"

        stdout_text = aggregated.get("stdout", "")
        stderr_text = aggregated.get("stderr", "")

        worker_stdout_delta, worker_stderr_delta = self._consume_worker_output_deltas(session)
        if worker_stdout_delta:
            if stdout_text:
                stdout_text += "\n\n"
            stdout_text += "=== WORKER PROCESS STDOUT ===\n" + worker_stdout_delta
        if worker_stderr_delta:
            if stderr_text:
                stderr_text += "\n\n"
            stderr_text += "=== WORKER PROCESS STDERR ===\n" + worker_stderr_delta

        if not has_next:
            finalize_stdout = str(session.get("finalize_stdout") or "").strip()
            finalize_stderr = str(session.get("finalize_stderr") or "").strip()
            if finalize_stdout:
                if stdout_text:
                    stdout_text += "\n\n"
                stdout_text += "=== FINALIZATION ===\n" + finalize_stdout
            if finalize_stderr:
                if stderr_text:
                    stderr_text += "\n\n"
                stderr_text += "=== FINALIZATION STDERR ===\n" + finalize_stderr

        self._append_step_log_locked(
            session, stdout_text, stderr_text, action="advance", has_step=has_step
        )
        payload = self._build_step_payload_locked(session, stdout_text, stderr_text, has_next)
        self._last_result = dict(payload)
        self._status = payload.get("status", "complete")
        return payload

    def _run_single_rank_python(
        self,
        setup_state: dict[str, Any],
        cwd: str | pathlib.Path,
        dev_enabled: bool,
        mod: Any,
    ) -> dict[str, Any]:
        """Execute a single-rank run directly in-process with captured output."""
        _ = cwd
        run_workspace = self.create_run_workspace(setup_state)

        # Translate the staged CLI args embedded in the command list into typed
        # RunOptions, avoiding any re-parsing of setup_state text.
        run_options = self._run_options_from_command(mod, run_workspace["command"])

        # Set PLUME_PLUGIN_DEV so the C++ core and any loaded plugins see it.
        _prev_dev = os.environ.get("PLUME_PLUGIN_DEV")
        os.environ["PLUME_PLUGIN_DEV"] = "true" if dev_enabled else "false"
        try:
            result, captured_stdout, captured_stderr = self._call_with_captured_fds(mod.execute, run_options)
        finally:
            if _prev_dev is None:
                os.environ.pop("PLUME_PLUGIN_DEV", None)
            else:
                os.environ["PLUME_PLUGIN_DEV"] = _prev_dev

        stem = self.emulator_path.stem
        prec = stem[-2:]
        module_name = f"nwp_emulator_core_{prec}"

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

        # Full mode now persists per-step field snapshots so map browsing can
        # reuse the same /api/run/map/step/{n} path as step mode.
        field_keys = self._capture_single_rank_full_run_field_snapshots(
            run_workspace["run_dir"],
            run_options,
            mod,
            dev_enabled,
        )
        if field_keys:
            run_log_text += "\n\n"
            run_log_text += f"Field snapshots written for keys: {', '.join(field_keys)}"

        # Archive full-resolution field data for post-run analysis
        self._archive_full_resolution_fields(run_workspace["run_dir"])

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
            "mpi_np": 1,
        }

    def _run_multirank_from_setup_state_python(
        self,
        setup_state: dict[str, Any],
        cwd: str | pathlib.Path,
        dev_enabled: bool,
        mpi_np: int,
        mod: Any,
    ) -> dict[str, Any]:
        """Orchestrate multi-rank MPI execution via worker processes.

        1. Create run workspace and stage run options to JSON.
        2. Invoke mpirun with the worker entrypoint.
        3. Collect per-rank results from JSON files.
        4. Aggregate outcomes and write unified run.log.
        5. Return aggregated payload with deterministic return code.
        """
        run_workspace = self.create_run_workspace(setup_state)
        run_dir = run_workspace["run_dir"]

        # Stage run options to JSON for worker consumption.
        run_options = self._run_options_from_command(mod, run_workspace["command"])
        data_source_type_name = getattr(run_options.data_source_type, "name", None)
        if not isinstance(data_source_type_name, str) or not data_source_type_name:
            data_source_type_name = str(run_options.data_source_type)
        options_json = {
            "data_source_type": data_source_type_name,
            "data_source_path": run_options.data_source_path,
            "plume_config_path": run_options.plume_config_path,
            "module_name": getattr(mod, "__name__", ""),
        }
        options_file = run_dir / "run_options.json"
        options_file.write_text(json.dumps(options_json, indent=2), encoding="utf-8")

        # Build mpirun command for worker invocation.
        # Use an absolute script path instead of `-m backend...` because worker
        # processes may run from a cwd where `backend` is not importable.
        worker_script = pathlib.Path(__file__).resolve().parent / "emulator_worker.py"
        module_search_dirs = [
            *self.module_search_dirs,
            pathlib.Path(cwd).expanduser().resolve() / "plume" / "bin",
            pathlib.Path(cwd).expanduser().resolve() / "bin",
            self.emulator_path.parent,
        ]
        module_search_dir_args: list[str] = []
        for path in module_search_dirs:
            path_str = str(path)
            if path.exists() and path_str not in module_search_dir_args:
                module_search_dir_args.append(path_str)

        worker_cmd = [
            "mpirun",
            "-np", str(mpi_np),
            # Propagate PLUME_PLUGIN_DEV to all MPI ranks (OpenMPI -x flag).
            "-x", "PLUME_PLUGIN_DEV",
            "python", str(worker_script),
            str(run_dir),
            "--module-search-dirs",
            *module_search_dir_args,
        ]

        # Set the env var in the launcher process so mpirun inherits and forwards it.
        _prev_dev = os.environ.get("PLUME_PLUGIN_DEV")
        os.environ["PLUME_PLUGIN_DEV"] = "true" if dev_enabled else "false"
        try:
            # Invoke workers via mpirun.
            cwd_path = pathlib.Path(cwd).expanduser().resolve()
            completed = subprocess.run(
                worker_cmd,
                cwd=str(cwd_path),
                check=False,
                capture_output=True,
                text=True,
            )
        finally:
            if _prev_dev is None:
                os.environ.pop("PLUME_PLUGIN_DEV", None)
            else:
                os.environ["PLUME_PLUGIN_DEV"] = _prev_dev

        # Aggregate per-rank results and build unified log.
        aggregated = self._aggregate_multirank_results(
            run_dir, mpi_np, completed.stdout, completed.stderr
        )

        # Build aggregated per-step map snapshots from rank-local step files.
        try:
            final_step = int(aggregated.get("last_step_run") or 0)
        except (TypeError, ValueError):
            final_step = 0
        for step in range(1, max(0, final_step) + 1):
            self._aggregate_step_rank_field_snapshots(run_dir, mpi_np, step)

        # Archive full-resolution field data for post-run analysis
        self._archive_full_resolution_fields(run_dir)

        return {
            "command": worker_cmd,
            "dev": bool(dev_enabled),
            "return_code": aggregated["return_code"],
            "stdout_tail": tail_text(aggregated["stdout"]),
            "stderr_tail": tail_text(aggregated["stderr"]),
            "run_id": run_workspace["run_id"],
            "run_dir": str(run_dir),
            "run_log": str(run_workspace["run_log_path"]),
            "plume_run": aggregated.get("plume_run", ""),
            "last_step_run": aggregated.get("last_step_run", ""),
            "mpi_np": int(mpi_np),
        }

    def _capture_single_rank_full_run_field_snapshots(
        self,
        run_dir: pathlib.Path,
        run_options: Any,
        mod: Any,
        dev_enabled: bool,
    ) -> list[str]:
        """Generate full-run step snapshots for single-rank executions.

        The normal full-mode path uses mod.execute() which may not persist
        step-wise overlay snapshots. This helper replays the run with
        NWPEmulatorCore and writes map_fields/step_{n}.json for UI browsing.
        """
        core_ctor = getattr(mod, "NWPEmulatorCore", None)
        if core_ctor is None:
            return []

        # If snapshots already exist, preserve them and avoid duplicate replay.
        field_dir = run_dir / "map_fields"
        if field_dir.exists() and any(field_dir.glob("step_*.json")):
            return []

        # Build a dry replay options object (no plume side-effects) for map data.
        replay_opts = mod.RunOptions()
        replay_opts.data_source_type = run_options.data_source_type
        replay_opts.data_source_path = run_options.data_source_path
        replay_opts.plume_config_path = ""

        core = core_ctor()
        latest_field_keys: list[str] = []

        def _replay_and_capture() -> None:
            if not bool(core.validate_run_options(replay_opts)):
                return
            if not bool(core.setup_data_provider(replay_opts)):
                return

            while True:
                has_step = bool(core.run_step())
                if not has_step:
                    break
                step = int(core.current_step())
                _, keys = self._capture_single_rank_step_field_snapshot(run_dir, core, step)
                if keys:
                    latest_field_keys[:] = keys

            if hasattr(core, "finalize_run"):
                core.finalize_run()
            elif hasattr(core, "finalize_plume"):
                core.finalize_plume()

        try:
            self._with_plugin_dev_capture(_replay_and_capture, bool(dev_enabled))
        except (AttributeError, RuntimeError, TypeError, ValueError):
            return []

        return latest_field_keys

    def _aggregate_multirank_results(
        self,
        run_dir: pathlib.Path,
        mpi_np: int,
        mpirun_stdout: str,
        mpirun_stderr: str,
    ) -> dict[str, Any]:
        """Collect per-rank JSON results and aggregate into unified outcome.

        Returns:
            dict with keys: return_code, stdout, stderr, plume_run, last_step_run
        """
        rank_results: dict[int, dict[str, Any]] = {}
        rank_files = sorted(
            path for path in run_dir.glob("rank_*.json")
            if re.fullmatch(r"rank_\d+\.json", path.name)
        )

        for rank_file in rank_files:
            try:
                with rank_file.open("r", encoding="utf-8") as f:
                    rank_data = json.load(f)
                    rank = int(rank_data.get("rank", -1))
                    rank_results[rank] = rank_data
            except (OSError, json.JSONDecodeError, KeyError, ValueError, TypeError) as e:
                # Log failure but continue aggregation.
                print(f"Failed to load rank result from {rank_file}: {e}", file=sys.stderr)

        # Aggregate return codes: 0 if all succeed, else non-zero.
        aggregated_return_code = 0
        for rank in range(mpi_np):
            if rank not in rank_results:
                aggregated_return_code = 1
                break
            if rank_results[rank].get("return_code", 1) != 0:
                aggregated_return_code = 1

        # Merge stdout/stderr from all ranks.
        all_stdout_lines = [mpirun_stdout] if mpirun_stdout else []
        all_stderr_lines = [mpirun_stderr] if mpirun_stderr else []
        plume_run = None
        last_step_run = None

        for rank in range(mpi_np):
            data = rank_results.get(rank, {})
            all_stdout_lines.append(
                f"Rank {rank}: return_code={data.get('return_code', 'missing')}, "
                f"status={data.get('status', 'missing')}"
            )
            if rank not in rank_results:
                all_stderr_lines.append(f"Missing rank result file for rank {rank}")
                continue
            if data.get("stdout"):
                all_stdout_lines.append(f"\n--- Rank {rank} stdout ---\n{data['stdout']}")
            if data.get("stderr"):
                all_stderr_lines.append(f"\n--- Rank {rank} stderr ---\n{data['stderr']}")
            # Extract plume_run/last_step_run from rank 0 (arbitrary choice).
            if rank == 0:
                plume_run = data.get("plume_run")
                last_step_run = data.get("last_step_run")

        # Write unified run.log.
        log_lines = [
            f"Multi-rank execution (mpi_np={mpi_np})",
            f"Aggregated return code: {aggregated_return_code}",
            "",
            "Per-rank results:\n",
        ]
        for rank in sorted(rank_results.keys()):
            data = rank_results[rank]
            log_lines.append(
                f"  Rank {rank}: return_code={data.get('return_code')}, "
                f"status={data.get('status')}"
            )
        log_lines.extend([
            "",
            "--- Merged stdout ---",
            "\n".join(all_stdout_lines),
            "",
            "--- Merged stderr ---",
            "\n".join(all_stderr_lines),
        ])
        run_log_path = run_dir / "run.log"
        run_log_path.write_text("\n".join(log_lines), encoding="utf-8")

        return {
            "return_code": aggregated_return_code,
            "stdout": "\n".join(all_stdout_lines),
            "stderr": "\n".join(all_stderr_lines),
            "plume_run": plume_run or "",
            "last_step_run": last_step_run or "",
        }

    def _import_bound_module(self, module_name: str, cwd: str | pathlib.Path) -> Any:
        """Import the pybind extension from known build/UI locations.

        The launcher may execute from a build tree while the Python source files
        are symlinked back to the source checkout. Therefore ``__file__`` for
        this service is not a reliable locator for the built extension module.
        Search explicit runtime paths first, then derive likely build-tree paths
        from the working directory and emulator binary location.
        """
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
