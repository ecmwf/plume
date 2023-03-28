! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_manager_module

use iso_c_binding
use fckit_configuration_module, only : fckit_configuration

use plume_utils_module, only : fortranise_cstr
use plume_data_module, only : plume_data
use plume_protocol_module, only : plume_protocol

implicit none
private


! plugins manager ("C"-handle wrapper)
type, public :: plume_manager
    type(c_ptr) :: impl = c_null_ptr
    logical :: is_finalised = .false.
contains
    procedure :: initialise => plume_manager_create_handle

    procedure :: configure => plume_manager_configure
    procedure :: negotiate => plume_manager_negotiate
    
    procedure :: active_fields => plume_manager_active_fields
    procedure :: active_fields_catalogue => plume_manager_active_fields_catalogue
    procedure :: is_param_requested => plume_manager_is_param_requested

    procedure :: feed_plugins => plume_manager_feed_plugins
    procedure :: run => plume_manager_run
    procedure :: finalise => plume_manager_finalise
end type


interface


function plume_manager_create_handle_interf( handle_impl ) result(err) &
    & bind(C,name="plume_manager_create_handle")
    use iso_c_binding, only: c_char, c_int, c_ptr
    type(c_ptr), intent(out) :: handle_impl
    integer(c_int) :: err
end function

function plume_manager_configure_interf(handle_impl, config_str) result(err) &
    & bind(C,name="plume_manager_configure")
    use iso_c_binding, only: c_char, c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char) :: config_str
    integer(c_int) :: err
end function

function plume_manager_negotiate_interf(handle_impl, proto_handle_impl) result(err) &
    & bind(C,name="plume_manager_negotiate")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    type(c_ptr), intent(in), value :: proto_handle_impl
    integer(c_int) :: err
end function

function plume_manager_active_fields_interf(handle_impl, fields) result(err) &
    & bind(C,name="plume_manager_active_fields")
    use iso_c_binding, only: c_ptr, c_int
    type(c_ptr), intent(in), value :: handle_impl
    type(c_ptr), intent(inout) :: fields
    integer(c_int) :: err
end function

function plume_manager_active_fields_catalogue_interf(handle_impl, field_catalogue_active_ptr) result(err) &
    & bind(C,name="plume_manager_active_data_catalogue")
    use iso_c_binding, only: c_ptr, c_int
    type(c_ptr), intent(in), value :: handle_impl
    type(c_ptr), intent(inout) :: field_catalogue_active_ptr
    integer(c_int) :: err
end function

function plume_manager_is_param_requested_interf(handle_impl, name, is_param) result(err) &
    & bind(C, name="plume_manager_is_param_requested")
    use iso_c_binding, only: c_ptr, c_char, c_bool
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char) :: name
    logical(c_bool), intent(inout) :: is_param
    integer :: err
end function

function plume_manager_feed_plugins_interf(handle_impl, fdata) result(err) &
  & bind(C,name="plume_manager_feed_plugins")
  use iso_c_binding, only: c_int, c_ptr
  type(c_ptr), intent(in), value :: handle_impl
  type(c_ptr), intent(in), value :: fdata
  integer(c_int) :: err
end function

function plume_manager_run_interf(handle_impl) result(err) &
    & bind(C,name="plume_manager_run")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function

function plume_manager_teardown_interf(handle_impl) result(err) &
    & bind(C,name="plume_manager_teardown")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function

function plume_manager_delete_handle_interf(handle_impl ) result(err) &
    & bind(C,name="plume_manager_delete_handle")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function

end interface


contains


function plume_manager_create_handle(handle) result(err)
    class(plume_manager), intent(inout) :: handle
    integer :: err
    err = plume_manager_create_handle_interf(handle%impl)
end function

function plume_manager_configure(handle, config_str) result(err)
    use iso_c_binding, only: c_null_char, c_char
    class(plume_manager), intent(inout) :: handle
    character(c_char) :: config_str
    integer :: err
    err = plume_manager_configure_interf(handle%impl, config_str)
end function

function plume_manager_negotiate(handle, protocol_handle) result(err)
    class(plume_manager), intent(inout) :: handle
    class(plume_protocol), intent(inout) :: protocol_handle
    integer :: err
    err = plume_manager_negotiate_interf(handle%impl, protocol_handle%impl)
end function

function plume_manager_feed_plugins(handle, fdata) result(err)
  class(plume_manager), intent(inout) :: handle
  class(plume_data), intent(in) :: fdata    
  integer :: err
  err = plume_manager_feed_plugins_interf(handle%impl, fdata%impl)
end function

function plume_manager_run(handle) result(err)
    class(plume_manager), intent(inout) :: handle
    integer :: err
    err = plume_manager_run_interf(handle%impl)
end function

! TODO: this really need to be checked!! not testing for errors, but returns a char*
function plume_manager_active_fields(handle) result(fields_str)
    use iso_c_binding, only: c_ptr
    class(plume_manager), intent(inout) :: handle    
    character(:), allocatable, target :: fields_str
    type(c_ptr) :: fields_ptr
    integer :: err
    err = plume_manager_active_fields_interf(handle%impl, fields_ptr)
    fields_str = fortranise_cstr(fields_ptr)
end function

! TODO: this really need to be checked!! not testing for errors, but returns a char*
function plume_manager_active_fields_catalogue(handle) result(active_catalogue)
    use iso_c_binding, only: c_ptr
    class(plume_manager), intent(inout) :: handle
    type(fckit_configuration) :: active_catalogue
    type(c_ptr) :: cat_ptr
    integer :: err

    ! init
    active_catalogue = fckit_configuration()

    ! get only the active part of the catalogue
    err = plume_manager_active_fields_catalogue_interf(handle%impl, cat_ptr)
    call active_catalogue%reset_c_ptr(cat_ptr)
end function

function plume_manager_is_param_requested(handle, name, is_param) result(err)
    use iso_c_binding, only: c_ptr, c_char, c_bool
    class(plume_manager), intent(inout) :: handle
    character(c_char) :: name
    logical(c_bool) :: is_param
    integer :: err
    err = plume_manager_is_param_requested_interf(handle%impl, name, is_param)
end function

function plume_manager_finalise(handle) result(err)
    class(plume_manager), intent(inout) :: handle
    integer :: err        
    if (handle%is_finalised .eqv. .false.) then
        ! teardown plugins and delete handle
        err = plume_manager_teardown_interf(handle%impl)
        err = plume_manager_delete_handle_interf(handle%impl)
        handle%is_finalised = .true.
    end if
end function


end module