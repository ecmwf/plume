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
use atlas_module
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

type(atlas_Field)       :: fort_w_field
integer(c_int), pointer :: fort_w_field_data(:)

type(atlas_Field)       :: fort_w_scope_field
integer(c_int), pointer :: fort_w_scope_field_data(:)

type(atlas_Field)       :: fort_w_scope_field2
integer(c_int), pointer :: fort_w_scope_field2_data(:)

character(len=800) :: mgr_conf_str
character(len=:), allocatable :: pending_str

call atlas_library%initialise()
call plume_check(plume_initialise())

! Offer writable parameters (model side)
call plume_check(protocol%initialise())
call plume_check(protocol%offer_int_writable("FORT_W_INT",    "always", "writable int"))
call plume_check(protocol%offer_double_writable("FORT_W_DOUBLE", "always", "writable double"))
call plume_check(protocol%offer_atlas_field_writable("FORT_W_FIELD", "always", "writable atlas field"))
call plume_check(protocol%offer_atlas_field_writable("FORT_W_SCOPE_FIELD", "always", "in-place scope field"))
call plume_check(protocol%offer_atlas_field_writable("FORT_W_SCOPE_FIELD2", "always", "explicit-commit scope field"))

mgr_conf_str = '{' // &
'"write-back-policy": "single-writer", ' // &
'"plugins": [' // &
'    {' // &
'        "lib": "plume_plugin_writeback_fapi",' // &
'        "name": "WriteBackTestFAPI",' // &
'        "parameters": [' // &
'            [' // &
'                {"name": "FORT_W_INT",    "type": "INT",    "writable": true},' // &
'                {"name": "FORT_W_DOUBLE", "type": "DOUBLE", "writable": true},' // &
'                {"name": "FORT_W_FIELD",  "type": "ATLAS_FIELD", "writable": true},' // &
'                {"name": "FORT_W_SCOPE_FIELD", "type": "ATLAS_FIELD", "writable": true},' // &
'                {"name": "FORT_W_SCOPE_FIELD2", "type": "ATLAS_FIELD", "writable": true}' // &
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

! Provided (model-owned) atlas field seeded with known values; the write-back must land in this buffer in place.
fort_w_field = atlas_Field("FORT_W_FIELD", atlas_integer(), (/3/))
call fort_w_field%data(fort_w_field_data)
fort_w_field_data(1) = 1_c_int
fort_w_field_data(2) = 2_c_int
fort_w_field_data(3) = 3_c_int
call plume_check(data%provide_atlas_field_shared("FORT_W_FIELD", fort_w_field))

! Provided (model-owned) field for the in-place scope path, seeded so the plugin's +1000 read-modify-write is checkable.
fort_w_scope_field = atlas_Field("FORT_W_SCOPE_FIELD", atlas_integer(), (/3/))
call fort_w_scope_field%data(fort_w_scope_field_data)
fort_w_scope_field_data(1) = 7_c_int
fort_w_scope_field_data(2) = 8_c_int
fort_w_scope_field_data(3) = 9_c_int
call plume_check(data%provide_atlas_field_shared("FORT_W_SCOPE_FIELD", fort_w_scope_field))

! Provided (model-owned) field for the explicit-commit scope path, seeded so the plugin's +500 write is checkable.
fort_w_scope_field2 = atlas_Field("FORT_W_SCOPE_FIELD2", atlas_integer(), (/3/))
call fort_w_scope_field2%data(fort_w_scope_field2_data)
fort_w_scope_field2_data(1) = 11_c_int
fort_w_scope_field2_data(2) = 12_c_int
fort_w_scope_field2_data(3) = 13_c_int
call plume_check(data%provide_atlas_field_shared("FORT_W_SCOPE_FIELD2", fort_w_scope_field2))

call plume_check(manager%feed_plugins(data))

! Run the plugin — it writes FORT_W_INT=99, FORT_W_DOUBLE=1.23 and FORT_W_FIELD=(100,200,300)
! FORT_W_SCOPE_FIELD=(1007,1008,1009), FORT_W_SCOPE_FIELD2=(511,512,513)
call plume_check(manager%run())

! Verify value written to model-owned memory via raw pointer
FCTEST_CHECK(fort_w_int == 99_c_int)

! Verify value written to Plume-owned memory via get
call plume_check(data%get_double("FORT_W_DOUBLE", fort_w_double_check))
FCTEST_CHECK(abs(fort_w_double_check - real(1.23d0, kind=c_double)) < 1.0d-10)

! Verify the provided atlas field was written in place: the model's own field handle sees the new values.
FCTEST_CHECK(fort_w_field_data(1) == 100_c_int)
FCTEST_CHECK(fort_w_field_data(2) == 200_c_int)
FCTEST_CHECK(fort_w_field_data(3) == 300_c_int)

! Verify the scope path wrote in place through the model's own buffer (seed 7,8,9 + 1000 via RAII auto-commit).
FCTEST_CHECK(fort_w_scope_field_data(1) == 1007_c_int)
FCTEST_CHECK(fort_w_scope_field_data(2) == 1008_c_int)
FCTEST_CHECK(fort_w_scope_field_data(3) == 1009_c_int)

! Verify the explicit-commit scope path wrote in place (seed 11,12,13 + 500); reaching here without a crash also
! confirms the block-exit finaliser did not double-commit/double-free the already-committed scope.
FCTEST_CHECK(fort_w_scope_field2_data(1) == 511_c_int)
FCTEST_CHECK(fort_w_scope_field2_data(2) == 512_c_int)
FCTEST_CHECK(fort_w_scope_field2_data(3) == 513_c_int)

! Verify pending write-backs
pending_str = data%pending_writebacks()
FCTEST_CHECK(index(pending_str, "FORT_W_INT")    > 0)
FCTEST_CHECK(index(pending_str, "FORT_W_DOUBLE") > 0)
FCTEST_CHECK(index(pending_str, "FORT_W_FIELD")  > 0)
FCTEST_CHECK(index(pending_str, "FORT_W_SCOPE_FIELD") > 0)
FCTEST_CHECK(index(pending_str, "FORT_W_SCOPE_FIELD2") > 0)

! Acknowledge each write-back
call plume_check(data%acknowledge_writeback("FORT_W_INT"))
call plume_check(data%acknowledge_writeback("FORT_W_DOUBLE"))
call plume_check(data%acknowledge_writeback("FORT_W_FIELD"))
call plume_check(data%acknowledge_writeback("FORT_W_SCOPE_FIELD"))
call plume_check(data%acknowledge_writeback("FORT_W_SCOPE_FIELD2"))

! Verify no more pending write-backs after acknowledgement
pending_str = data%pending_writebacks()
FCTEST_CHECK(index(pending_str, "FORT_W_INT")    == 0)
FCTEST_CHECK(index(pending_str, "FORT_W_DOUBLE") == 0)
FCTEST_CHECK(index(pending_str, "FORT_W_FIELD")  == 0)
FCTEST_CHECK(index(pending_str, "FORT_W_SCOPE_FIELD") == 0)
FCTEST_CHECK(index(pending_str, "FORT_W_SCOPE_FIELD2") == 0)

call plume_check(manager%finalise())
call plume_check(data%finalise())
call plume_check(protocol%finalise())
call plume_check(plume_finalise())
call atlas_library%finalise()

END_TEST

END_TESTSUITE
