import pathlib
import sys
import unittest
from importlib import import_module


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

setup_routes_module = import_module("backend.api.setup_routes")
checks_service_module = import_module("backend.services.checks_service")
setup_service_module = import_module("backend.services.setup_service")
source_service_module = import_module("backend.services.source_service")
store_module = import_module("backend.storage.in_memory_store")
enums_module = import_module("backend.domain.enums")
yaml_validators_module = import_module("backend.validation.yaml_validators")


SetupRoutes = setup_routes_module.SetupRoutes
ChecksService = checks_service_module.ChecksService
SetupService = setup_service_module.SetupService
SourceService = source_service_module.SourceService
InMemorySessionStore = store_module.InMemorySessionStore
RUN_MODE_CONFIG = enums_module.RUN_MODE_CONFIG
RUN_MODE_GRIB = enums_module.RUN_MODE_GRIB


class SetupRoutesParamsTest(unittest.TestCase):
    def setUp(self):
        self.store = InMemorySessionStore()
        self.routes = SetupRoutes(
            SetupService(self.store),
            SourceService(self.store),
            ChecksService(self.store),
        )

    def test_get_params_config_mode_extracts_plugin_names_and_field_names(self):
        if getattr(yaml_validators_module, "yaml", None) is None:
            self.skipTest("PyYAML not installed")

        session = self.store.get()
        session.options.run_mode = RUN_MODE_CONFIG
        session.plume_config.text = """
plugins:
  - name: out_temp
    lib: liba.so
    parameters: []
    core-config: {}
  - name: out_wind
    lib: libb.so
    parameters: []
    core-config: {}
  - name: out_temp
    lib: libc.so
    parameters: []
    core-config: {}
"""
        session.emulator_config.text = """
emulator:
  n_steps: 2
  grid_identifier: N80
  vertical_levels: 5
  fields:
    100u:
      levtype: sfc
      apply:
        vortex_rollup: {}
    u:
      apply:
        levels:
          "1,3":
            random: {}
          "4:":
            gaussian: {}
    v: "u"
"""
        self.store.save(session)

        payload = self.routes.get_params()

        self.assertEqual(payload["plugin_output_names"], ["out_temp", "out_wind"])
        self.assertIn("100u,sfc,0", payload["field_names"])
        # u has no levtype -> all model levels 1..vertical_levels
        for i in range(1, 6):
            self.assertIn(f"u,ml,{i}", payload["field_names"])
        # v is an alias for u -> same model levels under the v short_name
        for i in range(1, 6):
            self.assertIn(f"v,ml,{i}", payload["field_names"])
        # apply.levels blocks are intentionally ignored by the new logic
        self.assertEqual(len(payload["field_names"]), 11)  # 1 sfc + 5 u/ml + 5 v/ml

    def test_get_params_grib_mode_uses_grib_metadata_params(self):
        if getattr(yaml_validators_module, "yaml", None) is None:
            self.skipTest("PyYAML not installed")

        session = self.store.get()
        session.options.run_mode = RUN_MODE_GRIB
        session.plume_config.text = """
plugins:
  - name: out_precip
    lib: libp.so
    parameters: []
    core-config: {}
"""
        session.grib_source.metadata = {
            "params": [
                "u,ml,1",
                "100u,sfc,0",
                "u,ml,1",
                "v,ml,2",
            ]
        }
        self.store.save(session)

        payload = self.routes.get_params()

        self.assertEqual(payload["plugin_output_names"], ["out_precip"])
        self.assertEqual(payload["field_names"], ["100u,sfc,0", "u,ml,1", "v,ml,2"])

    def test_get_params_dry_run_returns_empty_plugin_names(self):
        if getattr(yaml_validators_module, "yaml", None) is None:
            self.skipTest("PyYAML not installed")

        session = self.store.get()
        session.options.run_mode = RUN_MODE_CONFIG
        session.options.dry_run = True
        session.plume_config.text = """
plugins:
  - name: out_temp
    lib: liba.so
    parameters: []
    core-config: {}
"""
        session.emulator_config.text = """
emulator:
  n_steps: 1
  grid_identifier: N80
  vertical_levels: 2
  fields:
    t:
      levtype: sfc
"""
        self.store.save(session)

        payload = self.routes.get_params()

        self.assertEqual(payload["plugin_output_names"], [])


if __name__ == "__main__":
    unittest.main()
