# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# built time configs and version
ecbuild_configure_file( plume_config.h.in   plume_config.h  )
ecbuild_configure_file( plume_version.h.in  plume_version.h )
ecbuild_configure_file( plume_version.cc.in  plume_version.cc )

install(FILES
			${CMAKE_CURRENT_BINARY_DIR}/plume_config.h
			${CMAKE_CURRENT_BINARY_DIR}/plume_version.h
		DESTINATION
			${INSTALL_INCLUDE_DIR}/plume )



# #################### Plume plugin ######################
set(PLUGIN_FILES_H    
    Plugin.h
    PluginHandler.h
    Protocol.h
    PluginCore.h
    Configurable.h
    data/ModelData.h
    data/ParameterCatalogue.h
    data/Parameter.h
    data/DataChecker.h
)

set(PLUGIN_FILES_CC
    Plugin.cc
    PluginHandler.cc
    Protocol.cc   
    PluginCore.cc
    Configurable.cc
    data/ModelData.cc
    data/ParameterCatalogue.cc
    data/Parameter.cc
    data/DataChecker.cc
)

set(PLUME_PLUGIN_SOURCES
    ${PLUGIN_FILES_H}
    ${PLUGIN_FILES_CC}
)

ecbuild_add_library(
    TARGET plume_plugin
    INSTALL_HEADERS LISTED
    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}
    SOURCES
        ${PLUME_PLUGIN_SOURCES}
    PUBLIC_INCLUDES
       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    PRIVATE_INCLUDES
        "${MPI_INCLUDE_DIRS}"
    PUBLIC_LIBS
        atlas
        eckit
)

install( FILES ${PLUGIN_FILES_H} DESTINATION include )

# #################### PLUME plugin manager ######################
set(PLUME_PLUGIN_MANAGER_SOURCES
    plume_config.h
    plume_version.h
    plume_version.cc
    Manager.h
    Manager.cc
    Protocol.h
    Protocol.cc
    Configurable.h
    Configurable.cc
    data/ModelData.h
    data/ModelData.cc
    data/ParameterCatalogue.h
    data/ParameterCatalogue.cc
    data/Parameter.h
    data/Parameter.cc
    data/DataChecker.h
    data/DataChecker.cc
)

ecbuild_add_library(
    TARGET plume_plugin_manager
    INSTALL_HEADERS LISTED
    HEADER_DESTINATION
        ${INSTALL_INCLUDE_DIR}
    SOURCES
        ${PLUME_PLUGIN_MANAGER_SOURCES}
    PUBLIC_INCLUDES
       $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
       $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    PRIVATE_INCLUDES
        "${MPI_INCLUDE_DIRS}"
    PUBLIC_LIBS        
        atlas
        eckit
    PRIVATE_LIBS        
        plume_plugin
)

# plume API
add_subdirectory(api)
