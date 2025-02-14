# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.
ecbuild_add_library(
    TARGET plume_nwp_emulator
    INSTALL_HEADERS LISTED
    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}/nwp_emulator
    SOURCES
        base_data_reader.h
        grib_file_reader.h
        nwp_definitions.h
        nwp_data_provider.h
        nwp_data_provider.cc
        grib_file_reader.cc
    PUBLIC_INCLUDES
       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    PRIVATE_INCLUDES
        "${MPI_INCLUDE_DIRS}"
    DEFINITIONS
        ${NWP_EMULATOR_DEFINITIONS}
    PUBLIC_LIBS
        plume_plugin
        plume_plugin_manager
        eccodes
        eckit
)

ecbuild_add_executable( TARGET nwp_emulator_run.x
  SOURCES
    nwp_emulator.cc
  LIBS
    plume_nwp_emulator
)
