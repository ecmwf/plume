"""Storage protocol for setup session state."""

from abc import ABC, abstractmethod

from ..domain.models import SetupSession


class SessionStore(ABC):
    @abstractmethod
    def get(self) -> SetupSession:
        raise NotImplementedError

    @abstractmethod
    def save(self, session: SetupSession) -> None:
        raise NotImplementedError
