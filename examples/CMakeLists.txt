# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

# ------------- example plugins --------------
ecbuild_add_library( TARGET plugin_foo
  SOURCES 
    plugin_foo.h
    plugin_foo.cc
  PRIVATE_LIBS
    plume_plugin
)

ecbuild_add_library( TARGET plugin_bar
  SOURCES 
    plugin_bar.h
    plugin_bar.cc
  PRIVATE_LIBS
    plume_plugin
)


# --------- script to run examples ----------
ecbuild_configure_file(run_examples.sh.in
                       ${CMAKE_BINARY_DIR}/bin/run_plume_examples.sh
)


# ================ Example 1 ====================
ecbuild_add_executable( TARGET plume_example1_c.x
  SOURCES example1.c
  LIBS
    plume
)

ecbuild_add_executable( TARGET plume_example1_cpp.x
  SOURCES example1.cc
  LIBS
    atlas
    plume_plugin_manager
)

ecbuild_add_executable( TARGET plume_example1_fort.x
  SOURCES example1.F90
  LIBS
    atlas
    plume_f
)


# ================ Example 2 ====================
ecbuild_add_executable( TARGET plume_example2_cpp.x
  SOURCES example2.cc
  LIBS
    atlas
    plume_plugin_manager
)

ecbuild_add_executable( TARGET plume_example2_fort.x
  SOURCES example2.F90
  LIBS
    atlas
    plume_f
)



# ================ Example 3 ====================
ecbuild_add_executable( TARGET plume_example3_cpp.x
  SOURCES example3.cc
  LIBS
    atlas
    plume_plugin_manager
)

ecbuild_add_executable( TARGET plume_example3_fort.x
  SOURCES example3.F90
  LIBS
    atlas
    plume_f
)
