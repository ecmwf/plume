"""Domain-level exceptions for setup backend."""


class SetupValidationError(ValueError):
    """Raised when setup payloads or state transitions are invalid."""
