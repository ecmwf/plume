! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

#include "fckit/fctest.h"


TESTSUITE( test_lib_fapi_suite )

TEST( test_lib_fapi )
use plume_module
implicit none

integer :: error_code = PLUME_SUCCESS

! initialise
error_code = plume_initialise()
FCTEST_CHECK(error_code == PLUME_SUCCESS)

! double initialization
error_code = plume_initialise()
FCTEST_CHECK(error_code /= PLUME_SUCCESS)

! finalise
error_code = plume_finalise()
FCTEST_CHECK(error_code == PLUME_SUCCESS)

! double finalisation
error_code = plume_finalise()
FCTEST_CHECK(error_code /= PLUME_SUCCESS)

    
END_TEST

END_TESTSUITE