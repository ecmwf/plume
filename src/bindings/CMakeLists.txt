# (C) Copyright 2025- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# Precision is a C++ implementation detail that does not surface in the Python API
# (all Python-facing types use double regardless of internal field storage).
# Build a single nwp_emulator_bindings module linked to the best available precision:
# double-precision is preferred; single-precision is the fallback.
if(HAVE_NWP_EMULATOR_dp)
    set(_NWP_EMULATOR_BINDINGS_PREC dp)
elseif(HAVE_NWP_EMULATOR_sp)
    set(_NWP_EMULATOR_BINDINGS_PREC sp)
endif()

if(_NWP_EMULATOR_BINDINGS_PREC)
    ecbuild_info("Building nwp_emulator_bindings using ${_NWP_EMULATOR_BINDINGS_PREC} precision (plume_nwp_emulator_${_NWP_EMULATOR_BINDINGS_PREC})")
    pybind11_add_module(nwp_emulator_bindings MODULE nwp_emulator.cc)

    target_link_libraries(nwp_emulator_bindings
        PRIVATE
            pybind11::module
            pybind11::lto
            plume_nwp_emulator_${_NWP_EMULATOR_BINDINGS_PREC}
    )

    target_include_directories(nwp_emulator_bindings
        PRIVATE ${PROJECT_SOURCE_DIR}/src/nwp_emulator
    )

    target_compile_definitions(nwp_emulator_bindings
        PRIVATE
            ${NWP_EMULATOR_DEFINITIONS_${_NWP_EMULATOR_BINDINGS_PREC}}
    )

    if(NOT MSVC AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug|RelWithDebInfo")
        pybind11_strip(nwp_emulator_bindings)
    endif()

    # Stage the .so inside the nwp_emulator_bindings/ namespace package directory
    # so Python can import it as `nwp_emulator_bindings.nwp_emulator_bindings`.
    set_target_properties(nwp_emulator_bindings
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${PYNWP_EMULATOR_STAGING}/nwp_emulator_bindings
    )
else()
    ecbuild_warn("PYTHON_NWP_EMULATOR_INTERFACE is enabled but no NWP emulator precision is available "
                 "(neither NWP_EMULATOR_DOUBLE_PRECISION nor NWP_EMULATOR_SINGLE_PRECISION is ON). "
                 "nwp_emulator_bindings will not be built.")
endif()

