# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

add_subdirectory(plume)

if (HAVE_NWP_EMULATOR)
    add_subdirectory(nwp_emulator)
    if (HAVE_PYTHON_NWP_EMULATOR_INTERFACE)
        add_subdirectory(bindings)
        add_subdirectory(pynwp_emulator)
    endif()
endif()
