! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_params_fapi_suite )

TEST( test_params_fapi )
use atlas_module
use plume_module
use fckit_module
implicit none

character(1024) :: plume_config_path

! plume structures
type(plume_protocol) :: offers
type(plume_manager) :: manager
! type(plume_data) :: data

! data
integer :: param_i = 0
integer :: param_j = 10
integer :: param_k = 100
type(atlas_Field) :: field

integer :: config_param_1 = 9
real(c_double) :: config_param_2 = 99.99
type(atlas_Field) :: config_param_3
character(len=1000) :: mgr_conf_str

logical(c_bool) :: is_requested

character(len=6) :: param_names_requested(6)
character(len=6) :: param_names_rejected(5)
integer :: i_param


call atlas_library%initialise()
call plume_check(plume_initialise())

! make offers (read-only params)
call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "this is param I"))
call plume_check(offers%offer_int("J", "always", "this is param J"))
call plume_check(offers%offer_float("FF1", "always", "this is param FF1"))
call plume_check(offers%offer_double("DD1", "always", "this is param DD1"))
call plume_check(offers%offer_atlas_field("AA1", "always", "this is param AA1"))

! make offers (writable params)
! W_INT is offered as writable; W_RO is offered as read-only
call plume_check(offers%offer_int_writable("W_INT", "always", "writable int param"))
call plume_check(offers%offer_int("W_RO", "always", "read-only int param"))


! negotiate
mgr_conf_str = '{' // &
'"write-back-policy": "single-writer", ' // &
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
'            ], ' // &
'            [ ' // &
'                {"name":"W_INT", "type":"INT", "writable": true} ' // &
'            ], ' // &
'            [ ' // &
'                {"name":"W_RO", "type":"INT", "writable": true} ' // &
'            ] ' // &
'        ], ' // &
'        "core-config": {} ' // &
'    } ' // &
'] ' // &
'}' //c_null_char


call plume_check(manager%initialise())
call plume_check(manager%configure_from_string(trim(mgr_conf_str)))
call plume_check(manager%negotiate(offers))

! check accepted parameters (existing + writable W_INT)
param_names_requested = [ character(len=6) :: "I", "J", "FF1", "DD1", "AA1", "W_INT" ]
do i_param=1,size(param_names_requested)
    is_requested = .false.
    call plume_check(manager%is_param_requested(trim(param_names_requested(i_param)), is_requested))
    FCTEST_CHECK(is_requested)
end do

! check rejected parameters:
!   - JJJ, KKMM, XYZ, K: not offered
!   - W_RO: offered read-only but plugin required it as writable -> group rejected
param_names_rejected = [ character(len=6) :: "JJJ", "KKMM", "XYZ", "K", "W_RO" ]
do i_param=1,size(param_names_rejected)
    is_requested = .true.
    call plume_check(manager%is_param_requested(trim(param_names_rejected(i_param)), is_requested))
    FCTEST_CHECK( .not. is_requested)
end do

! finalise
call plume_check(manager%finalise())
call plume_check(offers%finalise())
call plume_check(plume_finalise())
call atlas_library%finalise()

    
END_TEST

END_TESTSUITE