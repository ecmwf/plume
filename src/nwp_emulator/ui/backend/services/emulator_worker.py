#!/usr/bin/env python3
"""Worker entrypoint for multi-rank MPI execution of the NWP emulator.

This module is invoked as a worker process when the launcher orchestrates
multi-rank execution via: `mpirun -np N python -m nwp_emulator.worker <run_dir>`

Each worker process:
1. Determines its MPI rank and total number of ranks from the MPI context.
2. Loads the staged RunOptions from run_dir/run_options.json.
3. Imports and executes the binding module (nwp_emulator_core_{sp|dp}).
4. Emits per-rank result as JSON to run_dir/rank_{rank}.json.
5. Exits with aggregated return code (handled by launcher).

Step mode (--step-mode flag):
Each worker uses NWPEmulatorCore for atomic step execution and a file-based
IPC protocol with the launcher:
- rank_{rank}_ready.json  : written after setup, signals launcher readiness.
- step_advance_{N}        : created by launcher to trigger step N execution.
- rank_{rank}_step_{N}_done.json : written after completing step N.
- step_abort              : created by launcher to request clean exit.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import sys
import time
import traceback
from io import StringIO
from typing import Any

_STEP_POLL_INTERVAL = 0.05  # 50 ms between file-existence polls in step mode


def _get_mpi_context() -> tuple[int, int]:
    """Determine MPI rank and size from the current MPI context.

    Returns:
        (rank, nprocs) where rank is this process's rank and nprocs is total.
    """
    try:
        from mpi4py import MPI  # type: ignore[import-not-found]
        comm = MPI.COMM_WORLD
        return comm.Get_rank(), comm.Get_size()
    except ImportError:
        # Fallback to MPI launcher environment variables when mpi4py is absent.
        # OpenMPI/PRRTE uses OMPI_COMM_WORLD_{RANK,SIZE}; other launchers may use
        # PMI_RANK/PMI_SIZE or PMIX_RANK.
        rank_candidates = (
            os.getenv("OMPI_COMM_WORLD_RANK"),
            os.getenv("PMI_RANK"),
            os.getenv("PMIX_RANK"),
        )
        size_candidates = (
            os.getenv("OMPI_COMM_WORLD_SIZE"),
            os.getenv("PMI_SIZE"),
            os.getenv("PMIX_SIZE"),
        )

        def _first_int(values: tuple[str | None, ...], default: int) -> int:
            for value in values:
                if value is None:
                    continue
                try:
                    return int(value)
                except ValueError:
                    continue
            return default

        rank = _first_int(rank_candidates, 0)
        nprocs = _first_int(size_candidates, 1)
        if nprocs < 1:
            nprocs = 1
        if rank < 0:
            rank = 0
        return rank, nprocs


def _load_run_options(run_dir: pathlib.Path) -> dict[str, Any]:
    """Load staged RunOptions from run_dir/run_options.json."""
    options_file = run_dir / "run_options.json"
    if not options_file.exists():
        raise FileNotFoundError(f"RunOptions not found at {options_file}")
    with options_file.open("r", encoding="utf-8") as f:
        return json.load(f)


def _import_and_execute(
    run_options: dict[str, Any],
    module_search_dirs: list[pathlib.Path],
) -> tuple[int, str, str, Any, Any]:
    """Import binding module and execute with captured output.

    Args:
        run_options: Typed execution options to pass to mod.execute().
        module_search_dirs: Fallback search paths for the binding module.

    Returns:
        (return_code, stdout_text, stderr_text) from execution.
    """
    module_name = str(run_options.get("module_name", "")).strip()
    if not module_name:
        # Conservative default for older staged files.
        module_name = "nwp_emulator_core_dp"

    # Add search dirs to sys.path for module discovery.
    original_path = sys.path.copy()
    for search_dir in module_search_dirs:
        if str(search_dir) not in sys.path:
            sys.path.insert(0, str(search_dir))

    try:
        mod = __import__(module_name)
    except ImportError as exc:
        raise ImportError(
            f"Failed to import {module_name}. "
            f"Searched: {module_search_dirs}. "
            f"Error: {exc}"
        ) from exc
    finally:
        sys.path = original_path

    # Build typed RunOptions from dict.
    # The structure mirrors what EmulatorRuntime._run_options_from_command builds.
    run_options_obj = mod.RunOptions()
    data_source_type_name = str(run_options.get("data_source_type", "CONFIG"))
    data_source_type = getattr(mod.DataSourceType, data_source_type_name, None)
    if data_source_type is None:
        data_source_type = getattr(mod.DataSourceType, "INVALID", mod.DataSourceType.CONFIG)
    run_options_obj.data_source_type = data_source_type
    run_options_obj.data_source_path = run_options.get("data_source_path", "")
    run_options_obj.plume_config_path = run_options.get("plume_config_path", "")

    # Capture stdout/stderr during execution.
    old_stdout = sys.stdout
    old_stderr = sys.stderr
    captured_stdout = StringIO()
    captured_stderr = StringIO()

    try:
        sys.stdout = captured_stdout
        sys.stderr = captured_stderr
        result = mod.execute(run_options_obj)
        return_code = result.return_code
        stdout_text = captured_stdout.getvalue()
        stderr_text = captured_stderr.getvalue()
        plume_run = getattr(result, "plume_run", None)
        last_step_run = getattr(result, "last_step_run", None)
        return return_code, stdout_text, stderr_text, plume_run, last_step_run
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr


def _write_json(path: pathlib.Path, data: dict[str, Any]) -> None:
    """Write data as JSON to path atomically via a temp-then-rename pattern."""
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, indent=2), encoding="utf-8")
    tmp.rename(path)


def _run_step_mode(
    run_dir: pathlib.Path,
    run_options_dict: dict[str, Any],
    module_search_dirs: list[pathlib.Path],
) -> int:
    """Execute one NWP emulator session in step-by-step mode.

    File-based IPC protocol with the launcher process:
    - Write rank_{rank}_ready.json after provider setup completes.
    - Poll for step_advance_{N} before executing step N.
    - Write rank_{rank}_step_{N}_done.json after each step.
    - Exit cleanly when step_abort appears or run_step() returns False.
    """
    rank, _ = _get_mpi_context()

    module_name = str(run_options_dict.get("module_name", "")).strip() or "nwp_emulator_core_dp"
    original_path = sys.path.copy()
    for search_dir in module_search_dirs:
        if str(search_dir) not in sys.path:
            sys.path.insert(0, str(search_dir))
    try:
        mod = __import__(module_name)
    except ImportError as exc:
        _write_json(run_dir / f"rank_{rank}_ready.json", {
            "rank": rank,
            "status": "failed",
            "error": f"ImportError: {exc}",
            "stdout": "",
            "stderr": str(traceback.format_exc()),
        })
        return 1
    finally:
        sys.path = original_path

    # Build typed RunOptions.
    run_options_obj = mod.RunOptions()
    dst = str(run_options_dict.get("data_source_type", "CONFIG"))
    data_source_type = getattr(mod.DataSourceType, dst, None)
    if data_source_type is None:
        data_source_type = getattr(mod.DataSourceType, "INVALID", mod.DataSourceType.CONFIG)
    run_options_obj.data_source_type = data_source_type
    run_options_obj.data_source_path = run_options_dict.get("data_source_path", "")
    run_options_obj.plume_config_path = run_options_dict.get("plume_config_path", "")

    # Provider setup — capture all output.
    captured_stdout = StringIO()
    captured_stderr = StringIO()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = captured_stdout
    sys.stderr = captured_stderr
    plume_run = False
    try:
        # Log setup progress (will be captured by StringIO).
        print(f"[Rank {rank}] Step-mode setup starting...")
        print(f"[Rank {rank}] Initializing NWPEmulatorCore...")
        core = mod.NWPEmulatorCore()
        
        print(f"[Rank {rank}] Validating run options...")
        if not bool(core.validate_run_options(run_options_obj)):
            sys.stdout, sys.stderr = old_stdout, old_stderr
            _write_json(run_dir / f"rank_{rank}_ready.json", {
                "rank": rank, "status": "failed",
                "error": "validate_run_options returned False",
                "stdout": captured_stdout.getvalue(),
                "stderr": captured_stderr.getvalue(),
            })
            return 1
        
        print(f"[Rank {rank}] Setting up data provider...")
        if not bool(core.setup_data_provider(run_options_obj)):
            sys.stdout, sys.stderr = old_stdout, old_stderr
            _write_json(run_dir / f"rank_{rank}_ready.json", {
                "rank": rank, "status": "failed",
                "error": "setup_data_provider returned False",
                "stdout": captured_stdout.getvalue(),
                "stderr": captured_stderr.getvalue(),
            })
            return 1
        
        print(f"[Rank {rank}] Data provider setup completed.")
        if run_options_obj.plume_config_path:
            print(f"[Rank {rank}] Setting up plume provider...")
            plume_run = bool(core.setup_plume_provider())
            print(f"[Rank {rank}] Plume provider setup completed: {plume_run}")
        
        print(f"[Rank {rank}] Step-mode setup completed successfully.")
    except Exception as exc:  # noqa: BLE001
        sys.stdout, sys.stderr = old_stdout, old_stderr
        _write_json(run_dir / f"rank_{rank}_ready.json", {
            "rank": rank, "status": "failed",
            "error": str(exc),
            "stdout": captured_stdout.getvalue(),
            "stderr": captured_stderr.getvalue(),
        })
        return 1
    finally:
        sys.stdout, sys.stderr = old_stdout, old_stderr

    # Signal readiness to launcher.
    _write_json(run_dir / f"rank_{rank}_ready.json", {
        "rank": rank,
        "status": "ready",
        "stdout": captured_stdout.getvalue(),
        "stderr": captured_stderr.getvalue(),
    })

    step = 0
    return_code = 0
    finalized = False
    while True:
        next_step = step + 1
        advance_file = run_dir / f"step_advance_{next_step}"
        abort_file = run_dir / "step_abort"
        finalize_file = run_dir / "step_finalize"

        # Wait for the launcher to signal this step.
        while not advance_file.exists() and not abort_file.exists() and not finalize_file.exists():
            time.sleep(_STEP_POLL_INTERVAL)

        if abort_file.exists():
            break

        if finalize_file.exists():
            finalize_stdout = StringIO()
            finalize_stderr = StringIO()
            old_stdout, old_stderr = sys.stdout, sys.stderr
            sys.stdout = finalize_stdout
            sys.stderr = finalize_stderr
            finalize_error: str | None = None
            try:
                if hasattr(core, "finalize_run"):
                    core.finalize_run()
                else:
                    core.finalize_plume()
                finalized = True
            except Exception as exc:  # noqa: BLE001
                finalize_error = str(exc)
                return_code = 1
            finally:
                sys.stdout, sys.stderr = old_stdout, old_stderr

            _write_json(run_dir / f"rank_{rank}_finalized.json", {
                "rank": rank,
                "status": "ok" if finalize_error is None else "failed",
                "return_code": 0 if finalize_error is None else 1,
                "error": finalize_error,
                "stdout": finalize_stdout.getvalue(),
                "stderr": finalize_stderr.getvalue(),
            })
            break

        # Execute one step with captured output.
        step_stdout = StringIO()
        step_stderr = StringIO()
        old_stdout, old_stderr = sys.stdout, sys.stderr
        sys.stdout = step_stdout
        sys.stderr = step_stderr
        has_step = False
        try:
            has_step = bool(core.run_step())
            if has_step:
                step = int(core.current_step())
        except Exception as exc:  # noqa: BLE001
            sys.stdout, sys.stderr = old_stdout, old_stderr
            return_code = 1
            _write_json(run_dir / f"rank_{rank}_step_{next_step}_done.json", {
                "rank": rank,
                "step": next_step,
                "has_step": False,
                "return_code": 1,
                "status": "failed",
                "error": str(exc),
                "stdout": step_stdout.getvalue(),
                "stderr": step_stderr.getvalue(),
                "plume_run": plume_run,
                "last_step_run": step,
            })
            break
        finally:
            sys.stdout, sys.stderr = old_stdout, old_stderr

        _write_json(run_dir / f"rank_{rank}_step_{next_step}_done.json", {
            "rank": rank,
            "step": step if has_step else next_step,
            "has_step": has_step,
            "return_code": 0,
            "status": "ok",
            "error": None,
            "stdout": step_stdout.getvalue(),
            "stderr": step_stderr.getvalue(),
            "plume_run": plume_run,
            "last_step_run": step,
        })

        if not has_step:
            break

    if not finalized:
        try:
            core.finalize_plume()
        except (AttributeError, RuntimeError, TypeError, ValueError):
            pass

    return return_code


def main() -> int:
    """Main worker entrypoint.

    Parses run_dir from argv, loads setup, executes binding, emits result JSON.
    Returns aggregated exit code (0 for success, 1+ for failure).
    """
    parser = argparse.ArgumentParser(description="NWP Emulator MPI Worker Process")
    parser.add_argument("run_dir", help="Path to staged run workspace directory")
    parser.add_argument("--emulator-bin", default=None, help="Path to emulator binary (for precision inference)")
    parser.add_argument("--module-search-dirs", nargs="*", default=[], help="Additional module search paths")
    parser.add_argument("--step-mode", action="store_true", help="Run in step-by-step mode (file-based IPC with launcher)")

    args = parser.parse_args()
    run_dir = pathlib.Path(args.run_dir).expanduser().resolve()

    # Get MPI context.
    rank, nprocs = _get_mpi_context()

    # Load staged options.
    try:
        run_options = _load_run_options(run_dir)
    except (OSError, json.JSONDecodeError, ValueError, TypeError) as exc:
        result_file = run_dir / f"rank_{rank}.json"
        result_file.write_text(json.dumps({
            "rank": rank,
            "nprocs": nprocs,
            "return_code": 1,
            "status": "failed",
            "error": f"Failed to load run options: {exc}",
            "stdout": "",
            "stderr": str(traceback.format_exc()),
        }, indent=2), encoding="utf-8")
        return 1

    module_search_dirs = [pathlib.Path(p) for p in args.module_search_dirs]

    # Route to step mode when requested.
    if args.step_mode:
        return _run_step_mode(run_dir, run_options, module_search_dirs)

    # Full-run mode: import and execute binding.
    try:
        return_code, stdout_text, stderr_text, plume_run, last_step_run = _import_and_execute(run_options, module_search_dirs)
    except (ImportError, AttributeError, KeyError, OSError, ValueError, TypeError) as exc:
        result_file = run_dir / f"rank_{rank}.json"
        result_file.write_text(json.dumps({
            "rank": rank,
            "nprocs": nprocs,
            "return_code": 1,
            "status": "failed",
            "error": str(exc),
            "stdout": "",
            "stderr": str(traceback.format_exc()),
        }, indent=2), encoding="utf-8")
        return 1

    # Emit success result.
    result_file = run_dir / f"rank_{rank}.json"
    result_file.write_text(json.dumps({
        "rank": rank,
        "nprocs": nprocs,
        "return_code": return_code,
        "status": "complete" if return_code == 0 else "failed",
        "error": None,
        "stdout": stdout_text,
        "stderr": stderr_text,
        "plume_run": plume_run,
        "last_step_run": last_step_run,
    }, indent=2), encoding="utf-8")

    return return_code


if __name__ == "__main__":
    sys.exit(main())
