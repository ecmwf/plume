! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_utils_module

use iso_c_binding, only : c_ptr, c_null_char, c_loc, c_f_pointer

implicit none 
private

public :: fortranise_cstr
public :: plume_delete_string


interface
    pure function strlen(str) result(len) bind(c)
        use, intrinsic :: iso_c_binding
        implicit none
        type(c_ptr), intent(in), value :: str
        integer(c_int) :: len
    end function
end interface

interface
    function plume_delete_string_interf(str) bind(C, name="plume_delete_string") result(err)
        use iso_c_binding
        type(c_ptr), value :: str
        integer(c_int) :: err
    end function
end interface

contains


function fortranise_cstr(cstr) result(fstr)
    use iso_c_binding, only: c_char, c_ptr
    type(c_ptr), intent(in) :: cstr
    character(:), allocatable, target :: fstr
    character(c_char), pointer :: tmp(:)
    integer :: length  
    length = strlen(cstr)
    allocate(character(length) :: fstr)
    call c_f_pointer(cstr, tmp, [length])
    fstr = transfer(tmp(1:length), fstr)
end function

! use plume_delete_string to free C-strings (e.g. allocated by Plume API functions)
function plume_delete_string(str) result(err)
    use iso_c_binding
    type(c_ptr), intent(inout) :: str
    integer(c_int) :: err

    if (c_associated(str)) then
        err = plume_delete_string_interf(str)
        str = c_null_ptr
    else
        err = 0
    end if
end function


end module