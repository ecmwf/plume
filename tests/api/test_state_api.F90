! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_state_fapi_suite )

! -----------------------------------------------------------------------
! Tests the standalone plume_state_* API (plume_state_module), which
! queries the global PlumeState singleton independently of the manager
! handle.  The manager is used only to drive state transitions.
! -----------------------------------------------------------------------

TEST( test_state_fapi_lifecycle )
use iso_c_binding, only : c_null_char, c_int64_t
use plume_module
use plume_tags_module, only : PLUME_TAG_RUN
implicit none

type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

character(len=2000) :: mgr_conf_str

integer :: param_i = 1
integer :: param_j = 2

integer :: iter
integer(c_int64_t) :: state_iter
integer(c_int64_t) :: state_iter_rel
character(:), allocatable :: state_name

call plume_check(plume_initialise())

! ----- offers -----
call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "param I"))
call plume_check(offers%offer_int("J", "always", "param J"))

mgr_conf_str = '{' // &
'"plugins": [ ' // &
'    { ' // &
'        "lib": "plume_plugin_test_api", ' // &
'        "name": "PluginTestAPI", ' // &
'        "parameters": [ ' // &
'            [ ' // &
'                {"name":"I", "type":"INT"}, ' // &
'                {"name":"J", "type":"INT"} ' // &
'            ] ' // &
'        ], ' // &
'        "core-config": {} ' // &
'    } ' // &
'] ' // &
'}' // c_null_char

! ----- configure -----
call plume_check(manager%initialise())
call plume_check(manager%configure_from_string(trim(mgr_conf_str)))

state_name = plume_state_current_name()
call plume_check(plume_state_current_iteration(state_iter))
call plume_check(plume_state_current_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "configure")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

! ----- negotiate -----
call plume_check(manager%negotiate(offers))

state_name = plume_state_current_name()
call plume_check(plume_state_current_iteration(state_iter))
call plume_check(plume_state_current_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "negotiate")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

! ----- feed plugins -----
call plume_check(data%initialise())
call plume_check(data%provide_int("I", param_i))
call plume_check(data%provide_int("J", param_j))
call plume_check(manager%feed_plugins(data))

state_name = plume_state_current_name()
call plume_check(plume_state_current_iteration(state_iter))
call plume_check(plume_state_current_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "feed_plugins")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

! ----- run (3 iterations) -----
do iter = 1, 3
    call plume_check(manager%run(PLUME_TAG_RUN))

    state_name = plume_state_current_name()
    call plume_check(plume_state_current_iteration(state_iter))
    call plume_check(plume_state_current_iteration_rel(state_iter_rel))
    FCTEST_CHECK(trim(state_name) == "run")
    FCTEST_CHECK(state_iter == int(iter, c_int64_t))
    FCTEST_CHECK(state_iter_rel == int(iter, c_int64_t))
enddo

! ----- teardown -----
call plume_check(manager%finalise())

state_name = plume_state_current_name()
call plume_check(plume_state_current_iteration(state_iter))
call plume_check(plume_state_current_iteration_rel(state_iter_rel))
FCTEST_CHECK(trim(state_name) == "teardown")
FCTEST_CHECK(state_iter == 1)
FCTEST_CHECK(state_iter_rel == 1)

call plume_check(offers%finalise())
call plume_check(data%finalise())
call plume_check(plume_finalise())

END_TEST


TEST( test_state_fapi_custom_tags )
use iso_c_binding, only : c_null_char, c_int64_t
use plume_module
use plume_tags_module
implicit none

type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

character(len=2000) :: mgr_conf_str

integer :: param_i = 1
integer :: param_j = 2

integer :: iter
integer(c_int64_t) :: state_iter
integer(c_int64_t) :: state_iter_rel
character(:), allocatable :: state_name

call plume_check(plume_initialise())

call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "param I"))
call plume_check(offers%offer_int("J", "always", "param J"))

mgr_conf_str = '{' // &
'"plugins": [ ' // &
'    { ' // &
'        "lib": "plume_plugin_test_api", ' // &
'        "name": "PluginTestAPI", ' // &
'        "parameters": [ ' // &
'            [ ' // &
'                {"name":"I", "type":"INT"}, ' // &
'                {"name":"J", "type":"INT"} ' // &
'            ] ' // &
'        ], ' // &
'        "core-config": {} ' // &
'    } ' // &
'] ' // &
'}' // c_null_char

call plume_check(manager%initialise())
call plume_check(manager%configure_from_string(trim(mgr_conf_str)))

state_name = plume_state_current_name()
FCTEST_CHECK(trim(state_name) == "configure")

call plume_check(manager%negotiate(offers))

state_name = plume_state_current_name()
FCTEST_CHECK(trim(state_name) == "negotiate")

call plume_check(data%initialise())
call plume_check(data%provide_int("I", param_i))
call plume_check(data%provide_int("J", param_j))
call plume_check(manager%feed_plugins(data))

state_name = plume_state_current_name()
FCTEST_CHECK(trim(state_name) == "feed_plugins")

! ----- nested run: outer (2 iterations) / inner (3 iterations each) -----
do iter = 1, 2
    call plume_check(manager%run(PLUME_TAG_RUN_LVL1))

    state_name = plume_state_current_name()
    call plume_check(plume_state_current_iteration(state_iter))
    call plume_check(plume_state_current_iteration_rel(state_iter_rel))
    FCTEST_CHECK(trim(state_name) == "run_lvl1")
    FCTEST_CHECK(state_iter == int(iter, c_int64_t))
    FCTEST_CHECK(state_iter_rel == int(iter, c_int64_t))

    ! inner run -- relative iteration resets each time outer advances
    call plume_check(manager%run(PLUME_TAG_RUN_LVL2, PLUME_TAG_RUN_LVL1))
    state_name = plume_state_current_name()
    call plume_check(plume_state_current_iteration(state_iter))
    call plume_check(plume_state_current_iteration_rel(state_iter_rel))
    FCTEST_CHECK(trim(state_name) == "run_lvl2")
    FCTEST_CHECK(state_iter == int(iter, c_int64_t))   ! cumulative
    FCTEST_CHECK(state_iter_rel == 1)                  ! reset by parent advance
enddo

call plume_check(manager%finalise())

state_name = plume_state_current_name()
FCTEST_CHECK(trim(state_name) == "teardown")

call plume_check(offers%finalise())
call plume_check(data%finalise())
call plume_check(plume_finalise())

END_TEST


END_TESTSUITE
