"""Validation for GRIB source selections and metadata consistency."""

import os

from ..domain.errors import SetupValidationError

try:
    import eccodes as _eccodes  # type: ignore[import-not-found]
except ImportError:
    _eccodes = None

ALLOWED_GRIB_EXTS = (".grib", ".grib1", ".grib2")


def _param_sort_key(param_md):
    parts = str(param_md).split(",", 2)
    short_name = parts[0] if len(parts) > 0 else ""
    levtype = parts[1] if len(parts) > 1 else ""
    level = parts[2] if len(parts) > 2 else ""
    return (short_name, levtype, level)


def _read_message_metadata(ec, handle):
    """Return (grid_name, param_md) from an open eccodes handle.

    param_md is a comma-separated string ``shortName,levtype,level``,
    matching the format logged by the C++ GRIBFileReader.
    """
    try:
        grid_name = ec.codes_get_string(handle, "gridName")
    except ec.CodesInternalError:
        grid_name = "unknown"

    parts = []
    for key in ("shortName", "levtype", "level"):
        try:
            parts.append(str(ec.codes_get_string(handle, key)))
        except ec.CodesInternalError:
            parts.append("")
    return grid_name, ",".join(parts)


def validate_grib_metadata(folder_path):
    """Validate a GRIB folder on the server filesystem.

    Checks (non-recursive, matching the C++ GRIBFileReader logic):
      1. Folder exists and contains at least one .grib / .grib1 / .grib2 file.
      2. (eccodes required) Every file shares the same grid identifier and the
         same set of (shortName, levtype, level) combinations.  Within each
         file, that set must also be unique.

    Returns ``(ok, messages, metadata)`` where *metadata* is a dict with:
      - ``grib_file_count``  – number of matching files found
      - ``grid_identifier``  – gridName value from the first message
      - ``params``           – list of "shortName,levtype,level" strings from
                               the reference file (first file read)
    """
    folder = (folder_path or "").strip()
    if not folder:
        return False, ["Full path to GRIB folder is required for metadata checks"], {}

    if not os.path.isdir(folder):
        return False, [f"Folder not found: {folder!r}"], {}

    try:
        entries = os.listdir(folder)
    except PermissionError:
        return False, [f"Permission denied reading folder: {folder!r}"], {}

    grib_files = sorted(
        f for f in entries
        if os.path.isfile(os.path.join(folder, f)) and f.lower().endswith(ALLOWED_GRIB_EXTS)
    )

    if not grib_files:
        return (
            False,
            [f"No GRIB files (.grib, .grib1, .grib2) found in {folder!r} (non-recursive)"],
            {},
        )

    base_metadata = {
        "grib_file_count": len(grib_files),
        "grid_identifier": "unknown",
        "params": ["unknown"],
    }

    if _eccodes is None:
        return (True, [], base_metadata)

    messages = []
    reference_grid = None
    reference_params_sorted = None
    metadata = dict(base_metadata)

    for filename in grib_files:
        full_path = os.path.join(folder, filename)
        file_params = []
        file_grid = None
        try:
            with open(full_path, "rb") as f:
                while True:
                    handle = _eccodes.codes_grib_new_from_file(f)
                    if handle is None:
                        break
                    try:
                        grid_name, param_md = _read_message_metadata(_eccodes, handle)
                        if file_grid is None:
                            file_grid = grid_name
                        file_params.append(param_md)
                    finally:
                        _eccodes.codes_release(handle)
        except OSError as exc:
            messages.append(f"{filename}: could not open file: {exc}")
            continue

        # (shortName, levtype, level) combinations must be unique within a file
        if len(set(file_params)) != len(file_params):
            messages.append(
                f"{filename}: (shortName, levtype, level) combinations must be unique per file"
            )
            continue

        if reference_grid is None:
            # First valid file sets the reference
            reference_grid = file_grid
            reference_params_sorted = sorted(file_params)
            metadata["grid_identifier"] = reference_grid or "unknown"
            metadata["params"] = sorted(file_params, key=_param_sort_key)
        else:
            if file_grid != reference_grid:
                messages.append(
                    f"{filename}: grid {file_grid!r} is inconsistent with reference {reference_grid!r}"
                )
            if sorted(file_params) != reference_params_sorted:
                messages.append(
                    f"{filename}: fields/levels are inconsistent with reference file"
                )

    if reference_grid is None and not messages:
        # All files failed to open
        messages.append("No GRIB files could be read")

    return len(messages) == 0, messages, metadata


def validate_grib_selection(source_type, selected_paths):
    if source_type not in {"folder", "files"}:
        raise SetupValidationError("source_type must be 'folder' or 'files'")

    if not isinstance(selected_paths, list):
        raise SetupValidationError("selected_paths must be a list")

    if source_type == "files":
        if not selected_paths:
            raise SetupValidationError("selected_paths must be a non-empty list")
        bad = [path for path in selected_paths if not path.lower().endswith(ALLOWED_GRIB_EXTS)]
        if bad:
            raise SetupValidationError("All files must use .grib, .grib1, or .grib2 extensions")

    return True
