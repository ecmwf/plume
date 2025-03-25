# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
# Test files repository on Nexus
set(ECBUILD_DOWNLOAD_BASE_URL https://get.ecmwf.int/repository/plume-test-data)

set(test_nwp_emulator_files_dir ${CMAKE_BINARY_DIR}/tests/nwp_emulator/data)
add_custom_target(
    make_nwp_emulator_test_data_dir ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${test_nwp_emulator_files_dir}
)

ecbuild_get_test_multidata(
    TARGET test_nwp_emulator_files
    NAMES
        model_data_1.grib
        model_data_2.grib
    DIRNAME
        nwp_emulator
    DIRLOCAL
        ${test_nwp_emulator_files_dir}
    NOCHECK
)

ecbuild_add_test(
    TARGET      plume_test_nwp_grib
    SOURCES     test_grib_reader.cc
    LIBS        plume_nwp_emulator
    ENVIRONMENT TEST_DATA_DIR=${test_nwp_emulator_files_dir}
    MPI         3
    CONDITION   eckit_HAVE_MPI
    TEST_DEPENDS test_nwp_emulator_files
)

ecbuild_add_test(
    TARGET      plume_test_nwp_config
    SOURCES     test_config_reader.cc
    LIBS        plume_nwp_emulator
    ENVIRONMENT TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/
    MPI         3
    CONDITION   eckit_HAVE_MPI
)

ecbuild_add_library(
    TARGET nwp_emulator_test_plugin
    SOURCES 
        nwp_emulator_plugin.h
        nwp_emulator_plugin.cc
    PRIVATE_LIBS
        plume_plugin
)

ecbuild_add_test(
    TARGET  plume_test_nwp_tool
    LIBS simple_plugin nwp_emulator_test_plugin
    COMMAND nwp_emulator_run.x
    ARGS --config-src=${CMAKE_CURRENT_SOURCE_DIR}/data/valid_config.yml
         --plume-cfg=${CMAKE_CURRENT_SOURCE_DIR}/data/plume_config.yml
)