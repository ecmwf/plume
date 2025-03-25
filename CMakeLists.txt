# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

find_package( ecbuild 3.5 REQUIRED HINTS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ecbuild)

project( plume LANGUAGES CXX )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

############################################################################################
### dependencies and options
ecbuild_find_package( NAME eckit  VERSION  1.18 REQUIRED )
ecbuild_find_package( NAME atlas )
ecbuild_find_package( NAME eccodes )

ecbuild_add_option( FEATURE BUILD_TOOLS
                    DEFAULT ON
                    DESCRIPTION "Build the command line tools" )

ecbuild_add_option( FEATURE FORTRAN
                    DESCRIPTION "Provide Fortran bindings" )
if(HAVE_FORTRAN)
    ecbuild_enable_fortran( REQUIRED MODULE_DIRECTORY ${PROJECT_BINARY_DIR}/module )
endif()

############## FCKIT
ecbuild_add_option( FEATURE FCKIT
                    DEFAULT ON
                    DESCRIPTION "Support for fckit"
                    REQUIRED_PACKAGES fckit)

############## MPI
ecbuild_add_option(DEFAULT OFF
                   FEATURE MPI
                   DESCRIPTION "Use system MPI libraries"
                   REQUIRED_PACKAGES "MPI COMPONENTS CXX C" )

ecbuild_info("FCKIT_FOUND ${fckit_FOUND}")
ecbuild_info("FCKIT_LIBRARIES ${FCKIT_LIBRARIES}")
ecbuild_info("FCKIT_INCLUDE_DIRS ${FCKIT_INCLUDE_DIRS}")

############## PRECISION
ecbuild_add_option( FEATURE NWP_EMULATOR_SINGLE_PRECISION
                    DESCRIPTION "Single precision Atlas fields"
                    DEFAULT OFF )

if( HAVE_NWP_EMULATOR_SINGLE_PRECISION ) 
  list(APPEND NWP_EMULATOR_DEFINITIONS WITH_NWP_EMULATOR_SINGLE_PRECISION )
endif()

add_subdirectory( src )
add_subdirectory( tests )
add_subdirectory( examples )

# copy cmake plugin interface macro into binary directory
file(
    COPY 
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/plume-plugin-interface.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fortran_plugin.h.in
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fortran_plugin.cc.in
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fortran_plugin.F90.in
     DESTINATION 
        ${PROJECT_BINARY_DIR}/cmake
)

# cmake macro for fortran interface plugins
set(plume_INTERFACE_PLUGIN_CMAKE_FILE ${PROJECT_BINARY_DIR}/cmake/plume-plugin-interface.cmake)

# finalize
ecbuild_install_project( NAME ${PROJECT_NAME} )
ecbuild_print_summary()

