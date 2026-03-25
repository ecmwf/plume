"""Checks orchestration for Setup tab readiness."""

from ..domain.enums import (
    CHECK_STATUS_FAILED,
    CHECK_STATUS_IDLE,
    CHECK_STATUS_PASSED,
    RUN_MODE_CONFIG,
    RUN_MODE_GRIB,
)
from ..storage.session_store import SessionStore
from ..validation.grib_validators import validate_grib_metadata
from ..validation.setup_validators import validate_mpi_np
from ..validation.yaml_validators import validate_emulator_config, validate_plume_plugins_template


class ChecksService:
    def __init__(self, store: SessionStore):
        self._store = store

    def run_checks(self):
        session = self._store.get()
        errors = []
        messages = []
        result_messages = {
            "plume_plugins_template": [],
            "emulator_yaml_basic": [],
            "grib_source_selection": [],
            "mpi_np_config": [],
        }
        results = {
            "plume_plugins_template": CHECK_STATUS_IDLE,
            "emulator_yaml_basic": CHECK_STATUS_IDLE,
            "grib_source_selection": CHECK_STATUS_IDLE,
            "mpi_np_config": CHECK_STATUS_IDLE,
        }

        # Check mpi_np configuration
        mpi_ok, mpi_messages = validate_mpi_np(session.options.mpi_np)
        results["mpi_np_config"] = CHECK_STATUS_PASSED if mpi_ok else CHECK_STATUS_FAILED
        if not mpi_ok:
            errors.extend(mpi_messages)
            result_messages["mpi_np_config"].extend(mpi_messages)
        else:
            # Add warnings but keep status PASSED
            result_messages["mpi_np_config"].extend(mpi_messages)

        if session.options.run_mode == RUN_MODE_CONFIG:
            emulator_ok, emulator_messages = validate_emulator_config(session.emulator_config.text)
            results["emulator_yaml_basic"] = CHECK_STATUS_PASSED if emulator_ok else CHECK_STATUS_FAILED
            if not emulator_ok:
                errors.extend(emulator_messages)
                result_messages["emulator_yaml_basic"].extend(emulator_messages)

        if session.options.dry_run:
            messages.append("Dry run enabled: plume config checks are informational only")
            results["plume_plugins_template"] = CHECK_STATUS_IDLE
        else:
            plume_ok, plume_messages = validate_plume_plugins_template(session.plume_config.text)
            results["plume_plugins_template"] = CHECK_STATUS_PASSED if plume_ok else CHECK_STATUS_FAILED
            if not plume_ok:
                errors.extend(plume_messages)
                result_messages["plume_plugins_template"].extend(plume_messages)

        if session.options.run_mode == RUN_MODE_GRIB:
            grib_ok, grib_messages, grib_meta = validate_grib_metadata(
                session.grib_source.path_display
            )
            session.grib_source.valid = grib_ok
            session.grib_source.metadata = grib_meta
            results["grib_source_selection"] = (
                CHECK_STATUS_PASSED if grib_ok else CHECK_STATUS_FAILED
            )
            if not grib_ok:
                errors.extend(grib_messages)
                result_messages["grib_source_selection"].extend(grib_messages)

        status = CHECK_STATUS_PASSED if not errors else CHECK_STATUS_FAILED
        session.checks.status = status
        session.checks.messages = errors + messages
        session.checks.results = results
        session.checks.result_messages = result_messages
        self._store.save(session)
        return session
