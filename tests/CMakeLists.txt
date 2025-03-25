# (C) Copyright 2023- ECMWF.
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
#
# In applying this licence, ECMWF does not waive the privileges and immunities
# granted to it by virtue of its status as an intergovernmental organisation nor
# does it submit to any jurisdiction.

if( NOT DEFINED MPI_SLOTS )
  set( MPI_SLOTS 9999 )
endif()

add_subdirectory(core)
add_subdirectory(api)
add_subdirectory(nwp_emulator)