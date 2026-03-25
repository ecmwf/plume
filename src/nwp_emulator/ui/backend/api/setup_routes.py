"""Route handlers for Setup tab backend API."""

from ..domain.errors import SetupValidationError
from ..mappers.session_mapper import session_to_dict


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

        raise SetupValidationError("Unsupported setup API route")
