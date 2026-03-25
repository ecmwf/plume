"""Map domain session objects to API JSON payloads."""


def session_to_dict(session):
    return {
        "options": {
            "dry_run": session.options.dry_run,
            "run_mode": session.options.run_mode,
            "mpi_np": session.options.mpi_np,
        },
        "plume_config": {
            "path_display": session.plume_config.path_display,
            "text": session.plume_config.text,
            "valid": session.plume_config.valid,
            "messages": list(session.plume_config.messages),
        },
        "emulator_config": {
            "path_display": session.emulator_config.path_display,
            "text": session.emulator_config.text,
            "valid": session.emulator_config.valid,
            "messages": list(session.emulator_config.messages),
        },
        "grib_source": {
            "source_type": session.grib_source.source_type,
            "path_display": session.grib_source.path_display,
            "selected_paths": list(session.grib_source.selected_paths),
            "valid": session.grib_source.valid,
            "messages": list(session.grib_source.messages),
            "metadata": dict(session.grib_source.metadata),
        },
        "checks": {
            "status": session.checks.status,
            "messages": list(session.checks.messages),
            "results": dict(session.checks.results),
            "result_messages": dict(session.checks.result_messages),
        },
    }
