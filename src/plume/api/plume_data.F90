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

use atlas_module

! PLUME data ("C"-handle wrapper)
type, public :: plume_data
    type(c_ptr) :: impl = c_null_ptr
    logical :: is_finalised = .false.
contains
    procedure :: initialise          => plume_data_create_handle
    procedure :: initialise_from_ptr => plume_data_create_handle_from_ptr

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
    
    procedure :: get_int                    => plume_data_get_int
    procedure :: get_bool                   => plume_data_get_bool
    procedure :: get_float                  => plume_data_get_float
    procedure :: get_double                 => plume_data_get_double
    procedure :: get_shared_atlas_field     => plume_data_get_shared_atlas_field

    procedure :: print => plume_data_print
    procedure :: finalise => plume_data_delete_handle

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
  class(plume_data), intent(inout) :: handle
  type(c_ptr), intent(in), value :: data_c_ptr
  integer :: err
  err = plume_data_create_handle_from_ptr_interf(handle%impl, data_c_ptr)
end function

function plume_data_delete_handle( handle ) result(err)
  class(plume_data), intent(inout) :: handle
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
  class(plume_data), intent(inout) :: handle
  character(*), intent(in) :: name
  type(atlas_field), intent(inout) :: field
  type(c_ptr) :: field_ptr
  integer(c_int) :: err

  ! Set the internal c_ptr of the field
  err = plume_data_get_shared_atlas_field_interf(handle%impl, c_str(name), field_ptr)
  call field%reset_c_ptr(field_ptr)
end function

! this function returns a metadata parameter (int)
function plume_data_get_int( handle, name, val ) result (err)
  use iso_c_binding, only: c_int
  class(plume_data), intent(inout) :: handle
  character(*), intent(in) :: name
  integer(c_int), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_int_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_bool( handle, name, val ) result (err)
  use iso_c_binding, only: c_bool, c_int
  class(plume_data), intent(inout) :: handle
  character(*), intent(in) :: name
  logical(c_bool), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_bool_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_float( handle, name, val ) result (err)
  use iso_c_binding, only: c_float, c_int
  class(plume_data), intent(inout) :: handle
  character(*), intent(in) :: name
  real(c_float), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_float_interf(handle%impl, c_str(name), val)
end function

function plume_data_get_double( handle, name, val ) result (err)
  use iso_c_binding, only: c_double, c_int
  class(plume_data), intent(inout) :: handle
  character(*), intent(in) :: name
  real(c_double), intent(inout) :: val
  integer(c_int) :: err
  err = plume_data_get_double_interf(handle%impl, c_str(name), val)
end function

function plume_data_print( handle ) result(err)
  class(plume_data), intent(inout) :: handle
  integer :: err
  err = plume_data_print_interf(handle%impl)
end function


end module