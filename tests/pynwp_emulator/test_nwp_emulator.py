# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
"""Binding surface tests for pynwp_emulator.

These tests verify that the Python binding is correctly shaped and that the
native extension initialises cleanly. No real data, MPI environment, or
filesystem access is required.

Specifically they check:

* The package imports cleanly and the runtime initialisation runs.
* Enum values are accessible and have the expected names.
* RunOptions can be constructed and its attributes round-trip correctly.
* RunResult can be constructed, its read-only attributes are accessible,
  and its field types are correct.
* FieldOverlaySnapshot can be constructed, its read-only attributes are
  accessible, and its field types are correct.
* NWPEmulator can be instantiated, with and without options.
* validate_run_options returns the correct boolean for well-formed and
  malformed options (no data access required).
* The context manager protocol (__enter__/__exit__) behaves correctly.
* The iterator protocol (__iter__/__next__) is present and well-formed.
* The module-level ``execute`` symbol is callable.
"""

import pytest
import pynwp_emulator as emu


# ---------------------------------------------------------------------------
# Package import
# ---------------------------------------------------------------------------


def test_package_import():
    """pynwp_emulator must be importable and expose expected public names."""
    expected = {"DataSourceType", "execute", "FieldOverlaySnapshot", "NWPEmulator", "RunOptions", "RunResult"}
    assert expected.issubset(set(dir(emu)))


# ---------------------------------------------------------------------------
# DataSourceType enum
# ---------------------------------------------------------------------------


def test_data_source_type_values():
    assert emu.DataSourceType.GRIB is not None
    assert emu.DataSourceType.CONFIG is not None
    assert emu.DataSourceType.INVALID is not None


def test_data_source_type_distinct():
    assert emu.DataSourceType.GRIB != emu.DataSourceType.CONFIG
    assert emu.DataSourceType.GRIB != emu.DataSourceType.INVALID
    assert emu.DataSourceType.CONFIG != emu.DataSourceType.INVALID


# ---------------------------------------------------------------------------
# RunOptions construction and attribute round-trip
# ---------------------------------------------------------------------------


def test_run_options_defaults():
    opts = emu.RunOptions()
    assert opts.data_source_type == emu.DataSourceType.INVALID
    assert opts.data_source_path == ""
    assert opts.plume_config_path == ""


def test_run_options_attribute_assignment():
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.GRIB
    opts.data_source_path = "/some/path"
    opts.plume_config_path = "/some/plume.yml"

    assert opts.data_source_type == emu.DataSourceType.GRIB
    assert opts.data_source_path == "/some/path"
    assert opts.plume_config_path == "/some/plume.yml"


def test_run_options_config_source():
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.CONFIG
    assert opts.data_source_type == emu.DataSourceType.CONFIG


def test_run_options_equality():
    a = emu.RunOptions()
    a.data_source_type = emu.DataSourceType.GRIB
    a.data_source_path = "/some/path"
    a.plume_config_path = "/some/plume.yml"

    b = emu.RunOptions()
    b.data_source_type = emu.DataSourceType.GRIB
    b.data_source_path = "/some/path"
    b.plume_config_path = "/some/plume.yml"

    assert a == b
    assert not (a != b)


def test_run_options_inequality():
    a = emu.RunOptions()
    a.data_source_path = "/a"
    b = emu.RunOptions()
    b.data_source_path = "/b"
    assert a != b
    assert not (a == b)


def test_run_options_default_equality():
    assert emu.RunOptions() == emu.RunOptions()


def test_run_options_not_equal_to_other_type():
    assert emu.RunOptions() != object()
    assert emu.RunOptions() != "not a RunOptions"


# ---------------------------------------------------------------------------
# RunResult construction and read-only attributes
# ---------------------------------------------------------------------------


def test_run_result_defaults():
    r = emu.RunResult()
    assert r.return_code == 0
    assert r.plume_run is False
    assert r.last_step_run == -1


def test_run_result_repr():
    r = emu.RunResult()
    text = repr(r)
    assert "RunResult" in text
    assert "return_code" in text


def test_run_result_read_only():
    r = emu.RunResult()
    with pytest.raises(AttributeError):
        r.return_code = 1  # type: ignore[misc]


def test_run_result_equality():
    assert emu.RunResult() == emu.RunResult()


def test_run_result_not_equal_to_other_type():
    assert emu.RunResult() != object()


# ---------------------------------------------------------------------------
# FieldOverlaySnapshot construction and read-only attributes
# ---------------------------------------------------------------------------


def test_field_overlay_snapshot_defaults():
    s = emu.FieldOverlaySnapshot()
    assert s.field_key == ""
    assert s.step == -1
    assert s.rank == 0
    assert s.root == 0
    assert s.nprocs == 1
    assert s.lon == []
    assert s.lat == []
    assert s.values == []


def test_field_overlay_snapshot_repr():
    s = emu.FieldOverlaySnapshot()
    text = repr(s)
    assert "FieldOverlaySnapshot" in text


def test_field_overlay_snapshot_read_only():
    s = emu.FieldOverlaySnapshot()
    with pytest.raises(AttributeError):
        s.field_key = "u"  # type: ignore[misc]


# ---------------------------------------------------------------------------
# NWPEmulator instantiation
# ---------------------------------------------------------------------------


def test_nwp_emulator_instantiation():
    """NWPEmulator must be constructible without arguments."""
    emulator = emu.NWPEmulator()
    assert emulator is not None


def test_nwp_emulator_instantiation_with_options():
    """NWPEmulator must accept RunOptions in the constructor."""
    opts = emu.RunOptions()
    emulator = emu.NWPEmulator(opts)
    assert emulator is not None


def test_nwp_emulator_available_field_keys_before_setup():
    """available_field_keys returns an empty list before the data provider is set up."""
    emulator = emu.NWPEmulator()
    keys = emulator.available_field_keys()
    assert isinstance(keys, list)


def test_nwp_emulator_current_step_before_setup():
    emulator = emu.NWPEmulator()
    step = emulator.current_step()
    assert isinstance(step, int)


# ---------------------------------------------------------------------------
# Module-level execute symbol
# ---------------------------------------------------------------------------


def test_execute_is_callable():
    assert callable(emu.execute)


# ---------------------------------------------------------------------------
# Context manager protocol
# ---------------------------------------------------------------------------


def test_context_manager_returns_self():
    """`with NWPEmulator() as e` must bind the same object."""
    emulator = emu.NWPEmulator()
    with emulator as ctx:
        assert ctx is emulator


def test_context_manager_suppresses_no_exceptions():
    """Exceptions raised inside the `with` block must propagate."""
    with pytest.raises(RuntimeError, match="boom"):
        with emu.NWPEmulator():
            raise RuntimeError("boom")


def test_context_manager_teardown_without_setup():
    """__exit__ must not raise when the data provider was never set up.

    finalize_plume() and finalize_run() both guard against uninitialised
    state in C++, so exiting cleanly without any setup must be safe.
    """
    with emu.NWPEmulator():
        pass  # no setup_data_provider, no iteration


# ---------------------------------------------------------------------------
# Iterator protocol
# ---------------------------------------------------------------------------


def test_iterator_protocol_present():
    """`NWPEmulator` must expose `__iter__` and `__next__`."""
    emulator = emu.NWPEmulator()
    assert hasattr(emulator, "__iter__")
    assert hasattr(emulator, "__next__")
    assert iter(emulator) is emulator


# ---------------------------------------------------------------------------
# validate_run_options
# ---------------------------------------------------------------------------


def test_validate_run_options_invalid_type_and_empty_path():
    """INVALID source type with empty path must fail validation."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    assert opts.data_source_type == emu.DataSourceType.INVALID
    assert opts.data_source_path == ""
    assert emulator.validate_run_options(opts) is False


def test_validate_run_options_invalid_type_with_path():
    """INVALID source type must fail validation even when a path is provided."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.INVALID
    opts.data_source_path = "/some/path"
    assert emulator.validate_run_options(opts) is False


def test_validate_run_options_valid_type_empty_path():
    """A valid source type with an empty path must fail validation."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.GRIB
    opts.data_source_path = ""
    assert emulator.validate_run_options(opts) is False


def test_validate_run_options_grib_with_path():
    """GRIB type with a non-empty path must pass validation."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.GRIB
    opts.data_source_path = "/some/path"
    assert emulator.validate_run_options(opts) is True


def test_validate_run_options_config_with_path():
    """CONFIG type with a non-empty path must pass validation."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.CONFIG
    opts.data_source_path = "/some/path"
    assert emulator.validate_run_options(opts) is True


def test_validate_run_options_does_not_mutate_options():
    """validate_run_options must be side-effect free."""
    emulator = emu.NWPEmulator()
    opts = emu.RunOptions()
    opts.data_source_type = emu.DataSourceType.GRIB
    opts.data_source_path = "/some/path"
    opts.plume_config_path = "/some/plume.yml"
    emulator.validate_run_options(opts)
    assert opts.data_source_type == emu.DataSourceType.GRIB
    assert opts.data_source_path == "/some/path"
    assert opts.plume_config_path == "/some/plume.yml"


# ---------------------------------------------------------------------------
# Type correctness of FieldOverlaySnapshot and RunResult fields
# ---------------------------------------------------------------------------


def test_field_overlay_snapshot_field_types():
    """All fields of a default-constructed FieldOverlaySnapshot must have correct types."""
    s = emu.FieldOverlaySnapshot()
    assert isinstance(s.field_key, str)
    assert isinstance(s.lon, list)
    assert isinstance(s.lat, list)
    assert isinstance(s.values, list)
    assert isinstance(s.step, int)
    assert isinstance(s.rank, int)
    assert isinstance(s.root, int)
    assert isinstance(s.nprocs, int)


def test_run_result_field_types():
    """return_code and last_step_run must be int; plume_run must be bool."""
    r = emu.RunResult()
    assert isinstance(r.return_code, int)
    assert isinstance(r.last_step_run, int)
    assert isinstance(r.plume_run, bool)


def test_run_result_repr_contains_all_fields():
    r = emu.RunResult()
    text = repr(r)
    assert "return_code" in text
    assert "plume_run" in text
    assert "last_step_run" in text


def test_field_overlay_snapshot_repr_contains_key_fields():
    s = emu.FieldOverlaySnapshot()
    text = repr(s)
    assert "FieldOverlaySnapshot" in text
    assert "step" in text
    assert "rank" in text
    assert "nprocs" in text
