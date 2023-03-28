! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_protocol_module

    use iso_c_binding
    use plume_utils_module, only : fortranise_cstr
    
    implicit none
    private
    
    
    ! plugins manager ("C"-handle wrapper)
    type, public :: plume_protocol
        type(c_ptr) :: impl = c_null_ptr
        logical :: is_finalised = .false.
    contains
        procedure :: initialise         => plume_protocol_create_handle
        
        procedure :: offer_int          => plume_protocol_offer_int
        procedure :: offer_bool         => plume_protocol_offer_bool
        procedure :: offer_float        => plume_protocol_offer_float
        procedure :: offer_double       => plume_protocol_offer_double
        procedure :: offer_atlas_field  => plume_protocol_offer_atlas_field

        procedure :: finalise           => plume_protocol_delete_handle
    end type
    
    
    interface
    
    
    function plume_protocol_create_handle_interf( handle_impl ) result(err) &
        & bind(C,name="plume_protocol_create_handle")
        use iso_c_binding, only: c_int, c_ptr
        type(c_ptr), intent(out) :: handle_impl
        integer(c_int) :: err
    end function

    function plume_protocol_delete_handle_interf( handle_impl ) result(err) &
        & bind(C,name="plume_protocol_delete_handle")
        use iso_c_binding, only: c_int, c_ptr
        type(c_ptr), intent(in), value :: handle_impl
        integer(c_int) :: err
    end function    
    
    function plume_protocol_offer_int_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_int")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), intent(in) :: name
        character(c_char), intent(in) :: avail
        character(c_char), intent(in) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_bool_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_bool")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), intent(in) :: name
        character(c_char), intent(in) :: avail
        character(c_char), intent(in) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_float_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_float")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), intent(in) :: name
        character(c_char), intent(in) :: avail
        character(c_char), intent(in) :: comment
        integer(c_int) :: err
    end function
    
    function plume_protocol_offer_double_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_double")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), intent(in) :: name
        character(c_char), intent(in) :: avail
        character(c_char), intent(in) :: comment
        integer(c_int) :: err
    end function
    
    function plume_protocol_offer_atlas_field_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_atlas_field")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), intent(in) :: name
        character(c_char), intent(in) :: avail
        character(c_char), intent(in) :: comment
        integer(c_int) :: err
    end function    

    end interface


    contains


    function plume_protocol_create_handle( handle ) result(err)
        class(plume_protocol), intent(inout) :: handle
        integer :: err
        err = plume_protocol_create_handle_interf(handle%impl)
    end function

    function plume_protocol_offer_int( handle, name, avail, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_null_char
        class(plume_protocol), intent(inout) :: handle
        character(*), intent(in) :: name
        character(*), intent(in) :: avail
        character(*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_int_interf(handle%impl, name//c_null_char, avail//c_null_char, comment//c_null_char )
    end function

    function plume_protocol_offer_bool( handle, name, avail, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_null_char
        class(plume_protocol), intent(inout) :: handle
        character(*), intent(in) :: name
        character(*), intent(in) :: avail
        character(*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_bool_interf(handle%impl, name//c_null_char, avail//c_null_char, comment//c_null_char )
    end function

    function plume_protocol_offer_float( handle, name, avail, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_null_char
        class(plume_protocol), intent(inout) :: handle
        character(*), intent(in) :: name
        character(*), intent(in) :: avail
        character(*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_float_interf(handle%impl, name//c_null_char, avail//c_null_char, comment//c_null_char )
    end function
    
    function plume_protocol_offer_double( handle, name, avail, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_null_char
        class(plume_protocol), intent(inout) :: handle
        character(*), intent(in) :: name
        character(*), intent(in) :: avail
        character(*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_double_interf(handle%impl, name//c_null_char, avail//c_null_char, comment//c_null_char )
    end function
    
    function plume_protocol_offer_atlas_field( handle, name, avail, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_null_char
        class(plume_protocol), intent(inout) :: handle
        character(*), intent(in) :: name
        character(*), intent(in) :: avail
        character(*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_atlas_field_interf(handle%impl, name//c_null_char, avail//c_null_char, comment//c_null_char )
    end function    

    function plume_protocol_delete_handle( handle ) result(err)
      class(plume_protocol), intent(inout) :: handle
      integer :: err
      if (handle%is_finalised .eqv. .false.) then
        err = plume_protocol_delete_handle_interf(handle%impl)
        handle%is_finalised = .true.
      end if
    end function


end module