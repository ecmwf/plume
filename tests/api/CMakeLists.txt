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

# Fortran API tests
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
