! (C) Copyright 2025- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_atlas_field_lifetime_suite )

! Crosses the Fortran/C API to guard against the write-back refactor storing a
! dangling pointer when an atlas::Field is provided. The C side wraps the model's
! Field::Implementation* in a *local* atlas::Field before storing it; if Plume keeps
! a raw pointer to that local, it dangles as soon as the provide call returns.
! We therefore provide a field with known data, run intervening C-API calls to reuse
! the C stack, then read the field back and check its DATA (not just its name).
TEST( test_atlas_field_lifetime )
use atlas_module
use plume_module
use fckit_module
implicit none

type(plume_data) :: data
type(atlas_Field) :: field
type(atlas_Field) :: field_check
integer(c_int), pointer :: fdata(:)
integer(c_int), pointer :: fdata_check(:)

! scalar params used to exercise the C stack between provide and get
integer :: scratch = 123
integer :: scratch_check = -999

call atlas_library%initialise()
call plume_check(plume_initialise())
call plume_check(data%initialise())

! provided field carrying known data
field = atlas_Field("LF", atlas_integer(), (/4/))
call field%data(fdata)
fdata(1) = 11
fdata(2) = 22
fdata(3) = 33
fdata(4) = 44

call plume_check(data%provide_atlas_field_shared("LF", field))

! intervening C-API calls: push/pop frames over the region where the C-side
! temporary atlas::Field wrapper lived, so a dangling read is not masked.
call plume_check(data%provide_int("SCRATCH", scratch))
call plume_check(data%get_int("SCRATCH", scratch_check))
FCTEST_CHECK(scratch_check == 123)

! read the provided field back and validate its data survived
call plume_check(data%get_shared_atlas_field("LF", field_check))
FCTEST_CHECK(field_check%name() == "LF")

call field_check%data(fdata_check)
FCTEST_CHECK(fdata_check(1) == 11)
FCTEST_CHECK(fdata_check(2) == 22)
FCTEST_CHECK(fdata_check(3) == 33)
FCTEST_CHECK(fdata_check(4) == 44)

call plume_check(data%finalise())
call plume_check(plume_finalise())
call atlas_library%finalise()

END_TEST

END_TESTSUITE
