# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# Stage the pynwp_emulator Python package into the shared staging directory so
# that tests can set PYTHONPATH=${PYNWP_EMULATOR_STAGING} and do
# `import pynwp_emulator` without an install step.
#
# A symlink is used (rather than a copy) so that edits to .py files in the
# source tree are immediately visible in the staging directory without
# re-running CMake.
#
# The staging layout:
#   <staging>/
#     pynwp_emulator/          <- symlink to src/pynwp_emulator/
#     nwp_emulator_bindings/   <- .so files output by src/bindings/

file(
    CREATE_LINK
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PYNWP_EMULATOR_STAGING}/pynwp_emulator
    SYMBOLIC
)
