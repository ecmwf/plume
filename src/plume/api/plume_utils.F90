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


interface
    pure function strlen(str) result(len) bind(c)
        use, intrinsic :: iso_c_binding
        implicit none
        type(c_ptr), intent(in), value :: str
        integer(c_int) :: len
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

end module