! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_state_module

use iso_c_binding

use plume_utils_module, only : fortranise_cstr
use plume_utils_module, only : plume_delete_string

implicit none
private

public :: plume_state_current_name
public :: plume_state_current_parent
public :: plume_state_current_iteration
public :: plume_state_current_iteration_rel

interface

function plume_state_current_name_interf(state_name) result(err) &
    & bind(C, name="plume_state_current_name")
    use iso_c_binding, only: c_ptr, c_int
    type(c_ptr), intent(inout) :: state_name
    integer(c_int) :: err
end function

function plume_state_current_parent_interf(state_parent) result(err) &
    & bind(C, name="plume_state_current_parent")
    use iso_c_binding, only: c_ptr, c_int
    type(c_ptr), intent(inout) :: state_parent
    integer(c_int) :: err
end function

function plume_state_current_iteration_interf(iteration) result(err) &
    & bind(C, name="plume_state_current_iteration")
    use iso_c_binding, only: c_int, c_int64_t
    integer(c_int64_t), intent(inout) :: iteration
    integer(c_int) :: err
end function

function plume_state_current_iteration_rel_interf(iteration_rel) result(err) &
    & bind(C, name="plume_state_current_iteration_rel")
    use iso_c_binding, only: c_int, c_int64_t
    integer(c_int64_t), intent(inout) :: iteration_rel
    integer(c_int) :: err
end function

end interface

contains

! Not testing for errors, returns a string
function plume_state_current_name() result(state_name)
    use iso_c_binding, only: c_ptr
    character(:), allocatable, target :: state_name
    type(c_ptr) :: state_name_ptr
    integer :: err
    err = plume_state_current_name_interf(state_name_ptr)
    state_name = fortranise_cstr(state_name_ptr)
    err = plume_delete_string(state_name_ptr) ! free C-string
end function

! Not testing for errors, returns a string
function plume_state_current_parent() result(state_parent)
    use iso_c_binding, only: c_ptr
    character(:), allocatable, target :: state_parent
    type(c_ptr) :: state_parent_ptr
    integer :: err
    err = plume_state_current_parent_interf(state_parent_ptr)
    state_parent = fortranise_cstr(state_parent_ptr)
    err = plume_delete_string(state_parent_ptr) ! free C-string
end function

function plume_state_current_iteration(iteration) result(err)
    use iso_c_binding, only: c_int, c_int64_t
    integer(c_int64_t), intent(inout) :: iteration
    integer(c_int) :: err
    err = plume_state_current_iteration_interf(iteration)
end function

function plume_state_current_iteration_rel(iteration_rel) result(err)
    use iso_c_binding, only: c_int, c_int64_t
    integer(c_int64_t), intent(inout) :: iteration_rel
    integer(c_int) :: err
    err = plume_state_current_iteration_rel_interf(iteration_rel)
end function

end module
