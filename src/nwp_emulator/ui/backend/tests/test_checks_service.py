import pathlib
import sys
import unittest
from importlib import import_module
from unittest.mock import patch


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

enums = import_module("backend.domain.enums")
checks_service_module = import_module("backend.services.checks_service")
store_module = import_module("backend.storage.in_memory_store")

CHECK_STATUS_FAILED = enums.CHECK_STATUS_FAILED
CHECK_STATUS_IDLE = enums.CHECK_STATUS_IDLE
CHECK_STATUS_PASSED = enums.CHECK_STATUS_PASSED
RUN_MODE_CONFIG = enums.RUN_MODE_CONFIG
RUN_MODE_GRIB = enums.RUN_MODE_GRIB
ChecksService = checks_service_module.ChecksService
InMemorySessionStore = store_module.InMemorySessionStore


class ChecksServiceTest(unittest.TestCase):
    def setUp(self):
        self.store = InMemorySessionStore()
        self.service = ChecksService(self.store)
        session = self.store.get()
        session.plume_config.text = "plugins: []"
        session.emulator_config.text = "emulator: {}"
        session.grib_source.path_display = "/tmp/grib"
        self.store.save(session)

    @patch("backend.services.checks_service.validate_grib_metadata")
    @patch("backend.services.checks_service.validate_plume_plugins_template")
    @patch("backend.services.checks_service.validate_emulator_config")
    def test_config_mode_runs_emulator_and_plume_checks(self, emulator_mock, plume_mock, grib_mock):
        emulator_mock.return_value = (True, [])
        plume_mock.return_value = (True, [])
        grib_mock.return_value = (True, [], {"grib_file_count": 1, "grid_identifier": "unknown", "params": []})

        session = self.store.get()
        session.options.run_mode = RUN_MODE_CONFIG
        session.options.dry_run = False
        self.store.save(session)

        updated = self.service.run_checks()

        emulator_mock.assert_called_once_with("emulator: {}")
        plume_mock.assert_called_once_with("plugins: []")
        grib_mock.assert_not_called()
        self.assertEqual(updated.checks.results["emulator_yaml_basic"], CHECK_STATUS_PASSED)
        self.assertEqual(updated.checks.results["plume_plugins_template"], CHECK_STATUS_PASSED)
        self.assertEqual(updated.checks.results["grib_source_selection"], CHECK_STATUS_IDLE)

    @patch("backend.services.checks_service.validate_grib_metadata")
    @patch("backend.services.checks_service.validate_plume_plugins_template")
    @patch("backend.services.checks_service.validate_emulator_config")
    def test_grib_mode_still_runs_plume_check_when_not_dry_run(self, emulator_mock, plume_mock, grib_mock):
        emulator_mock.return_value = (True, [])
        plume_mock.return_value = (False, ["plume invalid"])
        grib_mock.return_value = (True, [], {"grib_file_count": 2, "grid_identifier": "N80", "params": ["u,sfc,0"]})

        session = self.store.get()
        session.options.run_mode = RUN_MODE_GRIB
        session.options.dry_run = False
        self.store.save(session)

        updated = self.service.run_checks()

        emulator_mock.assert_not_called()
        plume_mock.assert_called_once_with("plugins: []")
        grib_mock.assert_called_once_with("/tmp/grib")
        self.assertEqual(updated.checks.results["plume_plugins_template"], CHECK_STATUS_FAILED)
        self.assertEqual(updated.checks.results["grib_source_selection"], CHECK_STATUS_PASSED)
        self.assertEqual(updated.checks.status, CHECK_STATUS_FAILED)
        self.assertIn("plume invalid", updated.checks.result_messages["plume_plugins_template"])

    @patch("backend.services.checks_service.validate_grib_metadata")
    @patch("backend.services.checks_service.validate_plume_plugins_template")
    @patch("backend.services.checks_service.validate_emulator_config")
    def test_dry_run_keeps_plume_check_idle_in_grib_mode(self, emulator_mock, plume_mock, grib_mock):
        emulator_mock.return_value = (True, [])
        plume_mock.return_value = (True, [])
        grib_mock.return_value = (False, ["grib invalid"], {})

        session = self.store.get()
        session.options.run_mode = RUN_MODE_GRIB
        session.options.dry_run = True
        self.store.save(session)

        updated = self.service.run_checks()

        emulator_mock.assert_not_called()
        plume_mock.assert_not_called()
        grib_mock.assert_called_once_with("/tmp/grib")
        self.assertEqual(updated.checks.results["plume_plugins_template"], CHECK_STATUS_IDLE)
        self.assertEqual(updated.checks.results["grib_source_selection"], CHECK_STATUS_FAILED)
        self.assertIn("Dry run enabled: plume config checks are informational only", updated.checks.messages)


if __name__ == "__main__":
    unittest.main()