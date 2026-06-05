# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

"""pynwp_emulator — Python interface to the Plume NWP emulator core.

Importing this package loads the native C++ extension (``nwp_emulator_bindings``)
and initialises the eckit/Atlas runtime before exposing the public API.
"""

# _internal.__init__ is loaded first: it finds the native .so, loads eccodes via
# findlibs, and calls init_bindings() so the runtime is ready before anything else.
from pynwp_emulator._internal import DataSourceType, FieldOverlaySnapshot  # noqa: F401

from pynwp_emulator.nwp_emulator import NWPEmulator, RunOptions, RunResult, execute  # noqa: F401

__all__ = [
    "DataSourceType",
    "execute",
    "FieldOverlaySnapshot",
    "NWPEmulator",
    "RunOptions",
    "RunResult",
]
