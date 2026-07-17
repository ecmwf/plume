! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"

TESTSUITE( test_writeback_fapi_suite )

TEST( test_writeback_fapi )
use iso_c_binding, only: c_int, c_double, c_null_char
use plume_module
use plume_data_module
use plume_manager_module
use plume_protocol_module
implicit none

type(plume_manager)  :: manager
type(plume_protocol) :: protocol
type(plume_data)     :: data

! Model-owned memory for writable parameters
integer(c_int)    :: fort_w_int          = 0
real(c_double)    :: fort_w_double_check = 0.0d0

character(len=500) :: mgr_conf_str
character(len=:), allocatable :: pending_str

call plume_check(plume_initialise())

! Offer writable parameters (model side)
call plume_check(protocol%initialise())
call plume_check(protocol%offer_int_writable("FORT_W_INT",    "always", "writable int"))
call plume_check(protocol%offer_double_writable("FORT_W_DOUBLE", "always", "writable double"))

mgr_conf_str = '{' // &
'"write-back-policy": "single-writer", ' // &
'"plugins": [' // &
'    {' // &
'        "lib": "plume_plugin_writeback_fapi",' // &
'        "name": "WriteBackTestFAPI",' // &
'        "parameters": [' // &
'            [' // &
'                {"name": "FORT_W_INT",    "type": "INT",    "writable": true},' // &
'                {"name": "FORT_W_DOUBLE", "type": "DOUBLE", "writable": true}' // &
'            ]' // &
'        ],' // &
'        "core-config": {}' // &
'    }' // &
']}' // c_null_char

call plume_check(manager%initialise())
call plume_check(manager%configure_from_string(trim(mgr_conf_str)))
call plume_check(manager%negotiate(protocol))

call plume_check(data%initialise())
! Writable params can be either provided (model-owned memory) or created (Plume-owned). Both are tested here.
call plume_check(data%provide_int("FORT_W_INT",       fort_w_int))         ! model-owned
call plume_check(data%create_double("FORT_W_DOUBLE",  real(0.0d0, kind=c_double)))  ! Plume-owned
call plume_check(manager%feed_plugins(data))

! Run the plugin — it writes FORT_W_INT=99 and FORT_W_DOUBLE=1.23
call plume_check(manager%run())

! Verify value written to model-owned memory via raw pointer
FCTEST_CHECK(fort_w_int == 99_c_int)

! Verify value written to Plume-owned memory via get
call plume_check(data%get_double("FORT_W_DOUBLE", fort_w_double_check))
FCTEST_CHECK(abs(fort_w_double_check - real(1.23d0, kind=c_double)) < 1.0d-10)

! Verify pending write-backs
pending_str = data%pending_writebacks()
FCTEST_CHECK(index(pending_str, "FORT_W_INT")    > 0)
FCTEST_CHECK(index(pending_str, "FORT_W_DOUBLE") > 0)

! Acknowledge each write-back
call plume_check(data%acknowledge_writeback("FORT_W_INT"))
call plume_check(data%acknowledge_writeback("FORT_W_DOUBLE"))

! Verify no more pending write-backs after acknowledgement
pending_str = data%pending_writebacks()
FCTEST_CHECK(index(pending_str, "FORT_W_INT")    == 0)
FCTEST_CHECK(index(pending_str, "FORT_W_DOUBLE") == 0)

call plume_check(manager%finalise())
call plume_check(data%finalise())
call plume_check(protocol%finalise())
call plume_check(plume_finalise())

END_TEST

END_TESTSUITE
