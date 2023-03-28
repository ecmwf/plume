# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# plume C-API
ecbuild_add_library(

    TARGET plume

    INSTALL_HEADERS LISTED

    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}/plume/api

    SOURCES
        plume.h
        plume.cc

    INSTALL_HEADERS_LIST
        plume.h

    PUBLIC_INCLUDES
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    
    PUBLIC_LIBS
        eckit
        atlas
        plume_plugin_manager
)
install( FILES ${CMAKE_CURRENT_SOURCE_DIR}/plume.h DESTINATION include/plume/plume.h )



# plume Fortran-API
ecbuild_add_library(

    TARGET plume_f

    INSTALL_HEADERS LISTED

    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}/plume/api

    SOURCES
        plume.F90
        plume_lib.F90
        plume_utils.F90
        plume_data.F90
        plume_manager.F90
        plume_protocol.F90

    PUBLIC_INCLUDES
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_Fortran_MODULE_DIRECTORY}>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:module/plume>

    PUBLIC_LIBS
        eckit
        atlas
        fckit
        atlas_f
        plume
)

install( DIRECTORY ${CMAKE_Fortran_MODULE_DIRECTORY}/${CMAKE_CFG_INTDIR}
         DESTINATION module/plume
         COMPONENT modules )

