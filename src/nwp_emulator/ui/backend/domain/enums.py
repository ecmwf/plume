"""Constants for setup state."""

RUN_MODE_GRIB = "grib"
RUN_MODE_CONFIG = "config"
RUN_MODES = {RUN_MODE_GRIB, RUN_MODE_CONFIG}

CHECK_STATUS_IDLE = "not-run"
CHECK_STATUS_RUNNING = "running"
CHECK_STATUS_PASSED = "passed"
CHECK_STATUS_FAILED = "failed"
