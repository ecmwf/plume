"""In-memory setup session store for local single-process launcher use."""

from ..domain.models import SetupSession
from .session_store import SessionStore


class InMemorySessionStore(SessionStore):
    def __init__(self):
        self._session = SetupSession()

    def get(self) -> SetupSession:
        return self._session

    def save(self, session: SetupSession) -> None:
        self._session = session
