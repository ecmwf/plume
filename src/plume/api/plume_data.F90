! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_data_module

use iso_c_binding
use fckit_c_interop_module, only : c_str
use plume_utils_module, only : fortranise_cstr

use atlas_module

! PLUME data view — the plugin-facing handle. Binds ONLY plugin-legal procedures (read access +
! write-back + the plugin bridge entry point initialise_from_ptr). The model-facing plume_data type
! below EXTENDS this, adding the model-only mutators. Because Fortran type extension can only ADD
! bindings, a plugin that holds a plume_data_view has no create_*/provide_*/update_*/set_updated/
! pending_writebacks/acknowledge_writeback bindings at all: calling them is a compile-time error.
!
! NOTE — the inheritance is INVERTED relative to the C++ side, because the two languages restrict
! differently. In C++, ModelDataView DERIVES from the full ModelData (private inheritance) and HIDES the
! members it must not expose. In Fortran, extension can only add — never remove — so the restricted VIEW
! must be the BASE and the full model type extends it. Same guarantee (a plugin handle has no mutator to
! call), opposite direction. The type-bound implementations below therefore take class(plume_data_view)
! for the shared procedures; a model-side plume_data handle still resolves them via inheritance.
type, public :: plume_data_view
    type(c_ptr) :: impl = c_null_ptr
    logical :: is_finalised = .false.
contains
    procedure :: initialise_from_ptr    => plume_data_create_handle_from_ptr

    procedure :: get_int                => plume_data_get_int
    procedure :: get_bool               => plume_data_get_bool
    procedure :: get_float              => plume_data_get_float
    procedure :: get_double             => plume_data_get_double
    procedure :: get_shared_atlas_field => plume_data_get_shared_atlas_field

    procedure :: write_int               => plume_data_write_int
    procedure :: write_bool              => plume_data_write_bool
    procedure :: write_float             => plume_data_write_float
    procedure :: write_double            => plume_data_write_double
    procedure :: write_atlas_field_copy  => plume_data_write_atlas_field_copy
    procedure :: write_atlas_field_scope => plume_data_write_atlas_field_scope

    procedure :: print    => plume_data_print
    procedure :: finalise => plume_data_delete_handle
end type


! PLUME data — the model-facing handle. Extends plume_data_view with the model-only mutators
! (parameter creation, provisioning, in-place updates and write-back reconciliation).
type, extends(plume_data_view), public :: plume_data
contains
    procedure :: initialise                 => plume_data_create_handle

    procedure :: create_int                 => plume_data_create_int
    procedure :: create_bool                => plume_data_create_bool
    procedure :: create_float               => plume_data_create_float
    procedure :: create_double              => plume_data_create_double

    procedure :: provide_int                => plume_data_provide_int
    procedure :: provide_bool               => plume_data_provide_bool
    procedure :: provide_float              => plume_data_provide_float
    procedure :: provide_double             => plume_data_provide_double

    procedure :: provide_atlas_field_shared => plume_data_provide_atlas_field_shared

    procedure :: update_int                 => plume_data_update_int
    procedure :: update_bool                => plume_data_update_bool
    procedure :: update_float               => plume_data_update_float
    procedure :: update_double              => plume_data_update_double

    procedure :: set_updated                => plume_data_set_updated

    procedure :: pending_writebacks         => plume_data_pending_writebacks
    procedure :: acknowledge_writeback      => plume_data_acknowledge_writeback

end type


! PLUME write scope — an RAII handle for an in-place, copy-free Atlas write-back, the Fortran analogue of the C++
! WriteScope. Obtained via plume_data_view%write_atlas_field_scope(name, scope).
! Contrast write_atlas_field_copy, which submits a separate field whose contents are copied in.
type, public :: plume_write_scope
    type(c_ptr) :: scope     = c_null_ptr  ! opaque plume::data::WriteScope*
    type(c_ptr) :: field_ptr = c_null_ptr  ! staged atlas::Field::Implementation* (the model's own buffer)
contains
    procedure :: view   => plume_write_scope_view
    procedure :: commit => plume_write_scope_commit
    final     :: plume_write_scope_final
end type


! --------- PLUME ModelData
interface

function plume_data_create_handle_interf( handle_impl ) result(err) &
    & bind(C,name="plume_data_create_handle_t")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(out) :: handle_impl
    integer(c_int) :: err
end function


function plume_data_delete_handle_interf( handle_impl ) result(err) &
    & bind(C,name="plume_data_delete_handle")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function


function plume_data_print_interf( handle_impl ) result(err) &
    & bind(C,name="plume_data_print")
    use iso_c_binding, only: c_int, c_ptr
    type(c_ptr), intent(in), value :: handle_impl
    integer(c_int) :: err
end function


function plume_data_set_updated_interf( handle_impl, count, names ) result(err) &
  & bind(C,name="plume_data_set_updated")
  use iso_c_binding, only: c_int, c_ptr
  type(c_ptr), intent(in), value :: handle_impl
  integer(c_int), intent(in), value :: count
  type(c_ptr), dimension(*), intent(in) :: names
  integer(c_int) :: err
end function


! ------------- Data "creators"
function plume_data_create_int_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_create_int")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  integer(c_int), intent(in), value :: value
  integer(c_int) :: err
end function


function plume_data_create_bool_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_create_bool")
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  logical(c_bool), intent(in), value :: value
  integer(c_int) :: err
end function    


function plume_data_create_float_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_create_float")
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_float), intent(in), value :: value
  integer(c_int) :: err
end function


function plume_data_create_double_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_create_double")
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_double), intent(in), value :: value
  integer(c_int) :: err
end function



! ------------- Data "updaters"
function plume_data_update_int_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_update_int")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  integer(c_int), intent(in), value :: value
  integer(c_int) :: err
end function


function plume_data_update_bool_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_update_bool")
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  logical(c_bool), intent(in), value :: value
  integer(c_int) :: err
end function    


function plume_data_update_float_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_update_float")
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_float), intent(in), value :: value
  integer(c_int) :: err
end function


function plume_data_update_double_interf( handle_impl, name, value ) result(err) &
  & bind(C,name="plume_data_update_double")
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_double), intent(in), value :: value
  integer(c_int) :: err
end function




! ------------- Data "providers"
function plume_data_provide_int_interf( handle_impl, name, value ) result(err) &
    & bind(C,name="plume_data_provide_int")
    use iso_c_binding, only: c_ptr, c_char, c_int
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    integer(c_int), intent(in) :: value
    integer(c_int) :: err
end function


function plume_data_provide_bool_interf( handle_impl, name, value ) result(err) &
    & bind(C,name="plume_data_provide_bool")
    use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    logical(c_bool), intent(in) :: value
    integer(c_int) :: err
end function    


function plume_data_provide_float_interf( handle_impl, name, value ) result(err) &
    & bind(C,name="plume_data_provide_float")
    use iso_c_binding, only: c_ptr, c_char, c_float, c_int
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    real(c_float), intent(in) :: value
    integer(c_int) :: err
end function


function plume_data_provide_double_interf( handle_impl, name, value ) result(err) &
    & bind(C,name="plume_data_provide_double")
    use iso_c_binding, only: c_ptr, c_char, c_double, c_int
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    real(c_double), intent(in) :: value
    integer(c_int) :: err
end function


function plume_data_provide_atlas_field_shared_interf( handle_impl, name, value ) result(err) &
    & bind(C,name="plume_data_provide_atlas_field_shared")
    use iso_c_binding, only: c_ptr, c_char, c_ptr, c_int
    type(c_ptr), intent(in), value :: handle_impl
    character(c_char), dimension(*) :: name
    type(c_ptr), intent(in), value :: value
    integer(c_int) :: err
end function

! Needed to initialise the plume data wrapper from an existing object (used for fortran plugins)
function plume_data_create_handle_from_ptr_interf( handle_impl, data_c_ptr ) result(err) &
  & bind(C,name="plume_data_create_handle_from_ptr")
  use iso_c_binding, only: c_int, c_ptr
  type(c_ptr), intent(out) :: handle_impl
  type(c_ptr), intent(in), value :: data_c_ptr
  integer(c_int) :: err
end function

function plume_data_get_shared_atlas_field_interf(handle_impl, name, field_c_ptr) result(err) &
  & bind(C,name="plume_data_get_shared_atlas_field")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  type(c_ptr), intent(out) :: field_c_ptr
  integer(c_int) :: err
end function

function plume_data_write_scope_begin_interf(handle_impl, name, scope_ptr, field_c_ptr) result(err) &
  & bind(C,name="plume_data_write_scope_begin")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  type(c_ptr), intent(out) :: scope_ptr
  type(c_ptr), intent(out) :: field_c_ptr
  integer(c_int) :: err
end function

function plume_data_write_scope_commit_interf(scope_ptr) result(err) &
  & bind(C,name="plume_data_write_scope_commit")
  use iso_c_binding, only: c_ptr, c_int
  type(c_ptr), intent(in), value :: scope_ptr
  integer(c_int) :: err
end function

function plume_data_get_int_interf( handle_impl, name, val ) result (err) &
  & bind(C,name="plume_data_get_int")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  integer(c_int), intent(inout) :: val
  integer(c_int) :: err
end function

function plume_data_get_bool_interf( handle_impl, name, val ) result (err) &
  & bind(C,name="plume_data_get_bool")
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  logical(c_bool), intent(inout) :: val
  integer(c_int) :: err
end function

function plume_data_get_float_interf( handle_impl, name, val ) result (err) &
  & bind(C,name="plume_data_get_float")
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_float), intent(inout) :: val
  integer(c_int) :: err
end function

function plume_data_get_double_interf( handle_impl, name, val ) result (err) &
  & bind(C,name="plume_data_get_double")
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_double), intent(inout) :: val
  integer(c_int) :: err
end function

! Write-back API interfaces
function plume_data_write_int_interf( handle_impl, name, val ) result(err) &
  & bind(C,name="plume_data_write_int")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  integer(c_int), intent(in), value :: val
  integer(c_int) :: err
end function

function plume_data_write_bool_interf( handle_impl, name, val ) result(err) &
  & bind(C,name="plume_data_write_bool")
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  logical(c_bool), intent(in), value :: val
  integer(c_int) :: err
end function

function plume_data_write_float_interf( handle_impl, name, val ) result(err) &
  & bind(C,name="plume_data_write_float")
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_float), intent(in), value :: val
  integer(c_int) :: err
end function

function plume_data_write_double_interf( handle_impl, name, val ) result(err) &
  & bind(C,name="plume_data_write_double")
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  real(c_double), intent(in), value :: val
  integer(c_int) :: err
end function

function plume_data_write_atlas_field_interf( handle_impl, name, field_ptr ) result(err) &
  & bind(C,name="plume_data_write_atlas_field")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  type(c_ptr), intent(in), value :: field_ptr
  integer(c_int) :: err
end function

function plume_data_pending_writebacks_interf( handle_impl, names ) result(err) &
  & bind(C,name="plume_data_pending_writebacks")
  use iso_c_binding, only: c_ptr, c_int
  type(c_ptr), intent(in), value :: handle_impl
  type(c_ptr), intent(out) :: names
  integer(c_int) :: err
end function

function plume_data_acknowledge_writeback_interf( handle_impl, name ) result(err) &
  & bind(C,name="plume_data_acknowledge_writeback")
  use iso_c_binding, only: c_ptr, c_char, c_int
  type(c_ptr), intent(in), value :: handle_impl
  character(c_char), dimension(*) :: name
  integer(c_int) :: err
end function

end interface

! interface insert_param
!   module procedure plume_data_provide_int
!   module procedure plume_data_provide_bool
!   module procedure plume_data_provide_float
!   module procedure plume_data_provide_double
!   module procedure plume_data_provide_atlas_field_shared
! end interface

! interface get_param
!   module procedure plume_data_get_int
!   module procedure plume_data_get_bool
!   module procedure plume_data_get_float
!   module procedure plume_data_get_double
! end Interface

contains

! ------- Plume plugins data

function plume_data_create_handle( handle ) result(err)
    class(plume_data), intent(inout) :: handle
    integer :: err
    err = plume_data_create_handle_interf(handle%impl)
end function

function plume_data_create_handle_from_ptr( handle, data_c_ptr ) result(err)
  use iso_c_binding, only: c_ptr
  class(plume_data_view), intent(inout) :: handle
  type(c_ptr), intent(in), value :: data_c_ptr
  integer :: err
  err = plume_data_create_handle_from_ptr_interf(handle%impl, data_c_ptr)
end function

function plume_data_delete_handle( handle ) result(err)
  class(plume_data_view), intent(inout) :: handle
  integer :: err
  if (handle%is_finalised .eqv. .false.) then
    err = plume_data_delete_handle_interf(handle%impl)
    handle%is_finalised = .true.
  end if
end function



function plume_data_create_int( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  integer(c_int), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_create_int_interf(handle%impl, c_str(name), value )
end function

function plume_data_create_bool( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  logical(c_bool), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_create_bool_interf(handle%impl, c_str(name), value)
end function

function plume_data_create_float( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_float), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_create_float_interf(handle%impl, c_str(name), value)
end function

function plume_data_create_double( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_double), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_create_double_interf(handle%impl, c_str(name), value)
end function





function plume_data_update_int( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  integer(c_int), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_update_int_interf(handle%impl, c_str(name), value )
end function

function plume_data_update_bool( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  logical(c_bool), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_update_bool_interf(handle%impl, c_str(name), value)
end function

function plume_data_update_float( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_float), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_update_float_interf(handle%impl, c_str(name), value)
end function

function plume_data_update_double( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_double), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_update_double_interf(handle%impl, c_str(name), value)
end function





function plume_data_provide_int( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  integer(c_int), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_provide_int_interf(handle%impl, c_str(name), value )
end function

function plume_data_provide_bool( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_bool, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  logical(c_bool), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_provide_bool_interf(handle%impl, c_str(name), value)
end function

function plume_data_provide_float( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_float, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_float), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_provide_float_interf(handle%impl, c_str(name), value)
end function

function plume_data_provide_double( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_double, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_double), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_provide_double_interf(handle%impl, c_str(name), value)
end function

! insert an atlas field
function plume_data_provide_atlas_field_shared( handle, name, value ) result(err)
  use iso_c_binding, only: c_ptr, c_char, c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  type(atlas_Field), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_provide_atlas_field_shared_interf(handle%impl, c_str(name), value%c_ptr())
end function

! this function returns a field from a requested field name
function plume_data_get_shared_atlas_field( handle, name, field ) result (err)
  use iso_c_binding, only: c_ptr, c_char, c_int
  ! NOTE (PLUME-75): this returns a fully mutable atlas_field that shares the model's buffer. Unlike the C++
  ! path (where getParam yields a read-only plume::data::FieldView), the read-only field guarantee is NOT enforced
  ! here — a Fortran plugin can still mutate the field in place and bypass the write-back ledger. Extending the
  ! enforcement to the Fortran path is a possible follow-up.
  class(plume_data_view), intent(inout) :: handle
  character(*), intent(in) :: name
  type(atlas_field), intent(out) :: field
  type(c_ptr) :: field_ptr
  integer(c_int) :: err

  ! Wrap the borrowed (model-owned) FieldImpl with the canonical atlas cptr constructor: atlas_Field(cptr)
  ! attaches a reference, and the return() inside the constructor balances its own function result, so the
  ! assignment below leaves `field` holding exactly one reference to the model buffer. `field` is an
  ! intent(out) dummy (the caller's variable), NOT a function result, so we must NOT call field%return()
  ! here — the `x = atlas_Field(cptr); call x%return()` idiom is only correct for a function result. An extra
  ! return() would over-detach and, once the caller finalises `field`, drop the model buffer to zero owners
  ! and free it prematurely (a finaliser-dependent fault, so masked where FCKIT_HAVE_FINAL=0).
  err = plume_data_get_shared_atlas_field_interf(handle%impl, c_str(name), field_ptr)
  field = atlas_Field(field_ptr)
end function

! this function returns a metadata parameter (int)
function plume_data_get_int( handle, name, val ) result (err)
  use iso_c_binding, only: c_int
  class(plume_data_view), intent(inout) :: handle
  character(*), intent(in) :: name
  integer(c_int), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_int_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_bool( handle, name, val ) result (err)
  use iso_c_binding, only: c_bool, c_int
  class(plume_data_view), intent(inout) :: handle
  character(*), intent(in) :: name
  logical(c_bool), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_bool_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_float( handle, name, val ) result (err)
  use iso_c_binding, only: c_float, c_int
  class(plume_data_view), intent(inout) :: handle
  character(*), intent(in) :: name
  real(c_float), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_float_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_double( handle, name, val ) result (err)
  use iso_c_binding, only: c_double, c_int
  class(plume_data_view), intent(inout) :: handle
  character(*), intent(in) :: name
  real(c_double), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_double_interf(handle%impl, c_str(name), val)
end function

function plume_data_print( handle ) result(err)
  class(plume_data_view), intent(inout) :: handle
  integer :: err
  err = plume_data_print_interf(handle%impl)
end function

function plume_data_set_updated( handle, names ) result(err)
  use iso_c_binding, only: c_char, c_int, c_loc
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=:), allocatable, target, intent(in) :: names(:)
  type(c_ptr), allocatable :: c_param_names(:)
  integer :: err, i, count

  count = size(names)
  allocate(c_param_names(count))
  do i = 1, count
    c_param_names(i) = c_loc(names(i))
  end do
  err = plume_data_set_updated_interf(handle%impl, count, c_param_names)
end function

! Write-back API implementations

function plume_data_write_int( handle, name, val ) result(err)
  use iso_c_binding, only: c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  integer(c_int), intent(in) :: val
  integer(c_int) :: err
  err = plume_data_write_int_interf(handle%impl, c_str(name), val)
end function

function plume_data_write_bool( handle, name, val ) result(err)
  use iso_c_binding, only: c_bool, c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  logical(c_bool), intent(in) :: val
  integer(c_int) :: err
  err = plume_data_write_bool_interf(handle%impl, c_str(name), val)
end function

function plume_data_write_float( handle, name, val ) result(err)
  use iso_c_binding, only: c_float, c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_float), intent(in) :: val
  integer(c_int) :: err
  err = plume_data_write_float_interf(handle%impl, c_str(name), val)
end function

function plume_data_write_double( handle, name, val ) result(err)
  use iso_c_binding, only: c_double, c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  real(c_double), intent(in) :: val
  integer(c_int) :: err
  err = plume_data_write_double_interf(handle%impl, c_str(name), val)
end function

function plume_data_write_atlas_field_copy( handle, name, value ) result(err)
  use iso_c_binding, only: c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  type(atlas_Field), intent(in) :: value
  integer(c_int) :: err
  err = plume_data_write_atlas_field_interf(handle%impl, c_str(name), value%c_ptr())
end function

subroutine plume_data_write_atlas_field_scope( handle, name, scope, err )
  use iso_c_binding, only: c_char, c_int
  class(plume_data_view), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  type(plume_write_scope), intent(out) :: scope
  integer(c_int), intent(out), optional :: err
  integer(c_int) :: local_err
  local_err = plume_data_write_scope_begin_interf(handle%impl, c_str(name), scope%scope, scope%field_ptr)
  if (present(err)) err = local_err
end subroutine

! Alias the staged model buffer into an atlas_field for in-place mutation (mirrors get_shared_atlas_field).
subroutine plume_write_scope_view( scope, field )
  class(plume_write_scope), intent(inout) :: scope
  type(atlas_field), intent(out) :: field
  ! Wrap the borrowed (model-owned) FieldImpl with the canonical atlas cptr constructor: atlas_Field(cptr)
  ! attaches a reference, and the return() inside the constructor balances its own function result, so the
  ! assignment below leaves `field` holding exactly one reference to the model buffer. `field` is an
  ! intent(out) dummy (the caller's variable), NOT a function result, so we must NOT call field%return()
  ! here — the `x = atlas_Field(cptr); call x%return()` idiom is only correct for a function result. An extra
  ! return() would over-detach and, once the caller finalises `field`, drop the model buffer to zero owners
  ! and free it prematurely (a finaliser-dependent fault, so masked where FCKIT_HAVE_FINAL=0).
  field = atlas_Field(scope%field_ptr)
end subroutine

! Commit the write-back early and neutralise the scope so the final binding does not commit it a second time.
function plume_write_scope_commit( scope ) result(err)
  use iso_c_binding, only: c_int, c_associated, c_null_ptr
  class(plume_write_scope), intent(inout) :: scope
  integer(c_int) :: err
  err = 0_c_int
  if (c_associated(scope%scope)) then
    err = plume_data_write_scope_commit_interf(scope%scope)
    scope%scope     = c_null_ptr
    scope%field_ptr = c_null_ptr
  end if
end function

! Scope-exit finaliser — commits any un-committed write-back (the RAII auto-commit). Best-effort: a final procedure
! cannot return a status, so a commit error here is dropped; call commit() explicitly to observe the status.
subroutine plume_write_scope_final( scope )
  use iso_c_binding, only: c_int, c_associated, c_null_ptr
  type(plume_write_scope), intent(inout) :: scope
  integer(c_int) :: err
  if (c_associated(scope%scope)) then
    err = plume_data_write_scope_commit_interf(scope%scope)
    scope%scope     = c_null_ptr
    scope%field_ptr = c_null_ptr
  end if
end subroutine

function plume_data_pending_writebacks( handle ) result(pending_str)
  use iso_c_binding, only: c_ptr
  use plume_utils_module, only: plume_free_string
  class(plume_data), intent(inout) :: handle
  character(:), allocatable, target :: pending_str
  type(c_ptr) :: names_ptr
  integer :: err
  err = plume_data_pending_writebacks_interf(handle%impl, names_ptr)
  pending_str = fortranise_cstr(names_ptr)
  call plume_free_string(names_ptr)
end function

function plume_data_acknowledge_writeback( handle, name ) result(err)
  use iso_c_binding, only: c_int
  class(plume_data), intent(inout) :: handle
  character(kind=c_char,len=*), intent(in) :: name
  integer(c_int) :: err
  err = plume_data_acknowledge_writeback_interf(handle%impl, c_str(name))
end function


end module