"""Setup options service."""

from ..domain.enums import CHECK_STATUS_IDLE
from ..storage.session_store import SessionStore
from ..validation.setup_validators import validate_options_payload


class SetupService:
    def __init__(self, store: SessionStore):
        self._store = store

    def get_session(self):
        return self._store.get()

    def update_options(self, payload):
        dry_run, run_mode, mpi_np = validate_options_payload(payload)
        session = self._store.get()
        session.options.dry_run = dry_run
        session.options.run_mode = run_mode
        session.options.mpi_np = mpi_np
        session.checks.status = CHECK_STATUS_IDLE
        session.checks.messages = []
        session.checks.results = {}
        session.checks.result_messages = {}
        self._store.save(session)
        return session
