"""Validation for setup options and cross-field consistency."""

import os

from ..domain.enums import RUN_MODES
from ..domain.errors import SetupValidationError


def validate_options_payload(payload):
    dry_run = payload.get("dry_run", False)
    run_mode = payload.get("run_mode", "config")
    mpi_np = payload.get("mpi_np", 3)

    if not isinstance(dry_run, bool):
        raise SetupValidationError("dry_run must be a boolean")
    if run_mode not in RUN_MODES:
        raise SetupValidationError("run_mode must be one of: config, grib")

    try:
        mpi_np = int(mpi_np)
    except (TypeError, ValueError) as exc:
        raise SetupValidationError("mpi_np must be an integer") from exc

    if mpi_np < 1:
        raise SetupValidationError("mpi_np must be >= 1")

    return dry_run, run_mode, mpi_np


def validate_mpi_np(mpi_np):
    """
    Validate mpi_np option for MPI process count.
    
    Returns:
        (ok: bool, messages: list[str])
    """
    messages = []

    # Check type and bounds
    try:
        mpi_np_int = int(mpi_np)
    except (TypeError, ValueError):
        return False, ["mpi_np must be an integer"]

    if mpi_np_int < 1:
        return False, ["mpi_np must be at least 1"]

    # Hard cap at 256 processes
    if mpi_np_int > 256:
        return False, ["mpi_np cannot exceed 256 processes"]

    # Warn if exceeds available logical cores
    logical_cores = os.cpu_count() or 1
    if mpi_np_int > logical_cores:
        messages.append(
            f"mpi_np ({mpi_np_int}) exceeds available logical cores ({logical_cores}); "
            "oversubscription may impact performance"
        )

    return (not bool(messages)), messages
