"""Response constructors for Setup API handlers."""

import json


def json_response(status_code, payload):
    body = json.dumps(payload).encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Content-Length": str(len(body)),
    }
    return status_code, headers, body


def error_response(message, status_code=400):
    return json_response(status_code, {"error": message})
