# pynwp_emulator

Python interface to the [Plume](https://github.com/ecmwf/plume) NWP emulator.

## Installation

Build plume with the Python interface enabled:

```bash
cmake -DPLUME_ENABLE_NWP_EMULATOR=ON \
      -DPLUME_ENABLE_PYTHON_NWP_EMULATOR_INTERFACE=ON \
      ..
make -j$(nproc)
```

Then put the staging directory on `PYTHONPATH`:

```bash
export PYTHONPATH=<build_dir>/pynwp_emulator-staging:$PYTHONPATH
```

## Quick start

```python
import pynwp_emulator as emu

opts = emu.RunOptions()
opts.data_source_type = emu.DataSourceType.GRIB
opts.data_source_path = "/path/to/grib/dir"
opts.plume_config_path = "/path/to/plume.yml"  # optional

result = emu.execute(opts)
print(result)
```

## Dependencies

- [eccodes](https://github.com/ecmwf/eccodes) (loaded via `findlibs` if available)
- [eckit](https://github.com/ecmwf/eckit)
- [atlas](https://github.com/ecmwf/atlas)
- [plume](https://github.com/ecmwf/plume) built with `PLUME_ENABLE_NWP_EMULATOR=ON`
- Python ≥ 3.11
- [findlibs](https://github.com/ecmwf/findlibs) for resolving shared libraries in non-standard locations

## Precision variants

The Python module is always named `nwp_emulator_bindings`. CMake selects the
best available precision at configure time (double preferred, single fallback)
and links accordingly. The choice is transparent to Python callers — all
public-facing types use `double`.

## License

Apache License Version 2.0 — see [LICENSE](../../LICENSE).
