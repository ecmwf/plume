"""Helpers to parse incoming JSON requests."""

import json

from ..domain.errors import SetupValidationError


def parse_json_body(raw_bytes):
    if not raw_bytes:
        return {}

    try:
        payload = json.loads(raw_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SetupValidationError("Invalid JSON body") from exc

    if not isinstance(payload, dict):
        raise SetupValidationError("JSON payload must be an object")

    return payload
