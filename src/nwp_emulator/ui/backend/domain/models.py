"""Dataclasses representing setup tab backend state."""

from dataclasses import dataclass, field
from typing import Dict, List

from .enums import CHECK_STATUS_IDLE, RUN_MODE_CONFIG


@dataclass
class SetupOptions:
    dry_run: bool = False
    run_mode: str = RUN_MODE_CONFIG
    mpi_np: int = 3


@dataclass
class SourceState:
    path_display: str = ""
    text: str = ""
    valid: bool = True
    messages: List[str] = field(default_factory=list)


@dataclass
class GribSourceState:
    source_type: str = ""
    path_display: str = ""
    selected_paths: List[str] = field(default_factory=list)
    valid: bool = True
    messages: List[str] = field(default_factory=list)
    metadata: Dict = field(default_factory=dict)


@dataclass
class ChecksState:
    status: str = CHECK_STATUS_IDLE
    messages: List[str] = field(default_factory=list)
    results: Dict[str, str] = field(default_factory=dict)
    result_messages: Dict[str, List[str]] = field(default_factory=dict)


@dataclass
class SetupSession:
    options: SetupOptions = field(default_factory=SetupOptions)
    plume_config: SourceState = field(default_factory=SourceState)
    emulator_config: SourceState = field(default_factory=SourceState)
    grib_source: GribSourceState = field(default_factory=GribSourceState)
    checks: ChecksState = field(default_factory=ChecksState)
