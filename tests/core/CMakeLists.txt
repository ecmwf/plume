# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# plume tests
ecbuild_add_test( TARGET   plume_test_plugin
                  SOURCES  test_plugin.cc
                  LIBS
                    plume_plugin
                    eckit
)


ecbuild_add_test( TARGET   plume_test_plugincore
                  SOURCES  test_plugincore.cc
                  LIBS
                    plume_plugin_manager
                    plume_plugin
                    eckit
)

# -------------- test manager ---------------
ecbuild_add_test( TARGET   plume_test_manager
                  SOURCES  test_manager.cc
                  ENVIRONMENT
                    DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/lib
                  LIBS
                    plume_plugin_manager
                    eckit
)

# simple plugin (used for testing)
ecbuild_add_library( TARGET simple_plugin
  SOURCES 
    simple_plugin.h
    simple_plugin.cc
  PRIVATE_LIBS
    plume_plugin
)
# -------------------------------------------


ecbuild_add_test( TARGET   plume_test_protocol
                  SOURCES  test_protocol.cc
                  LIBS                    
                    plume_plugin_manager
                    plume_plugin       
)


ecbuild_add_test( TARGET   plume_test_catalogue
                  SOURCES  test_catalogue.cc
                  LIBS                    
                    plume_plugin_manager
                    plume_plugin       
)


ecbuild_add_test( TARGET   plume_test_model_data
                  SOURCES  test_model_data.cc
                  LIBS                    
                    plume_plugin_manager
                    plume_plugin       
)

ecbuild_add_test( TARGET   plume_test_parameter
                  SOURCES  test_parameter.cc
                  LIBS                    
                    plume_plugin_manager
)
