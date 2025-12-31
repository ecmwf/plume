# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# ===============================================================================
# 
# plume_plugin_interface
# ======================
#
# Autogenerate interface files for Fortran plugins.
#
# ===============================================================================
function( plume_plugin_interface )

    set( options )

    set(single_value_args
        PLUGIN_TEMPLATE
        PLUGIN_NAME
        PLUGIN_VERSION
        PLUGIN_SHA
        PLUGINCORE_NAME
        PLUGIN_PRECISION
    )

    set(multi_value_args
        PLUGIN_REQUIRED_PARAMS
    )

    cmake_parse_arguments( _PAR "${options}" "${single_value_args}" "${multi_value_args}"  ${_FIRST_ARG} ${ARGN} )

    if(_PAR_UNPARSED_ARGUMENTS)
        message("Unknown keywords given to plume_plugin_interface(): \"${_PAR_UNPARSED_ARGUMENTS}\"")
    endif()

    set(PLUGIN_NAME ${_PAR_PLUGIN_NAME})
    set(PLUGIN_VERSION ${_PAR_PLUGIN_VERSION})
    set(PLUGIN_SHA ${_PAR_PLUGIN_SHA})
    set(PLUGINCORE_NAME ${_PAR_PLUGINCORE_NAME})
    set(PLUGIN_REQUIRED_PARAMS ${_PAR_PLUGIN_REQUIRED_PARAMS})
    set(PLUGIN_PRECISION ${_PAR_PLUGIN_PRECISION})

    # list of required parameters
    set(REQUIRED_PARAM_LIST "")
    foreach(param ${PLUGIN_REQUIRED_PARAMS})
        string(REPLACE ":" ";" param_name_type ${param})
        list(GET param_name_type 0 param_name)
        list(GET param_name_type 1 param_type)
        message("param_name ${param_name}, param_type ${param_type}")
        if(param_type STREQUAL "INT")
            set(REQUIRED_PARAM_LIST "${REQUIRED_PARAM_LIST} protocol.require<int>(\"${param_name}\");\n")
        elseif(param_type STREQUAL "BOOL")
            set(REQUIRED_PARAM_LIST "${REQUIRED_PARAM_LIST} protocol.require<bool>(\"${param_name}\");\n")
        elseif(param_type STREQUAL "FLOAT")
            set(REQUIRED_PARAM_LIST "${REQUIRED_PARAM_LIST} protocol.require<float>(\"${param_name}\");\n")
        elseif(param_type STREQUAL "DOUBLE")
            set(REQUIRED_PARAM_LIST "${REQUIRED_PARAM_LIST} protocol.require<double>(\"${param_name}\");\n")
        elseif(param_type STREQUAL "ATLAS_FIELD")
            set(REQUIRED_PARAM_LIST "${REQUIRED_PARAM_LIST} protocol.require<atlas::Field>(\"${param_name}\");\n")
        endif()
    endforeach()

    get_filename_component( PLUGIN_TEMPLATE_FILENAME ${_PAR_PLUGIN_TEMPLATE} NAME)
    get_filename_component( GENERATED_USER_SOURCE_FILE ${_PAR_PLUGIN_TEMPLATE} NAME_WLE)
    get_filename_component( GENERATED_USER_SOURCE_FILE_WEXT ${GENERATED_USER_SOURCE_FILE} NAME_WLE)

    message(" ==>> GENERATED_USER_SOURCE_FILE: ${GENERATED_USER_SOURCE_FILE}")
    message("Plugin: ${PLUGIN_NAME}; PluginCore {PLUGINCORE_NAME}; Source: ${PLUGIN_TEMPLATE_FILENAME}")

    # define finename suffix _PREC_SUFFIX if precision is set
    if (PLUGIN_PRECISION)
        set(PREC_SUFFIX "_${PLUGIN_PRECISION}")
    else()
        set(PREC_SUFFIX "")
    endif()

    SET(CONFIGURED_PLUGIN_FILENAME ${GENERATED_USER_SOURCE_FILE_WEXT}${PREC_SUFFIX}.F90)

    # pre-process user source-template
    ecbuild_configure_file(
        ${_PAR_PLUGIN_TEMPLATE}
        ${CONFIGURED_PLUGIN_FILENAME} @ONLY
    )

    # pre-process interface files
    ecbuild_configure_file(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fortran_plugin.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.h @ONLY
    )

    ecbuild_configure_file(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fortran_plugin.cc.in
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.cc @ONLY
    )

    ecbuild_configure_file(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/fortran_plugin.F90.in
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.F90 @ONLY
    )

    # set source files to be included 
    set(INTERFACE_PLUGIN_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.h
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.cc
        ${CMAKE_CURRENT_BINARY_DIR}/fortran_plugin${PREC_SUFFIX}.F90
        ${CONFIGURED_PLUGIN_FILENAME}
        PARENT_SCOPE
    )

endfunction()