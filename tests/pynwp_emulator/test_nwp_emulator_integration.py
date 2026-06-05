# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
"""Integration tests for pynwp_emulator using the CONFIG data source.

These tests exercise actual emulator execution paths against the synthetic
CONFIG data source (``valid_config.yml``) that ships with the test suite.
No GRIB files or MPI environment are required: the CONFIG reader generates
data from mathematical functions in a single process.

All tests are skipped automatically when ``TEST_DATA_DIR`` is not set in the
environment, so the test suite remains runnable in environments that have not
been configured for integration testing.
"""

import os
import subprocess
import sys
import textwrap
import pytest
import yaml

import pynwp_emulator as emu

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

TEST_DATA_DIR = os.environ.get("TEST_DATA_DIR", "")
PLUME_CONFIG_PATH = (
    os.path.join(TEST_DATA_DIR, "plume_config_simple.yml") if TEST_DATA_DIR else ""
)


def _valid_config() -> dict:
    """Return the parsed valid_config.yml as a dict."""
    with open(os.path.join(TEST_DATA_DIR, "valid_config.yml")) as f:
        return yaml.safe_load(f)


def _config_opts(plume_config_path: str = "") -> emu.RunOptions:
    """Return RunOptions pointing at the synthetic CONFIG data source."""
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.CONFIG
    opts.data_source_path = os.path.join(TEST_DATA_DIR, "valid_config.yml")
    opts.plume_config_path = plume_config_path
    return opts


needs_data = pytest.mark.skipif(
    not TEST_DATA_DIR,
    reason="TEST_DATA_DIR not set — skipping integration tests",
)

needs_plume = pytest.mark.skipif(
    not (TEST_DATA_DIR and PLUME_CONFIG_PATH),
    reason="TEST_DATA_DIR or PLUME_CONFIG_PATH not set — skipping Plume integration tests",
)


# ---------------------------------------------------------------------------
# Module-scoped fixtures — expensive state shared across tests
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def valid_cfg():
    """Parsed valid_config.yml, loaded once per module."""
    if not TEST_DATA_DIR:
        pytest.skip("TEST_DATA_DIR not set")
    return _valid_config()


@pytest.fixture(scope="module")
def dry_run_result():
    """execute() result from a single dry run, shared across execute tests."""
    if not TEST_DATA_DIR:
        pytest.skip("TEST_DATA_DIR not set")
    return emu.NWPEmulator().execute(_config_opts())


@pytest.fixture(scope="module")
def emulator_with_keys():
    """Emulator after setup_data_provider only, shared across field-key tests."""
    if not TEST_DATA_DIR:
        pytest.skip("TEST_DATA_DIR not set")
    e = emu.NWPEmulator()
    e.setup_data_provider(_config_opts())
    yield e
    e.finalize_run()


@pytest.fixture(scope="module")
def snapshot_after_step():
    """(emulator, key, snapshot) after one step, shared across all snapshot tests."""
    if not TEST_DATA_DIR:
        pytest.skip("TEST_DATA_DIR not set")
    e = emu.NWPEmulator()
    e.setup_data_provider(_config_opts())
    e.run_step()
    key = e.available_field_keys()[0]
    snapshot = e.get_field_overlay_snapshot(key)
    yield e, key, snapshot
    e.finalize_run()


# ---------------------------------------------------------------------------
# validate_run_options with a real path
# ---------------------------------------------------------------------------


@needs_data
def test_validate_run_options_real_config_path():
    """validate_run_options must return True for the known-good CONFIG source."""
    assert emu.NWPEmulator().validate_run_options(_config_opts()) is True


# ---------------------------------------------------------------------------
# execute() single-shot
# ---------------------------------------------------------------------------


@needs_data
def test_execute_config_dry_run(dry_run_result, valid_cfg):
    """execute() dry run must succeed, report correct types and values."""
    result = dry_run_result
    assert isinstance(result, emu.RunResult)
    assert isinstance(result.return_code, int)
    assert isinstance(result.plume_run, bool)
    assert isinstance(result.last_step_run, int)
    assert result.return_code == 0
    assert result.plume_run is False
    assert result.last_step_run == valid_cfg["emulator"]["n_steps"]

    # A completed run must differ from a default-constructed RunResult
    # (last_step_run defaults to -1), and equal another result of the same run.
    assert result != emu.RunResult()
    assert result == emu.NWPEmulator().execute(_config_opts())


# ---------------------------------------------------------------------------
# Manual lifecycle (setup -> step loop -> finalize)
# ---------------------------------------------------------------------------


@needs_data
def test_manual_lifecycle_setup_and_step(valid_cfg):
    """Step-by-step lifecycle must complete without error."""
    emulator = emu.NWPEmulator()
    opts = _config_opts()

    assert emulator.setup_data_provider(opts) is True
    assert emulator.setup_plume_provider() is True

    steps_run = 0
    while emulator.run_step():
        steps_run += 1

    emulator.finalize_plume()
    emulator.finalize_run()

    assert steps_run == valid_cfg["emulator"]["n_steps"]


@needs_data
def test_available_field_keys_after_setup(emulator_with_keys, valid_cfg):
    """available_field_keys must cover exactly the fields declared in the config.

    Each returned key is ``shortName,levtype,level``; a field with multiple
    vertical levels will contribute several keys. The set of distinct short
    names must therefore equal the set of keys under ``emulator.fields`` in
    ``valid_config.yml``.
    """
    keys = emulator_with_keys.available_field_keys()
    config_field_names = set(valid_cfg["emulator"]["fields"].keys())
    returned_short_names = {k.split(",")[0] for k in keys}

    assert isinstance(keys, list)
    assert returned_short_names == config_field_names


@needs_data
def test_available_field_keys_format(emulator_with_keys):
    """Each field key must follow the 'shortName,levtype,level' format."""
    for key in emulator_with_keys.available_field_keys():
        parts = key.split(",")
        assert len(parts) == 3, f"Unexpected key format: {key!r}"


@needs_data
def test_current_step_advances():
    """current_step must increase after each run_step call."""
    emulator = emu.NWPEmulator()
    emulator.setup_data_provider(_config_opts())

    step_before = emulator.current_step()
    emulator.run_step()
    step_after = emulator.current_step()

    assert step_after > step_before


# ---------------------------------------------------------------------------
# get_field_overlay_snapshot
# ---------------------------------------------------------------------------


@needs_data
def test_get_field_overlay_snapshot_returns_correct_type(snapshot_after_step):
    """get_field_overlay_snapshot must return a FieldOverlaySnapshot."""
    _, _, snapshot = snapshot_after_step
    assert isinstance(snapshot, emu.FieldOverlaySnapshot)


@needs_data
def test_get_field_overlay_snapshot_field_key_matches(snapshot_after_step):
    """Snapshot field_key must match the requested key."""
    _, key, snapshot = snapshot_after_step
    assert snapshot.field_key == key


@needs_data
def test_get_field_overlay_snapshot_arrays_non_empty(snapshot_after_step):
    """lon, lat and values arrays must be non-empty after a step."""
    _, _, snapshot = snapshot_after_step
    assert len(snapshot.lon) > 0
    assert len(snapshot.lat) > 0
    assert len(snapshot.values) > 0


@needs_data
def test_get_field_overlay_snapshot_array_lengths_match(snapshot_after_step):
    """lon, lat and values must have equal lengths."""
    _, _, snapshot = snapshot_after_step
    assert len(snapshot.lon) == len(snapshot.lat) == len(snapshot.values)


@needs_data
def test_get_field_overlay_snapshot_mpi_metadata(snapshot_after_step):
    """Single-process run must report rank=0, root=0, nprocs=1."""
    _, _, snapshot = snapshot_after_step
    assert snapshot.rank == 0
    assert snapshot.root == 0
    assert snapshot.nprocs == 1


@needs_data
def test_get_field_overlay_snapshot_step_matches_current(snapshot_after_step):
    """Snapshot step must equal current_step() at time of capture."""
    emulator, _, snapshot = snapshot_after_step
    assert snapshot.step == emulator.current_step()


# ---------------------------------------------------------------------------
# Context manager + iterator full lifecycle
# ---------------------------------------------------------------------------


@needs_data
def test_context_manager_iterator_dry_run(valid_cfg):
    """Context manager with iterator must complete all steps in order."""
    steps_seen = []
    with emu.NWPEmulator(_config_opts()) as emulator:
        for step in emulator:
            steps_seen.append(step)

    assert len(steps_seen) == valid_cfg["emulator"]["n_steps"]
    assert all(isinstance(s, int) for s in steps_seen)
    assert steps_seen == sorted(set(steps_seen))


@needs_data
def test_context_manager_teardown_on_exception():
    """__exit__ must run teardown even when an exception escapes the iterator loop."""
    with pytest.raises(RuntimeError, match="early exit"):
        with emu.NWPEmulator(_config_opts()) as emulator:
            for _ in emulator:
                raise RuntimeError("early exit")


# ---------------------------------------------------------------------------
# Plume-enabled run (requires plugin libs on LD_LIBRARY_PATH)
# Each test runs in a fresh subprocess so the Plume Manager singleton is
# initialised independently, allowing both execute() and context-manager paths
# to be tested with a real Plume configuration.
# ---------------------------------------------------------------------------


def _run_plume_subprocess(script: str) -> subprocess.CompletedProcess:
    """Run *script* in a subprocess inheriting the current environment."""
    return subprocess.run(
        [sys.executable, "-c", textwrap.dedent(script)],
        capture_output=True,
        text=True,
        check=False,
    )


@needs_plume
def test_plume_execute_result(valid_cfg):
    """execute() with a Plume config must succeed and report a complete run."""
    n_steps = valid_cfg["emulator"]["n_steps"]
    proc = _run_plume_subprocess(f"""
        import pynwp_emulator as emu, os
        opts = emu.RunOptions()
        opts.data_source_type = emu.DataSourceType.CONFIG
        opts.data_source_path = os.path.join({TEST_DATA_DIR!r}, "valid_config.yml")
        opts.plume_config_path = {PLUME_CONFIG_PATH!r}
        result = emu.NWPEmulator().execute(opts)
        assert result.return_code == 0, f"return_code={{result.return_code}}"
        assert result.plume_run is True, "plume_run is not True"
        assert result.last_step_run == {n_steps}, f"last_step_run={{result.last_step_run}}"
    """)
    assert proc.returncode == 0, proc.stderr


@needs_plume
def test_plume_context_manager_iterator(valid_cfg):
    """Context manager with a Plume config must iterate over all steps."""
    n_steps = valid_cfg["emulator"]["n_steps"]
    proc = _run_plume_subprocess(f"""
        import pynwp_emulator as emu, os
        opts = emu.RunOptions()
        opts.data_source_type = emu.DataSourceType.CONFIG
        opts.data_source_path = os.path.join({TEST_DATA_DIR!r}, "valid_config.yml")
        opts.plume_config_path = {PLUME_CONFIG_PATH!r}
        steps_seen = []
        with emu.NWPEmulator(opts) as emulator:
            for step in emulator:
                steps_seen.append(step)
        assert len(steps_seen) == {n_steps}, f"steps_seen={{len(steps_seen)}}"
        assert steps_seen == sorted(set(steps_seen)), f"steps not monotonically increasing: {{steps_seen}}"
    """)
    assert proc.returncode == 0, proc.stderr
