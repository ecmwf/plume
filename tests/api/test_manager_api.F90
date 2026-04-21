! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_manager_fapi_suite )

TEST( test_manager_fapi )
use iso_c_binding, only : c_int64_t
use atlas_module
use plume_module
use plume_tags_module, only : PLUME_TAG_RUN
use fckit_module
implicit none

character(1024) :: plume_config_path

! plume structures
type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

character(len=2000) :: mgr_conf_str

! data
integer :: param_i = 111
integer :: param_j = 222
real(c_float) :: param_ff1 = 333.3
real(c_double) :: param_dd1 = 444.4
type(atlas_Field) :: field

! params for fortran plugin
integer :: param_i_fort = 555
integer :: param_j_fort = 666
real(c_float) :: param_ff1_fort = 777.7
real(c_double) :: param_dd1_fort = 888.8

logical(c_bool) :: is_plugin_activated

integer :: iter
integer(c_int64_t) :: state_iter
integer(c_int64_t) :: state_iter_rel
character(:), allocatable :: state_name


call atlas_library%initialise()
call plume_check(plume_initialise())

! make offers
call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "this is param I"))
call plume_check(offers%offer_int("J", "always", "this is param J"))
call plume_check(offers%offer_float("FF1", "always", "this is param FF1"))
call plume_check(offers%offer_float("FF2", "always", "this is param FF2"))
call plume_check(offers%offer_double("DD1", "always", "this is param DD1"))
call plume_check(offers%offer_atlas_field("AA1", "always", "this is param AA1"))

! offer parameters for fortran plugin
! Note: parameters offered/used by the fortran plugin have their 
! name preprended with "FORT_" just for clarity, they are conceptually
! the same as the ones offered to the C++ plugin.
call plume_check(offers%offer_int("FORT_I", "always", "this is param FORT_I"))
call plume_check(offers%offer_int("FORT_J", "always", "this is param FORT_J"))
call plume_check(offers%offer_float("FORT_FF1", "always", "this is param FORT_FF1"))
call plume_check(offers%offer_double("FORT_DD1", "always", "this is param FORT_DD1"))


! negotiate
mgr_conf_str = '{' // &
'"plugins": [ ' // &
'    { ' // &
'        "lib": "plume_plugin_test_api", ' // &
'        "name": "PluginTestAPI", ' // &
'        "parameters": [ ' // &
'            [ ' // &
'                {"name":"I", "type":"INT"}, ' // &
'                {"name":"J", "type":"INT"}, ' // &
'                {"name":"FF1", "type":"FLOAT"}, ' // &
'                {"name":"DD1", "type":"DOUBLE"}, ' // &
'                {"name":"AA1", "type":"ATLAS_FIELD"} ' // &
'            ], ' // &
'            [ ' // &
'                {"name":"JJJ", "type":"INT"}, ' // &
'                {"name":"J", "type":"INT"}, ' // &
'                {"name":"KKMM", "type":"INT"} ' // &
'            ], ' // &
'            [ ' // &
'                {"name":"XYZ", "type":"INT"}, ' // &
'                {"name":"K", "type":"INT"} ' // &
'            ] ' // &
'        ], ' // &
'        "core-config": {} ' // &
'    }, ' // &
'    { ' // &
'        "lib": "plume_plugin_test_fapi", ' // &
'        "name": "PluginTestFAPI", ' // &
'        "parameters": [ ' // &
'            [ ' // &
'                {"name":"FORT_I", "type":"INT"}, ' // &
'                {"name":"FORT_J", "type":"INT"}, ' // &
'                {"name":"FORT_FF1", "type":"FLOAT"}, ' // &
'                {"name":"FORT_DD1", "type":"DOUBLE"} ' // &
'            ] ' // &
'        ], ' // &
'        "core-config": {} ' // &
'    } ' // &
'] ' // &
'}' //c_null_char


call plume_check(manager%initialise())
call plume_check(manager%configure_from_string(trim(mgr_conf_str)))


state_name = manager%current_state_name()
call plume_check(manager%current_state_iteration(state_iter))
call plume_check(manager%current_state_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "configure")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

call plume_check(manager%negotiate(offers))
state_name = manager%current_state_name()
call plume_check(manager%current_state_iteration(state_iter))
call plume_check(manager%current_state_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "negotiate")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

! check that the plugins are activated
is_plugin_activated = .false.
call plume_check(manager%is_plugin_activated("PluginTestAPI", is_plugin_activated))
FCTEST_CHECK(is_plugin_activated)

! check that the Fortran plugin is activated
call plume_check(manager%is_plugin_activated("PluginTestFAPI", is_plugin_activated))
FCTEST_CHECK(is_plugin_activated)

! check that a non-existent plugin is not activated
call plume_check(manager%is_plugin_activated("NonExistentPlugin", is_plugin_activated))
FCTEST_CHECK(.not. is_plugin_activated)


! data
field = atlas_Field("AA1", atlas_integer(), (/0,10/))
call plume_check(data%initialise())
call plume_check(data%provide_int("I", param_i) )
call plume_check(data%provide_int("J", param_j) )
call plume_check(data%provide_float("FF1", param_ff1) )
call plume_check(data%provide_double("DD1", param_dd1) )
call plume_check(data%provide_atlas_field_shared("AA1", field) )

! provide params for fortran plugin
call plume_check(data%provide_int("FORT_I", param_i) )
call plume_check(data%provide_int("FORT_J", param_j) )
call plume_check(data%provide_float("FORT_FF1", param_ff1) )
call plume_check(data%provide_double("FORT_DD1", param_dd1) )

! feed plugins
call plume_check(manager%feed_plugins(data))

state_name = manager%current_state_name()
call plume_check(manager%current_state_iteration(state_iter))
call plume_check(manager%current_state_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "feed_plugins")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

! run the model for 2 iterations
do iter=1,2
  call plume_check(manager%run(PLUME_TAG_RUN))
  state_name = manager%current_state_name()
  call plume_check(manager%current_state_iteration(state_iter))
  call plume_check(manager%current_state_iteration_rel(state_iter_rel))
  FCTEST_CHECK(trim(state_name) == "run")
  FCTEST_CHECK(state_iter == iter)
  FCTEST_CHECK(state_iter_rel == iter)
enddo

! finalise
call plume_check(manager%finalise())
call plume_check(offers%finalise())
call plume_check(data%finalise())

call plume_check(plume_finalise())
call atlas_library%finalise()

    
END_TEST

END_TESTSUITE