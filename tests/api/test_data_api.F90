! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_data_fapi_suite )

TEST( test_data_fapi )
use atlas_module
use plume_module
use fckit_module
implicit none

! plume structures
type(plume_data) :: data

! data
integer :: param_i = 111
integer :: param_j = 222
real(c_float) :: param_ff1 = 333.3
real(c_double) :: param_dd1 = 444.4
type(atlas_Field) :: field

! check parameters (for provided params)
integer :: param_i_check = -999
integer :: param_j_check = -999
real(c_float) :: param_ff1_check = -999.0
real(c_double) :: param_dd1_check = -999.0
type(atlas_Field) :: field_check
integer :: param_not_found_check = -999

! check parameters (for created params)
integer :: param_cc_i_check = -999
real(c_float) :: param_cc_f_check = -999.0
real(c_double) :: param_cc_d_check = -999.0


call atlas_library%initialise()
call plume_check(plume_initialise())


! data
field = atlas_Field("AA1", atlas_integer(), (/0,10/))
call plume_check(data%initialise())
call plume_check(data%provide_int("I", param_i) )
call plume_check(data%provide_int("J", param_j) )
call plume_check(data%provide_float("FF1", param_ff1) )
call plume_check(data%provide_double("DD1", param_dd1) )
call plume_check(data%provide_atlas_field_shared("AA1", field) )

! create params
call plume_check(data%create_int("CC_I", -1) )
call plume_check(data%create_float("CC_F",real(-1.1,kind=c_float)) )
call plume_check(data%create_double("CC_D",real(-2.2,kind=c_double)) )


! just check that params have been provided in the data
call plume_check(data%get_int("I", param_i_check))
FCTEST_CHECK(param_i_check == param_i)

call plume_check(data%get_int("J", param_j_check))
FCTEST_CHECK(param_j_check == param_j)

call plume_check(data%get_float("FF1", param_ff1_check))
FCTEST_CHECK(param_ff1_check == param_ff1)

call plume_check(data%get_double("DD1", param_dd1_check))
FCTEST_CHECK(param_dd1_check == param_dd1)

call plume_check(data%get_shared_atlas_field("AA1", field_check))
FCTEST_CHECK(field_check%name() == field%name())


! check created params
call plume_check(data%get_int("CC_I", param_cc_i_check))
FCTEST_CHECK(param_cc_i_check == -1)

call plume_check(data%get_float("CC_F", param_cc_f_check))
FCTEST_CHECK(param_cc_f_check == -1.1)

call plume_check(data%get_double("CC_D", param_cc_d_check))
FCTEST_CHECK(param_cc_d_check == -2.2)


! now update the (created) parameters
call plume_check(data%update_int("CC_I", 777) )
call plume_check(data%update_float("CC_F", real(888.8,kind=c_float)) )
call plume_check(data%update_double("CC_D", real(999.9,kind=c_double)) )

! check updated parameters
call plume_check(data%get_int("CC_I", param_cc_i_check))
FCTEST_CHECK(param_cc_i_check == 777)

call plume_check(data%get_float("CC_F", param_cc_f_check))
FCTEST_CHECK(param_cc_f_check == 888.8)

call plume_check(data%get_double("CC_D", param_cc_d_check))
FCTEST_CHECK(param_cc_d_check == 999.9)


write(*,*) "Field check: ", field_check%name(), " vs ", field%name()

! finalise
call plume_check(data%finalise())
call plume_check(plume_finalise())
call atlas_library%finalise()
    
END_TEST

END_TESTSUITE