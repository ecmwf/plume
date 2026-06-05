# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
"""MPI integration tests for pynwp_emulator.

These tests are designed to run under an MPI launcher::

    mpirun -n 3 python -m pytest test_nwp_emulator_mpi.py

MPI launches one independent pytest process per rank. Because all ranks
execute the same test sequence, they reach the MPI collective operations
(barriers) inside ``run_step()`` and ``finalize_run()`` simultaneously —
which is what the C++ core requires. Tests that carry rank-specific
assertions use the ``rank`` and ``nprocs`` fields returned by
:meth:`~pynwp_emulator.NWPEmulator.get_field_overlay_snapshot` rather
than a direct MPI dependency on the Python side.

All tests are skipped when ``TEST_DATA_DIR`` is not set.
"""

import os

import pytest
import yaml

import pynwp_emulator as emu

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

TEST_DATA_DIR = os.environ.get("TEST_DATA_DIR", "")
MPI_NPROCS = int(os.environ.get("MPI_NPROCS", "3"))

needs_data = pytest.mark.skipif(
    not TEST_DATA_DIR,
    reason="TEST_DATA_DIR not set — skipping MPI integration tests",
)


def _valid_config() -> dict:
    """Return the parsed valid_config.yml as a dict."""
    config_path = os.path.join(TEST_DATA_DIR, "valid_config.yml")
    with open(config_path) as f:
        return yaml.safe_load(f)


def _config_opts() -> emu.RunOptions:
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.CONFIG
    opts.data_source_path = os.path.join(TEST_DATA_DIR, "valid_config.yml")
    return opts


# ---------------------------------------------------------------------------
# execute() under MPI
# ---------------------------------------------------------------------------


@needs_data
def test_mpi_execute_succeeds():
    """execute() must succeed on all ranks."""
    result = emu.NWPEmulator().execute(_config_opts())
    assert result.return_code == 0


@needs_data
def test_mpi_execute_result_fields():
    """execute() dry-run result must be consistent on all ranks."""
    result = emu.NWPEmulator().execute(_config_opts())
    assert result.plume_run is False
    assert result.last_step_run >= 0


# ---------------------------------------------------------------------------
# MPI topology reflected in FieldOverlaySnapshot
# ---------------------------------------------------------------------------


@needs_data
def test_mpi_snapshot_nprocs():
    """snapshot.nprocs must equal the number of MPI ranks."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert snapshot.nprocs == MPI_NPROCS


@needs_data
def test_mpi_snapshot_rank_in_range():
    """snapshot.rank must be a valid rank index for this communicator."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert 0 <= snapshot.rank < snapshot.nprocs


@needs_data
def test_mpi_snapshot_root_is_zero():
    """Root rank must always be 0."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert snapshot.root == 0


# ---------------------------------------------------------------------------
# Rank-local data partitioning
# ---------------------------------------------------------------------------


@needs_data
def test_mpi_snapshot_local_arrays_non_empty():
    """Each rank must hold a non-empty local partition of the global grid."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert len(snapshot.lon) > 0
    assert len(snapshot.lat) > 0
    assert len(snapshot.values) > 0


@needs_data
def test_mpi_snapshot_array_lengths_consistent():
    """lon, lat and values within a single rank's snapshot must have equal length.

    Each grid point held by this rank must have a longitude, a latitude, and a
    value — so the three arrays are always co-sized regardless of how Atlas
    partitions the global grid across ranks.
    """
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert len(snapshot.lon) == len(snapshot.lat) == len(snapshot.values)


@needs_data
def test_mpi_snapshot_local_size_within_global_grid():
    """Each rank's local partition must not exceed the total number of grid points.

    When nprocs > 1 it must also be strictly smaller, since Atlas distributes
    the grid across all ranks.

    The total grid size is obtained via atlas4py so the assertion is exact
    rather than heuristic. The test is skipped when atlas4py is not installed.
    """
    atlas4py = pytest.importorskip("atlas4py")

    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    total_points = atlas4py.Grid(_valid_config()["emulator"]["grid_identifier"]).size
    local_size = len(snapshot.values)

    assert local_size <= total_points
    if snapshot.nprocs > 1:
        assert local_size < total_points


# ---------------------------------------------------------------------------
# Context manager + iterator under MPI
# ---------------------------------------------------------------------------


@needs_data
def test_mpi_context_manager_iterator_all_ranks():
    """All ranks must complete the full step loop via the context manager."""
    steps_seen = []
    with emu.NWPEmulator(_config_opts()) as emulator:
        for step in emulator:
            steps_seen.append(step)

    assert len(steps_seen) == _valid_config()["emulator"]["n_steps"]
    assert len(steps_seen) > 0
    assert all(isinstance(s, int) for s in steps_seen)


@needs_data
def test_mpi_context_manager_steps_increase():
    """Step numbers must be strictly increasing on every rank."""
    steps_seen = []
    with emu.NWPEmulator(_config_opts()) as emulator:
        for step in emulator:
            steps_seen.append(step)

    assert steps_seen == sorted(set(steps_seen))


@needs_data
def test_mpi_snapshot_step_matches_current_step():
    """snapshot.step must equal current_step() on every rank."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())
    emulator.run_step()

    key = emulator.available_field_keys()[0]
    snapshot = emulator.get_field_overlay_snapshot(key)

    assert snapshot.step == emulator.current_step()
