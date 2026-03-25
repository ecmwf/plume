"""Source ingestion and update service for Setup tab."""

from ..domain.enums import CHECK_STATUS_IDLE
from ..domain.errors import SetupValidationError
from ..storage.session_store import SessionStore
from ..validation.grib_validators import validate_grib_selection
from ..validation.yaml_validators import validate_yaml_text


class SourceService:
    def __init__(self, store: SessionStore):
        self._store = store

    def update_plume_config(self, payload):
        text = str(payload.get("text", ""))
        path_display = str(payload.get("path_display", ""))

        valid, messages = validate_yaml_text(text)
        session = self._store.get()
        session.plume_config.text = text
        session.plume_config.path_display = path_display
        session.plume_config.valid = valid
        session.plume_config.messages = messages
        session.checks.status = CHECK_STATUS_IDLE
        session.checks.messages = []
        session.checks.results = {}
        session.checks.result_messages = {}
        self._store.save(session)
        return session

    def update_emulator_config(self, payload):
        text = str(payload.get("text", ""))
        path_display = str(payload.get("path_display", ""))

        valid, messages = validate_yaml_text(text)
        session = self._store.get()
        session.emulator_config.text = text
        session.emulator_config.path_display = path_display
        session.emulator_config.valid = valid
        session.emulator_config.messages = messages
        session.checks.status = CHECK_STATUS_IDLE
        session.checks.messages = []
        session.checks.results = {}
        session.checks.result_messages = {}
        self._store.save(session)
        return session

    def update_grib_source(self, payload):
        source_type = str(payload.get("source_type", ""))
        path_display = str(payload.get("path_display", ""))
        selected_paths = payload.get("selected_paths", [])

        if not isinstance(selected_paths, list):
            raise SetupValidationError("selected_paths must be a list")

        validate_grib_selection(source_type, selected_paths)

        session = self._store.get()
        session.grib_source.source_type = source_type
        session.grib_source.path_display = path_display
        session.grib_source.selected_paths = [str(path) for path in selected_paths]
        session.grib_source.valid = True
        session.grib_source.messages = []
        session.grib_source.metadata = {}
        session.checks.status = CHECK_STATUS_IDLE
        session.checks.messages = []
        session.checks.results = {}
        session.checks.result_messages = {}
        self._store.save(session)
        return session
