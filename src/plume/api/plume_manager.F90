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
use fckit_c_interop_module, only : c_str

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
    procedure :: configure_from_string => plume_manager_configure_from_string
    procedure :: negotiate => plume_manager_negotiate
    
    procedure :: active_fields => plume_manager_active_fields
    procedure :: active_fields_catalogue => plume_manager_active_fields_catalogue
    procedure :: is_param_requested => plume_manager_is_param_requested
    procedure :: is_plugin_activated => plume_manager_is_plugin_activated

    procedure :: active_fields_at_hook => plume_manager_active_fields_at_hook
    procedure :: is_param_requested_at_hook => plume_manager_is_param_requested_at_hook
    procedure :: is_hook_active => plume_manager_is_hook_active
    procedure :: registered_hooks => plume_manager_registered_hooks

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
    character(c_char), dimension(*) :: config_str
    integer(c_int) :: err
end function

function plume_manager_configure_from_string_interf(handle_impl, config_str) result(err) &
    & bind(C,name="plume_manager_configure_from_string")
    use iso_c_binding, only: c_char, c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: config_str
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
    character(c_char), dimension(*) :: name
    logical(c_bool), intent(inout) :: is_param
    integer :: err
end function

function plume_manager_is_plugin_activated_interf(handle_impl, name, is_plugin) result(err) &
    & bind(C, name="plume_manager_is_plugin_activated")
    use iso_c_binding, only: c_ptr, c_char, c_bool
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    logical(c_bool), intent(inout) :: is_plugin
    integer :: err
end function

function plume_manager_feed_plugins_interf(handle_impl, fdata) result(err) &
  & bind(C,name="plume_manager_feed_plugins")
  use iso_c_binding, only: c_int, c_ptr
  type(c_ptr), intent(in), value :: handle_impl
  type(c_ptr), intent(in), value :: fdata
  integer(c_int) :: err
end function

function plume_manager_active_fields_at_hook_interf(handle_impl, hook, derived, fields) result(err) &
    & bind(C,name="plume_manager_active_fields_at_hook")
    use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: hook
    logical(c_bool), intent(in), value :: derived
    type(c_ptr), intent(inout) :: fields
    integer(c_int) :: err
end function

function plume_manager_is_param_requested_at_hook_interf(handle_impl, name, hook, is_param) result(err) &
    & bind(C, name="plume_manager_is_param_requested_at_hook")
    use iso_c_binding, only: c_ptr, c_char, c_bool
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    character(c_char), dimension(*) :: hook
    logical(c_bool), intent(inout) :: is_param
    integer :: err
end function

function plume_manager_is_hook_active_interf(handle_impl, name, is_active) result(err) &
    & bind(C, name="plume_manager_is_hook_active")
    use iso_c_binding, only: c_ptr, c_char, c_bool
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    logical(c_bool), intent(inout) :: is_active
    integer :: err
end function

function plume_manager_registered_hooks_interf(handle_impl, hooks) result(err) &
    & bind(C,name="plume_manager_registered_hooks")
    use iso_c_binding, only: c_ptr, c_int
    type(c_ptr), intent(in), value :: handle_impl
    type(c_ptr), intent(inout) :: hooks
    integer(c_int) :: err
end function

function plume_manager_run_interf(handle_impl) result(err) &
    & bind(C,name="plume_manager_run")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function

function plume_manager_run_hook_interf(handle_impl, hook) result(err) &
    & bind(C,name="plume_manager_run_hook")
    use iso_c_binding, only: c_int, c_ptr, c_char
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: hook
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
    character(kind=c_char,len=*), intent(in) :: config_str
    integer :: err
    err = plume_manager_configure_interf(handle%impl, c_str(config_str))
end function

function plume_manager_configure_from_string(handle, config_str) result(err)
    use iso_c_binding, only: c_null_char, c_char
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in) :: config_str
    integer :: err
    err = plume_manager_configure_from_string_interf(handle%impl, c_str(config_str))
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

! Run the active plugins. Without the optional "hook" argument this targets the default hook
! point, exactly as it did before hook points existed.
function plume_manager_run(handle, hook) result(err)
    use iso_c_binding, only: c_char
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in), optional :: hook
    integer :: err
    if (present(hook)) then
        err = plume_manager_run_hook_interf(handle%impl, c_str(hook))
    else
        err = plume_manager_run_interf(handle%impl)
    end if
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
    character(kind=c_char,len=*), intent(in) :: name
    logical(c_bool) :: is_param
    integer :: err
    err = plume_manager_is_param_requested_interf(handle%impl, c_str(name), is_param)
end function

function plume_manager_is_plugin_activated(handle, name, is_plugin) result(err)
    use iso_c_binding, only: c_ptr, c_char, c_bool
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in) :: name
    logical(c_bool) :: is_plugin
    integer :: err
    err = plume_manager_is_plugin_activated_interf(handle%impl, c_str(name), is_plugin)
end function

! Fields requested by the plugins bound to a specific hook point.
! "derived" defaults to .true., matching the global active_fields query. Pass .false. to get only
! the fields that the model is expected to provide.
! TODO: like active_fields, this does not check for errors, but returns a char*
function plume_manager_active_fields_at_hook(handle, hook, derived) result(fields_str)
    use iso_c_binding, only: c_ptr, c_char, c_bool
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in) :: hook
    logical, intent(in), optional :: derived
    character(:), allocatable, target :: fields_str
    type(c_ptr) :: fields_ptr
    logical(c_bool) :: with_derived
    integer :: err

    with_derived = .true._c_bool
    if (present(derived)) then
        with_derived = logical(derived, kind=c_bool)
    end if

    err = plume_manager_active_fields_at_hook_interf(handle%impl, c_str(hook), with_derived, fields_ptr)
    fields_str = fortranise_cstr(fields_ptr)
end function

function plume_manager_is_param_requested_at_hook(handle, name, hook, is_param) result(err)
    use iso_c_binding, only: c_ptr, c_char, c_bool
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in) :: name
    character(kind=c_char,len=*), intent(in) :: hook
    logical(c_bool) :: is_param
    integer :: err
    err = plume_manager_is_param_requested_at_hook_interf(handle%impl, c_str(name), c_str(hook), is_param)
end function

function plume_manager_is_hook_active(handle, name, is_active) result(err)
    use iso_c_binding, only: c_ptr, c_char, c_bool
    class(plume_manager), intent(inout) :: handle
    character(kind=c_char,len=*), intent(in) :: name
    logical(c_bool) :: is_active
    integer :: err
    err = plume_manager_is_hook_active_interf(handle%impl, c_str(name), is_active)
end function

! Hook points registered by the model (always includes the default one).
! TODO: like active_fields, this does not check for errors, but returns a char*
function plume_manager_registered_hooks(handle) result(hooks_str)
    use iso_c_binding, only: c_ptr
    class(plume_manager), intent(inout) :: handle
    character(:), allocatable, target :: hooks_str
    type(c_ptr) :: hooks_ptr
    integer :: err
    err = plume_manager_registered_hooks_interf(handle%impl, hooks_ptr)
    hooks_str = fortranise_cstr(hooks_ptr)
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