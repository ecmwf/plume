! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_module

use plume_lib_module, only : plume_initialise, &
                             plume_finalise

use plume_data_module, only : plume_data
use plume_manager_module, only : plume_manager
use plume_protocol_module, only : plume_protocol
use plume_utils_module, only : fortranise_cstr

implicit none
private

! Error values
integer, public, parameter :: PLUME_SUCCESS = 0
integer, public, parameter :: PLUME_ERROR_GENERAL_EXCEPTION = 1
integer, public, parameter :: PLUME_ERROR_UNKNOWN_EXCEPTION = 2


! API modules
public :: plume_check
public :: plume_error_string

public :: plume_initialise
public :: plume_finalise

public :: plume_data
public :: plume_manager
public :: plume_protocol


interface
  function plume_error_string_interf(err) result(error_string) bind(c, name='plume_error_string')
    use, intrinsic :: iso_c_binding
    implicit none
    integer(c_int), intent(in), value :: err
    type(c_ptr) :: error_string
  end function
end interface


contains


subroutine plume_check(err)
    integer, intent(in) :: err  
    if (err /= PLUME_SUCCESS) then
        print *, "Error: ", plume_error_string(err)
        stop 1
    end if
end subroutine

function plume_error_string(err) result(error_string)
    integer, intent(in) :: err
    character(:), allocatable, target :: error_string
    error_string = fortranise_cstr(plume_error_string_interf(err))
end function


end module