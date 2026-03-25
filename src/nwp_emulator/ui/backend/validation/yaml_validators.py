"""Basic YAML checks for setup text sources.

This keeps validation lightweight and dependency-free for now.
"""

import re

from ..domain.errors import SetupValidationError

try:
    import yaml  # type: ignore[import-not-found]
except ImportError:  # pragma: no cover
    yaml = None


if yaml is not None:
    class PlumeLoader(yaml.SafeLoader):
        pass


    def _construct_mapping_preserve_scalar_keys(loader, node, deep=False):
        loader.flatten_mapping(node)
        mapping = {}
        for key_node, value_node in node.value:
            # Preserve scalar keys exactly as written to avoid implicit coercions
            # like "no" -> False or "01" -> 1 in validation messages.
            if getattr(key_node, "id", None) == "scalar":
                key = key_node.value
            else:
                key = loader.construct_object(key_node, deep=deep)
            value = loader.construct_object(value_node, deep=deep)
            mapping[key] = value
        return mapping


    PlumeLoader.add_constructor(
        "tag:yaml.org,2002:map",
        _construct_mapping_preserve_scalar_keys,
    )


    # Keep YAML 1.2-like boolean behavior so literals such as "no" are
    # preserved as strings and can be reported verbatim in validation errors.
    # Also disable timestamp autotyping to avoid date-like strings being parsed
    # into datetime objects implicitly.
    for ch, resolvers in list(PlumeLoader.yaml_implicit_resolvers.items()):
        PlumeLoader.yaml_implicit_resolvers[ch] = [
            (tag, regexp)
            for tag, regexp in resolvers
            if tag not in {"tag:yaml.org,2002:bool", "tag:yaml.org,2002:timestamp"}
        ]

    PlumeLoader.add_implicit_resolver(
        "tag:yaml.org,2002:bool",
        re.compile(r"^(?:true|false|True|False|TRUE|FALSE)$"),
        list("tTfF"),
    )


def validate_yaml_text(text):
    messages = []
    cleaned = (text or "").strip()
    if not cleaned:
        messages.append("YAML content is empty")
    if "\t" in cleaned:
        messages.append("YAML should not contain tab indentation")
    return len(messages) == 0, messages


def validate_emulator_config(text):
    """Validate a Plume NWP emulator configuration file.

    Expected top-level structure::

        emulator:
          n_steps: <positive int>
          grid_identifier: <string>
          vertical_levels: <positive int>
          area: [lat_N, lon_W, lat_S, lon_E]   # optional
          fields:
            <field_name>: <field_alias_string> | <field_def>

    A field definition is a mapping with an optional ``levtype`` string and a
    required ``apply`` mapping.  ``apply`` may either contain direct function
    names (vortex_rollup, random, step, sinc, gaussian …) or a single
    ``levels`` key whose value maps level-spec strings to dicts of function
    names → parameters.
    """
    if yaml is None:
        raise SetupValidationError("PyYAML is required for emulator config checks")

    cleaned = (text or "").strip()
    if not cleaned:
        return False, ["Emulator config is empty"]

    try:
        parsed = yaml.load(cleaned, Loader=PlumeLoader)
    except yaml.YAMLError as exc:
        return False, [f"Emulator config YAML parse error: {exc}"]

    if not isinstance(parsed, dict):
        return False, ["Emulator config must be a mapping with a single top-level key: emulator"]

    top_keys = list(parsed.keys())
    if top_keys != ["emulator"]:
        return False, ["Emulator config must contain exactly one top-level key: emulator"]

    emulator = parsed.get("emulator")
    if not isinstance(emulator, dict):
        return False, ["emulator must be a mapping"]

    messages = []
    allowed_emulator_keys = {"n_steps", "grid_identifier", "vertical_levels", "area", "fields"}
    extra_emulator_keys = set(emulator.keys()) - allowed_emulator_keys
    if extra_emulator_keys:
        keys_list = ", ".join(str(k) for k in sorted(extra_emulator_keys, key=str))
        messages.append(f"emulator has unsupported keys: {keys_list}")

    n_steps = emulator.get("n_steps")
    if n_steps is None:
        messages.append("emulator.n_steps is required")
    elif not isinstance(n_steps, int) or n_steps <= 0:
        messages.append("emulator.n_steps must be a positive integer")

    grid_identifier = emulator.get("grid_identifier")
    if grid_identifier is None:
        messages.append("emulator.grid_identifier is required")
    elif not isinstance(grid_identifier, str) or not grid_identifier.strip():
        messages.append("emulator.grid_identifier must be a non-empty string")

    vertical_levels = emulator.get("vertical_levels")
    if vertical_levels is None:
        messages.append("emulator.vertical_levels is required")
    elif not isinstance(vertical_levels, int) or vertical_levels <= 0:
        messages.append("emulator.vertical_levels must be a positive integer")

    area = emulator.get("area")
    if area is not None:
        if not isinstance(area, list) or len(area) != 4:
            messages.append("emulator.area must be a list of exactly 4 numbers [lat_N, lon_W, lat_S, lon_E]")
        elif not all(isinstance(x, (int, float)) for x in area):
            messages.append("emulator.area values must all be numbers")

    fields = emulator.get("fields")
    if fields is None:
        messages.append("emulator.fields is required")
    elif not isinstance(fields, dict) or not fields:
        messages.append("emulator.fields must be a non-empty mapping")
    else:
        allowed_field_keys = {"levtype", "apply"}
        for field_name, field_def in fields.items():
            fname = f"emulator.fields.{field_name}"
            if isinstance(field_def, str):
                # alias like v: "u"
                continue
            if not isinstance(field_def, dict):
                messages.append(f"{fname} must be a mapping or a field-alias string")
                continue

            extra_field_keys = set(field_def.keys()) - allowed_field_keys
            if extra_field_keys:
                keys_list = ", ".join(str(k) for k in sorted(extra_field_keys, key=str))
                messages.append(f"{fname} has unsupported keys: {keys_list}")

            levtype = field_def.get("levtype")
            if levtype is not None and not isinstance(levtype, str):
                messages.append(f"{fname}.levtype must be a string")

            apply = field_def.get("apply")
            if apply is None:
                messages.append(f"{fname}.apply is required")
            elif not isinstance(apply, dict) or not apply:
                messages.append(f"{fname}.apply must be a non-empty mapping")
            else:
                if "levels" in apply:
                    levels_block = apply["levels"]
                    if not isinstance(levels_block, dict) or not levels_block:
                        messages.append(f"{fname}.apply.levels must be a non-empty mapping")
                    else:
                        for level_spec, funcs in levels_block.items():
                            lname = f"{fname}.apply.levels.{level_spec}"
                            if not isinstance(funcs, dict) or not funcs:
                                messages.append(f"{lname} must be a non-empty mapping of function names to parameters")
                            else:
                                for func_name, func_params in funcs.items():
                                    if func_params is not None and not isinstance(func_params, dict):
                                        messages.append(
                                            f"{lname}.{func_name} parameters must be a mapping or null"
                                        )
                else:
                    for func_name, func_params in apply.items():
                        if func_params is not None and not isinstance(func_params, dict):
                            messages.append(
                                f"{fname}.apply.{func_name} parameters must be a mapping or null"
                            )

    return len(messages) == 0, messages


def validate_plume_plugins_template(text):
    if yaml is None:
        raise SetupValidationError("PyYAML is required for plume config checks")

    cleaned = (text or "").strip()
    if not cleaned:
        return False, ["Plume config is empty"]

    try:
        parsed = yaml.load(cleaned, Loader=PlumeLoader)
    except yaml.YAMLError as exc:
        return False, [f"Plume config YAML parse error: {exc}"]

    if not isinstance(parsed, dict):
        return False, ["Plume config must be a mapping with a single top-level key: plugins"]

    top_keys = list(parsed.keys())
    if top_keys != ["plugins"]:
        return False, ["Plume config must contain exactly one top-level key: plugins"]

    plugins = parsed.get("plugins")
    if not isinstance(plugins, list):
        return False, ["plugins must be a list"]

    messages = []
    allowed_entry_keys = {"name", "lib", "parameters", "core-config"}
    allowed_parameter_keys = {"name", "type", "height", "description"}

    for idx, entry in enumerate(plugins):
        item_name = f"plugins[{idx}]"
        if not isinstance(entry, dict):
            messages.append(f"{item_name} must be a mapping")
            continue

        extra_entry_keys = set(entry.keys()) - allowed_entry_keys
        if extra_entry_keys:
            keys_list = ", ".join(str(key) for key in sorted(extra_entry_keys, key=str))
            messages.append(f"{item_name} has unsupported keys: {keys_list}")

        entry_name = entry.get("name")
        if not isinstance(entry_name, str) or not entry_name.strip():
            messages.append(f"{item_name}.name must be a non-empty string")

        entry_lib = entry.get("lib")
        if not isinstance(entry_lib, str) or not entry_lib.strip():
            messages.append(f"{item_name}.lib must be a non-empty string")

        parameters = entry.get("parameters", [])
        if not isinstance(parameters, list):
            messages.append(f"{item_name}.parameters must be a list")
        else:
            for group_idx, parameter_group in enumerate(parameters):
                group_name = f"{item_name}.parameters[{group_idx}]"
                if not isinstance(parameter_group, list):
                    messages.append(f"{group_name} must be a list")
                    continue

                if not parameter_group:
                    messages.append(f"{group_name} must not be empty")
                    continue

                for p_idx, parameter in enumerate(parameter_group):
                    p_name = f"{group_name}[{p_idx}]"
                    if not isinstance(parameter, dict):
                        messages.append(f"{p_name} must be a mapping")
                        continue

                    extra_parameter_keys = set(parameter.keys()) - allowed_parameter_keys
                    if extra_parameter_keys:
                        keys_list = ", ".join(str(key) for key in sorted(extra_parameter_keys, key=str))
                        messages.append(f"{p_name} has unsupported keys: {keys_list}")

                    parameter_name = parameter.get("name")
                    if not isinstance(parameter_name, str) or not parameter_name.strip():
                        messages.append(f"{p_name}.name must be a non-empty string")

                    param_type = parameter.get("type")
                    if not isinstance(param_type, str) or not param_type.strip():
                        messages.append(f"{p_name}.type must be a non-empty string")
                    elif param_type != param_type.upper():
                        messages.append(f"{p_name}.type must be all caps")

                    param_height = parameter.get("height", 0)
                    if not isinstance(param_height, int) or param_height < 0:
                        messages.append(f"{p_name}.height must be a non-negative integer")

                    param_description = parameter.get("description")
                    if not isinstance(param_description, (str, type(None))):
                        messages.append(f"{p_name}.description must be a string if provided")
        if "core-config" not in entry:
            messages.append(f"{item_name}.core-config is required")

    return len(messages) == 0, messages
