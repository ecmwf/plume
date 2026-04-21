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
        ${INSTALL_INCLUDE_DIR}/plume

    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../TagDefinitions.def
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


set(PLUME_TAG_DEFINITIONS ${CMAKE_CURRENT_SOURCE_DIR}/../TagDefinitions.def)
set(PLUME_FORTRAN_TAGS_MODULE ${CMAKE_CURRENT_BINARY_DIR}/plume_tags.F90)

add_custom_command(
    OUTPUT ${PLUME_FORTRAN_TAGS_MODULE}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${PLUME_TAG_DEFINITIONS}
        -DOUTPUT=${PLUME_FORTRAN_TAGS_MODULE}
        -P ${PROJECT_SOURCE_DIR}/cmake/generate_plume_tags_fortran.cmake
    DEPENDS
        ${PLUME_TAG_DEFINITIONS}
        ${PROJECT_SOURCE_DIR}/cmake/generate_plume_tags_fortran.cmake
    COMMENT "Generating Fortran plume tag constants module"
    VERBATIM
)

add_custom_target(plume_generate_fortran_tags DEPENDS ${PLUME_FORTRAN_TAGS_MODULE})


# plume Fortran-API
ecbuild_add_library(

    TARGET plume_f

    INSTALL_HEADERS LISTED

    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}/plume

    SOURCES
        ${PLUME_FORTRAN_TAGS_MODULE}
        plume.F90
        plume_lib.F90
        plume_state.F90
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

    add_dependencies(plume_f plume_generate_fortran_tags)

install( DIRECTORY ${CMAKE_Fortran_MODULE_DIRECTORY}/${CMAKE_CFG_INTDIR}
         DESTINATION module/plume
         COMPONENT modules )

