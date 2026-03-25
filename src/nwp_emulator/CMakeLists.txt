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

if (HAVE_NWP_EMULATOR_UI)

    set(NWP_EMULATOR_UI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
    set(NWP_EMULATOR_UI_BUILD_BIN_DIR ${PROJECT_BINARY_DIR}/bin)

    # Cleanup legacy top-level symlinks from older configurations.
    # Ideally logos would be fetched from the logos repository instead of living in the assets
    foreach(legacy_path IN ITEMS assets js backend)
        if (IS_SYMLINK ${NWP_EMULATOR_UI_BUILD_BIN_DIR}/${legacy_path})
            file(REMOVE ${NWP_EMULATOR_UI_BUILD_BIN_DIR}/${legacy_path})
        endif()
    endforeach()

    file(GLOB_RECURSE NWP_EMULATOR_UI_FILES
        RELATIVE ${NWP_EMULATOR_UI_SOURCE_DIR}
        LIST_DIRECTORIES FALSE
        ${NWP_EMULATOR_UI_SOURCE_DIR}/*)

    foreach(ui_file IN LISTS NWP_EMULATOR_UI_FILES)
        get_filename_component(ui_subdir ${ui_file} DIRECTORY)
        if (ui_subdir)
            file(MAKE_DIRECTORY ${NWP_EMULATOR_UI_BUILD_BIN_DIR}/${ui_subdir})
        endif()

        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${NWP_EMULATOR_UI_SOURCE_DIR}/${ui_file}
            ${NWP_EMULATOR_UI_BUILD_BIN_DIR}/${ui_file})
    endforeach()
endif()


