# Copyright 2025-, European Centre for Medium Range Weather Forecasts.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
"""Integration smoke tests for the real nwp_emulator pybind module."""

import importlib
import os
import pathlib
import sys


def _required_env(name: str) -> str:
    value = os.getenv(name, "").strip()
    if not value:
        raise RuntimeError(f"Missing required environment variable: {name}")
    return value


def main() -> int:
    module_name = _required_env("NWP_BINDING_MODULE")
    module_dir = _required_env("NWP_BINDING_MODULE_DIR")
    test_data_dir = _required_env("TEST_DATA_DIR")

    sys.path.insert(0, module_dir)
    mod = importlib.import_module(module_name)

    config_path = str(pathlib.Path(test_data_dir) / "valid_config.yml")

    # Positive path: dry-run config execute through bound module-level function.
    opts = mod.RunOptions()
    opts.data_source_type = mod.DataSourceType.CONFIG
    opts.data_source_path = config_path
    opts.plume_config_path = ""
    result = mod.execute(opts)

    assert result.return_code == 0, f"expected return_code=0, got {result.return_code}"
    assert result.plume_run is False, "dry-run should not set plume_run"
    assert result.last_step_run >= 0, "expected at least one completed step"

    # Positive path: class-based execute.
    core = mod.NWPEmulatorCore()
    class_result = core.execute(opts)
    assert class_result.return_code == 0, (
        f"class execute expected return_code=0, got {class_result.return_code}"
    )

    # Negative path: invalid options should return non-zero.
    bad_opts = mod.RunOptions()
    bad_result = mod.execute(bad_opts)
    assert bad_result.return_code != 0, "invalid options should fail"

    print(f"{module_name} integration smoke test passed")

    # Grib path (optional: only runs when model data files are available).
    grib_data_dir_str = os.getenv("NWP_GRIB_DATA_DIR", "").strip()
    if grib_data_dir_str:
        grib_data_dir = pathlib.Path(grib_data_dir_str)
        grib_files = sorted(grib_data_dir.glob("*.grib")) if grib_data_dir.exists() else []
        if grib_files:
            grib_opts = mod.RunOptions()
            grib_opts.data_source_type = mod.DataSourceType.GRIB
            grib_opts.data_source_path = str(grib_data_dir)
            grib_opts.plume_config_path = ""
            grib_result = mod.execute(grib_opts)
            assert grib_result.return_code == 0, (
                f"grib execute expected return_code=0, got {grib_result.return_code}"
            )
            assert grib_result.last_step_run >= 0, (
                "expected at least one completed step in grib mode"
            )
            print(f"{module_name} grib path integration test passed")
        else:
            print(f"{module_name} grib path test skipped (no .grib files in {grib_data_dir_str})")
    else:
        print(f"{module_name} grib path test skipped (NWP_GRIB_DATA_DIR not set)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
