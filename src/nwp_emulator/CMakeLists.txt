# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
foreach(prec sp dp)
    if (HAVE_NWP_EMULATOR_${prec})
        set(NWP_EMULATOR_LIB_NAME "plume_nwp_emulator_${prec}")
        message(STATUS "Building ${NWP_EMULATOR_LIB_NAME} and definitions: ${NWP_EMULATOR_DEFINITIONS_${prec}}")

        ecbuild_add_library(
            TARGET ${NWP_EMULATOR_LIB_NAME}
            INSTALL_HEADERS LISTED
            HEADER_DESTINATION
                ${INSTALL_INCLUDE_DIR}/nwp_emulator_${prec}
            SOURCES
                data_reader.h
                grib_file_reader.h
                config_reader.h
                nwp_definitions.h
                nwp_data_provider.h
                nwp_emulator_core.h
                nwp_data_provider.cc
                nwp_emulator_core.cc
                grib_file_reader.cc
                config_reader.cc
                config_reader_funcs.cc
            PUBLIC_INCLUDES
                $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
                $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
            PRIVATE_INCLUDES
                "${MPI_INCLUDE_DIRS}"
            DEFINITIONS
                ${NWP_EMULATOR_DEFINITIONS_${prec}}
            PUBLIC_LIBS
                plume_plugin
                plume_plugin_manager
                eccodes
                eckit
        )

        ecbuild_add_executable(
            TARGET nwp_emulator_run_${prec}.x
            SOURCES
                nwp_emulator.cc
            LIBS
                ${NWP_EMULATOR_LIB_NAME}
        )
    endif()
endforeach()


