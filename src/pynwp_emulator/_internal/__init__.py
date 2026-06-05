# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# libplume_nwp_emulator and dependencies have to be loaded prior to importing
# pynwp_emulator
import findlibs

findlibs.load("eccodes")

try:
    findlibs.load("plume_nwp_emulator_dp")
except OSError:
    findlibs.load("plume_nwp_emulator_sp")

from nwp_emulator_bindings import nwp_emulator_bindings as _bindings

# Initialise the eckit/Atlas runtime once, before any other symbol is used.
_bindings.init_bindings()

# Re-export raw binding types for use by nwp_emulator.py.
DataSourceType = _bindings.DataSourceType
RunOptions = _bindings.RunOptions
RunResult = _bindings.RunResult
FieldOverlaySnapshot = _bindings.FieldOverlaySnapshot
NWPEmulatorCore = _bindings.NWPEmulatorCore

__all__ = [
    "DataSourceType",
    "FieldOverlaySnapshot",
    "NWPEmulatorCore",
    "RunOptions",
    "RunResult",
]
