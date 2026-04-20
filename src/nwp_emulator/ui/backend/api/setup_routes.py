"""Route handlers for Setup tab backend API."""

from ..domain.enums import RUN_MODE_CONFIG, RUN_MODE_GRIB
from ..domain.errors import SetupValidationError
from ..mappers.session_mapper import session_to_dict
from ..validation.grib_validators import validate_grib_metadata
from ..validation import yaml_validators


def _field_sort_key(field_md):
    parts = str(field_md).split(",", 2)
    short_name = parts[0] if len(parts) > 0 else ""
    levtype = parts[1] if len(parts) > 1 else ""
    level = parts[2] if len(parts) > 2 else ""
    try:
        level_num = int(level)
    except ValueError:
        level_num = level
    return (short_name, levtype, level_num)


def _parse_yaml_mapping(text):
    cleaned = (text or "").strip()
    yaml_mod = getattr(yaml_validators, "yaml", None)
    if not cleaned or yaml_mod is None:
        return {}
    loader = getattr(yaml_validators, "PlumeLoader", None)
    yaml_error = getattr(yaml_mod, "YAMLError", ValueError)
    try:
        if loader is not None:
            parsed = yaml_mod.load(cleaned, Loader=loader)
        else:
            parsed = yaml_mod.safe_load(cleaned)
    except (yaml_error, ValueError, TypeError, AttributeError):
        return {}
    return parsed if isinstance(parsed, dict) else {}


def _field_names_from_config(emulator_text):
    parsed = _parse_yaml_mapping(emulator_text)
    emulator = parsed.get("emulator") if isinstance(parsed.get("emulator"), dict) else {}
    fields = emulator.get("fields") if isinstance(emulator.get("fields"), dict) else {}
    vertical_levels = emulator.get("vertical_levels", 1)
    try:
        vertical_levels = int(vertical_levels)
    except (TypeError, ValueError):
        vertical_levels = 1
    if vertical_levels < 1:
        vertical_levels = 1

    found = set()

    def _apply_field(short_name, field_def):
        if not isinstance(field_def, dict):
            return
        if "levtype" in field_def:
            # Case (1): field has a levtype key — surface field, single entry at level 0.
            levtype = str(field_def["levtype"]).strip() or "sfc"
            found.add(f"{short_name},{levtype},0")
        else:
            # Case (2): no levtype — model-level field, one entry per vertical level.
            for i in range(1, vertical_levels + 1):
                found.add(f"{short_name},ml,{i}")

    for short_name, field_def in fields.items():
        short_name = str(short_name)
        if isinstance(field_def, str):
            # Case (3): alias string — resolve to the referenced field's definition.
            aliased_def = fields.get(field_def.strip())
            if isinstance(aliased_def, dict):
                _apply_field(short_name, aliased_def)
            # If the alias target doesn't exist, the config validator will flag it.
        else:
            _apply_field(short_name, field_def)

    return sorted(found, key=_field_sort_key)


def _field_names_from_grib(session):
    metadata = session.grib_source.metadata if isinstance(session.grib_source.metadata, dict) else {}
    params = metadata.get("params") if isinstance(metadata.get("params"), list) else []
    if params:
        unique = sorted({str(item) for item in params if str(item).strip()}, key=_field_sort_key)
        if unique:
            return unique

    folder = (session.grib_source.path_display or "").strip()
    if not folder:
        return []

    ok, _messages, grib_meta = validate_grib_metadata(folder)
    if not ok:
        return []
    parsed = grib_meta.get("params") if isinstance(grib_meta, dict) else []
    if not isinstance(parsed, list):
        return []
    return sorted({str(item) for item in parsed if str(item).strip()}, key=_field_sort_key)


def _plugin_output_names(plume_text):
    parsed = _parse_yaml_mapping(plume_text)
    plugins = parsed.get("plugins") if isinstance(parsed.get("plugins"), list) else []
    names = []
    seen = set()
    for plugin in plugins:
        if not isinstance(plugin, dict):
            continue
        name = plugin.get("name")
        if not isinstance(name, str):
            continue
        cleaned = name.strip()
        if not cleaned or cleaned in seen:
            continue
        seen.add(cleaned)
        names.append(cleaned)
    return names


class SetupRoutes:
    def __init__(self, setup_service, source_service, checks_service):
        self._setup_service = setup_service
        self._source_service = source_service
        self._checks_service = checks_service

    def get_state(self):
        session = self._setup_service.get_session()
        return session_to_dict(session)

    def update_options(self, payload):
        session = self._setup_service.update_options(payload)
        return session_to_dict(session)

    def update_plume_config(self, payload):
        session = self._source_service.update_plume_config(payload)
        return session_to_dict(session)

    def update_emulator_config(self, payload):
        session = self._source_service.update_emulator_config(payload)
        return session_to_dict(session)

    def update_grib_source(self, payload):
        session = self._source_service.update_grib_source(payload)
        return session_to_dict(session)

    def run_checks(self):
        session = self._checks_service.run_checks()
        return session_to_dict(session)

    def get_params(self):
        session = self._setup_service.get_session()

        if session.options.dry_run:
            plugin_output_names = []
        else:
            plugin_output_names = _plugin_output_names(session.plume_config.text)

        if session.options.run_mode == RUN_MODE_GRIB:
            field_names = _field_names_from_grib(session)
        elif session.options.run_mode == RUN_MODE_CONFIG:
            field_names = _field_names_from_config(session.emulator_config.text)
        else:
            field_names = []

        return {
            "field_names": field_names,
            "plugin_output_names": plugin_output_names,
        }

    def dispatch(self, method, path, payload):
        if method == "GET" and path == "/api/setup/state":
            return self.get_state()

        if method == "POST" and path == "/api/setup/options":
            return self.update_options(payload)

        if method == "POST" and path == "/api/setup/source/plume-config":
            return self.update_plume_config(payload)

        if method == "POST" and path == "/api/setup/source/emulator-config":
            return self.update_emulator_config(payload)

        if method == "POST" and path == "/api/setup/source/grib":
            return self.update_grib_source(payload)

        if method == "POST" and path == "/api/setup/checks/run":
            return self.run_checks()

        if method == "GET" and path == "/api/setup/params":
            return self.get_params()

        raise SetupValidationError("Unsupported setup API route")
