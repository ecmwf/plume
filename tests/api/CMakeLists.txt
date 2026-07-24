# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.


# C++ plugin used for testing
ecbuild_add_library( TARGET plume_plugin_test_api
  SOURCES 
    plugin_test_api.h
    plugin_test_api.cc
  PRIVATE_LIBS
    plume_plugin
)

# Fortran plugin used for testing
ecbuild_add_library( TARGET plume_plugin_test_fapi
  SOURCES 
    plugin_test_fapi.h
    plugin_test_fapi.cc
    plugin_test_fapi.F90

  PRIVATE_LIBS
    plume_plugin
    plume_f
)

# C++ write-back plugin for API testing
ecbuild_add_library( TARGET plume_plugin_writeback_api
  SOURCES
    plugin_writeback_api.h
    plugin_writeback_api.cc
  PRIVATE_LIBS
    plume_plugin
)

# Fortran write-back plugin for API testing
ecbuild_add_library( TARGET plume_plugin_writeback_fapi
  SOURCES
    plugin_writeback_fapi.h
    plugin_writeback_fapi.cc
    plugin_writeback_fapi.F90
  PRIVATE_LIBS
    plume_plugin
    plume_f
)

# C API tests
ecbuild_add_test( TARGET   plume_test_data_api
                  SOURCES
                    test_api_utils.h
                    test_data_api.cc
                  LIBS
                    plume
)

ecbuild_add_test( TARGET   plume_test_lib_api
                  SOURCES
                    test_api_utils.h
                    test_lib_api.cc
                  LIBS
                    plume
)

ecbuild_add_test( TARGET   plume_test_manager_api
                  SOURCES
                    test_api_utils.h
                    test_manager_api.cc
                  LIBS
                    plume
)

ecbuild_add_test( TARGET   plume_test_params_api
                  SOURCES
                    test_api_utils.h
                    test_params_api.cc
                  LIBS
                    plume
)

ecbuild_add_test( TARGET   plume_test_writeback_api
                  SOURCES
                    test_api_utils.h
                    test_writeback_api.cc
                  LIBS
                    plume
)

# Fortran API tests
add_fctest( TARGET   plume_test_data_fapi
            SOURCES  test_data_api.F90
            LIBS
              plume_f
)

add_fctest( TARGET   plume_test_lib_fapi
            SOURCES  test_lib_api.F90
            LIBS
              plume_f
)

add_fctest( TARGET   plume_test_manager_fapi
            SOURCES  test_manager_api.F90
            LIBS
              plume_f
)

add_fctest( TARGET   plume_test_params_fapi
            SOURCES  test_params_api.F90
            LIBS
              plume_f
)

add_fctest( TARGET   plume_test_atlas_field_lifetime_fapi
            SOURCES  test_atlas_field_lifetime.F90
            LIBS
              plume_f
)

add_fctest( TARGET   plume_test_writeback_fapi
            SOURCES  test_writeback_fapi.F90
            LIBS
              plume_f
)


# ----------------------------------------------------------------------------------------------------------------------
# Compile-time enforcement: a Fortran plugin holds a plume_data_view and MUST NOT be able to call a model-only mutator.
# The target is EXCLUDE_FROM_ALL (so the normal build stays green) and the test below drives its compilation and asserts
# it FAILS (WILL_FAIL). If enforcement regresses, the target compiles and the test fails.
# ----------------------------------------------------------------------------------------------------------------------
ecbuild_add_executable( TARGET       plume_should_not_compile_fapi
                        SOURCES      should_not_compile_fapi.F90
                        LIBS         plume_f
                        NOINSTALL )
set_target_properties( plume_should_not_compile_fapi
                       PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE )

ecbuild_add_test( TARGET          plume_test_fapi_view_enforcement
                  COMMAND         ${CMAKE_COMMAND}
                  ARGS            --build ${CMAKE_BINARY_DIR}
                                  --target plume_should_not_compile_fapi
                                  --config $<CONFIG>
                  LABELS          fortran
                  TEST_PROPERTIES WILL_FAIL TRUE )
