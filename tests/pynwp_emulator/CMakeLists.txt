# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

set(PYNWP_EMULATOR_PYTHONPATH
    "${PYNWP_EMULATOR_STAGING}:$ENV{PYTHONPATH}"
)

#prevent findlibs in pynwp_emulator to pickup any globally installed plume
set(PYNWP_EMULATOR_FINDLIBS_ENV
    "PLUME_NWP_EMULATOR_DP_DIR=${CMAKE_BINARY_DIR}"
    "PLUME_NWP_EMULATOR_SP_DIR=${CMAKE_BINARY_DIR}"
    "FINDLIBS_DISABLE_PACKAGE=yes"
)

# Binding surface tests — no test data required.
ecbuild_add_test(
    TARGET      plume_test_pynwp_emulator
    TYPE        SCRIPT
    COMMAND     ${Python_EXECUTABLE}
    ARGS        -m pytest -vv -s
                ${CMAKE_CURRENT_SOURCE_DIR}/test_nwp_emulator.py
    ENVIRONMENT
        PYTHONPATH=${PYNWP_EMULATOR_PYTHONPATH}
        ${PYNWP_EMULATOR_FINDLIBS_ENV}
    LABELS      pynwp_emulator
)

# Integration tests — require the CONFIG data source from the nwp_emulator
# test data directory.  Uses a single process (no MPI).
ecbuild_add_test(
    TARGET      plume_test_pynwp_emulator_integration
    TYPE        SCRIPT
    COMMAND     ${Python_EXECUTABLE}
    ARGS        -m pytest -vv -s
                ${CMAKE_CURRENT_SOURCE_DIR}/test_nwp_emulator_integration.py
    ENVIRONMENT
        PYTHONPATH=${PYNWP_EMULATOR_PYTHONPATH}
        TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../nwp_emulator/data/
        ${PYNWP_EMULATOR_FINDLIBS_ENV}
    LABELS      pynwp_emulator pynwp_emulator_integration
)

# MPI integration tests — MPI_NPROCS is forwarded so the Python tests can assert snapshot.nprocs.
ecbuild_add_test(
    TARGET      plume_test_pynwp_emulator_mpi
    TYPE        SCRIPT
    COMMAND     ${Python_EXECUTABLE}
    ARGS        -m pytest -vv -s
                ${CMAKE_CURRENT_SOURCE_DIR}/test_nwp_emulator_mpi.py
    ENVIRONMENT
        PYTHONPATH=${PYNWP_EMULATOR_PYTHONPATH}
        TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../nwp_emulator/data/
        MPI_NPROCS=3
        ${PYNWP_EMULATOR_FINDLIBS_ENV}
    MPI         3
    CONDITION   eckit_HAVE_MPI
    LABELS      pynwp_emulator pynwp_emulator_mpi
)
