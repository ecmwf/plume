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
                  SOURCES
                    ManagerTestAccess.h
                    test_manager.cc
                  ENVIRONMENT
                    DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/lib
                  LIBS
                    plume_plugin_manager
                    eckit
)

ecbuild_add_test( TARGET   plume_test_manager_json
                  SOURCES  test_manager_json.cc
                  ENVIRONMENT
                    DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/lib
                  LIBS
                    plume_plugin_manager
                    eckit
)

# simple plugins (used for testing)
ecbuild_add_library( TARGET simple_plugins
  SOURCES 
    plugins/simple_plugin.h
    plugins/simple_plugin.cc
    plugins/simple_atlas_plugin.h
    plugins/simple_atlas_plugin.cc
    plugins/writeback_plugin.h
    plugins/writeback_plugin.cc
    plugins/skipwrite_plugin.cc
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

ecbuild_add_test( TARGET   plume_test_model_data_view_enforcement
                  SOURCES  test_model_data_view_enforcement.cc
                  LIBS
                    plume_plugin_manager
                    plume_plugin
)

ecbuild_add_test( TARGET   plume_test_atlas_model_data
                  SOURCES  test_atlas_model_data.cc
                  LIBS                    
                    plume_plugin_manager
                    plume_plugin       
)

ecbuild_add_test( TARGET   plume_test_update_strategies
                  SOURCES  test_update_strategies.cc
                  LIBS                    
                    plume_plugin_manager
                    plume_plugin       
)

ecbuild_add_test( TARGET   plume_test_parameter
                  SOURCES  test_parameter.cc
                  LIBS                    
                    plume_plugin_manager
)

ecbuild_add_test( TARGET   plume_test_negotiator
                  SOURCES  test_negotiator.cc
                  LIBS                    
                    plume_plugin_manager
)

ecbuild_add_test( TARGET   plume_test_configs
                  SOURCES  test_configs.cc
                  LIBS                    
                    plume_plugin_manager
)


ecbuild_add_test( TARGET   plume_test_writeback_ledger
                  SOURCES
                    ManagerTestAccess.h
                    test_writeback_ledger.cc
                  LIBS
                    plume_plugin_manager
                    eckit
)

ecbuild_add_test( TARGET   plume_test_plugin_writeback
                  SOURCES
                    ManagerTestAccess.h
                    test_plugin_writeback.cc
                  ENVIRONMENT
                    DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/lib
                  LIBS
                    plume_plugin_manager
                    eckit
)

ecbuild_add_test( TARGET   plume_test_writeback_atlas
                  SOURCES
                    ManagerTestAccess.h
                    test_writeback_atlas.cc
                  LIBS
                    plume_plugin_manager
                    plume_plugin
                    eckit
)

ecbuild_add_test( TARGET   plume_test_writeback_scope
                  SOURCES
                    ManagerTestAccess.h
                    test_writeback_scope.cc
                  LIBS
                    plume_plugin_manager
                    plume_plugin
                    eckit
)


ecbuild_add_test( TARGET   plume_test_plugin_params
                  SOURCES
                    ManagerTestAccess.h
                    test_plugin_params.cc
                  LIBS
                    plume_plugin_manager
                    eckit
)


# ----------------------------------------------------------------------------------------------------------------------
# Compile-time enforcement: the value-based writeParam(name, value) overload must REJECT FieldView and
# FieldWriter (access handles, not values) via its in-body static_assert. That rejection is a hard error.
# ----------------------------------------------------------------------------------------------------------------------
ecbuild_add_executable( TARGET       plume_should_not_compile_writeparam_fieldview
                        SOURCES      should_not_compile_writeparam.cc
                        DEFINITIONS  CHECK_FIELDVIEW
                        LIBS         plume_plugin
                        NOINSTALL )
set_target_properties( plume_should_not_compile_writeparam_fieldview
                       PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE )

ecbuild_add_test( TARGET          plume_test_writeparam_rejects_fieldview
                  COMMAND         ${CMAKE_COMMAND}
                  ARGS            --build ${CMAKE_BINARY_DIR}
                                  --target plume_should_not_compile_writeparam_fieldview
                                  --config $<CONFIG>
                  TEST_PROPERTIES WILL_FAIL TRUE )

ecbuild_add_executable( TARGET       plume_should_not_compile_writeparam_fieldwriter
                        SOURCES      should_not_compile_writeparam.cc
                        DEFINITIONS  CHECK_FIELDWRITER
                        LIBS         plume_plugin
                        NOINSTALL )
set_target_properties( plume_should_not_compile_writeparam_fieldwriter
                       PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE )

ecbuild_add_test( TARGET          plume_test_writeparam_rejects_fieldwriter
                  COMMAND         ${CMAKE_COMMAND}
                  ARGS            --build ${CMAKE_BINARY_DIR}
                                  --target plume_should_not_compile_writeparam_fieldwriter
                                  --config $<CONFIG>
                  TEST_PROPERTIES WILL_FAIL TRUE )
