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
    use fckit_c_interop_module, only : c_str
    use plume_utils_module, only : fortranise_cstr
    
    implicit none
    private
    
    
    ! plugins manager ("C"-handle wrapper)
    type, public :: plume_protocol
        type(c_ptr) :: impl = c_null_ptr
        logical :: is_finalised = .false.
    contains
        procedure :: initialise         => plume_protocol_create_handle
        
        ! offer_<type>(name, avail, comment [, hooks] [, writable]): "hooks" is an optional comma-separated list
        ! of the hook points at which the param is offered (default: at every registered hook point), "writable"
        ! whether plugins may write it back there (default: .false.). Offering the same param several times merges
        ! the definitions per hook point (writable wins), e.g. writable at "H1" and read-only at "H2".
        procedure :: offer_int          => plume_protocol_offer_int
        procedure :: offer_bool         => plume_protocol_offer_bool
        procedure :: offer_float        => plume_protocol_offer_float
        procedure :: offer_double       => plume_protocol_offer_double
        procedure :: offer_atlas_field  => plume_protocol_offer_atlas_field
        procedure :: offer_hook         => plume_protocol_offer_hook

        ! offer_<type>_writable(name, avail, comment [, hooks]): shorthand for offer_<type>(..., writable=.true.)
        procedure :: offer_int_writable         => plume_protocol_offer_int_writable
        procedure :: offer_bool_writable        => plume_protocol_offer_bool_writable
        procedure :: offer_float_writable       => plume_protocol_offer_float_writable
        procedure :: offer_double_writable      => plume_protocol_offer_double_writable
        procedure :: offer_atlas_field_writable => plume_protocol_offer_atlas_field_writable

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
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_bool_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_bool")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_float_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_float")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function
    
    function plume_protocol_offer_double_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_double")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function
    
    function plume_protocol_offer_atlas_field_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_atlas_field")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_int_ext_interf( handle_impl, name, avail, comment, writable, hooks ) result(err) &
        & bind(C,name="plume_protocol_offer_int_ext")
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        logical(c_bool), intent(in), value :: writable
        character(c_char), dimension(*) :: hooks
        integer(c_int) :: err
    end function

    function plume_protocol_offer_bool_ext_interf( handle_impl, name, avail, comment, writable, hooks ) result(err) &
        & bind(C,name="plume_protocol_offer_bool_ext")
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        logical(c_bool), intent(in), value :: writable
        character(c_char), dimension(*) :: hooks
        integer(c_int) :: err
    end function

    function plume_protocol_offer_float_ext_interf( handle_impl, name, avail, comment, writable, hooks ) result(err) &
        & bind(C,name="plume_protocol_offer_float_ext")
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        logical(c_bool), intent(in), value :: writable
        character(c_char), dimension(*) :: hooks
        integer(c_int) :: err
    end function

    function plume_protocol_offer_double_ext_interf( handle_impl, name, avail, comment, writable, hooks ) result(err) &
        & bind(C,name="plume_protocol_offer_double_ext")
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        logical(c_bool), intent(in), value :: writable
        character(c_char), dimension(*) :: hooks
        integer(c_int) :: err
    end function

    function plume_protocol_offer_atlas_field_ext_interf( handle_impl, name, avail, comment, writable, hooks ) result(err) &
        & bind(C,name="plume_protocol_offer_atlas_field_ext")
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        logical(c_bool), intent(in), value :: writable
        character(c_char), dimension(*) :: hooks
        integer(c_int) :: err
    end function

    function plume_protocol_offer_hook_interf( handle_impl, name, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_hook")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_int_writable_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_int_writable")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_bool_writable_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_bool_writable")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_float_writable_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_float_writable")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_double_writable_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_double_writable")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    function plume_protocol_offer_atlas_field_writable_interf( handle_impl, name, avail, comment ) result(err) &
        & bind(C,name="plume_protocol_offer_atlas_field_writable")
        use iso_c_binding, only: c_ptr, c_char, c_int
        type(c_ptr), intent(in), value :: handle_impl
        character(c_char), dimension(*) :: name
        character(c_char), dimension(*) :: avail
        character(c_char), dimension(*) :: comment
        integer(c_int) :: err
    end function

    end interface


    contains


    function plume_protocol_create_handle( handle ) result(err)
        class(plume_protocol), intent(inout) :: handle
        integer :: err
        err = plume_protocol_create_handle_interf(handle%impl)
    end function

    function plume_protocol_offer_int( handle, name, avail, comment, hooks, writable ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        logical, intent(in), optional :: writable
        integer(c_int) :: err
        logical(c_bool) :: writable_c
        if (.not. present(hooks) .and. .not. present(writable)) then
            err = plume_protocol_offer_int_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
            return
        end if
        writable_c = .false._c_bool
        if (present(writable)) writable_c = logical(writable, c_bool)
        if (present(hooks)) then
            err = plume_protocol_offer_int_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str(hooks) )
        else
            err = plume_protocol_offer_int_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str("") )
        end if
    end function

    function plume_protocol_offer_bool( handle, name, avail, comment, hooks, writable ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        logical, intent(in), optional :: writable
        integer(c_int) :: err
        logical(c_bool) :: writable_c
        if (.not. present(hooks) .and. .not. present(writable)) then
            err = plume_protocol_offer_bool_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
            return
        end if
        writable_c = .false._c_bool
        if (present(writable)) writable_c = logical(writable, c_bool)
        if (present(hooks)) then
            err = plume_protocol_offer_bool_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str(hooks) )
        else
            err = plume_protocol_offer_bool_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str("") )
        end if
    end function

    function plume_protocol_offer_float( handle, name, avail, comment, hooks, writable ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        logical, intent(in), optional :: writable
        integer(c_int) :: err
        logical(c_bool) :: writable_c
        if (.not. present(hooks) .and. .not. present(writable)) then
            err = plume_protocol_offer_float_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
            return
        end if
        writable_c = .false._c_bool
        if (present(writable)) writable_c = logical(writable, c_bool)
        if (present(hooks)) then
            err = plume_protocol_offer_float_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str(hooks) )
        else
            err = plume_protocol_offer_float_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str("") )
        end if
    end function
    
    function plume_protocol_offer_double( handle, name, avail, comment, hooks, writable ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        logical, intent(in), optional :: writable
        integer(c_int) :: err
        logical(c_bool) :: writable_c
        if (.not. present(hooks) .and. .not. present(writable)) then
            err = plume_protocol_offer_double_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
            return
        end if
        writable_c = .false._c_bool
        if (present(writable)) writable_c = logical(writable, c_bool)
        if (present(hooks)) then
            err = plume_protocol_offer_double_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str(hooks) )
        else
            err = plume_protocol_offer_double_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str("") )
        end if
    end function
    
    function plume_protocol_offer_atlas_field( handle, name, avail, comment, hooks, writable ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        logical, intent(in), optional :: writable
        integer(c_int) :: err
        logical(c_bool) :: writable_c
        if (.not. present(hooks) .and. .not. present(writable)) then
            err = plume_protocol_offer_atlas_field_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
            return
        end if
        writable_c = .false._c_bool
        if (present(writable)) writable_c = logical(writable, c_bool)
        if (present(hooks)) then
            err = plume_protocol_offer_atlas_field_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str(hooks) )
        else
            err = plume_protocol_offer_atlas_field_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & writable_c, c_str("") )
        end if
    end function

    function plume_protocol_offer_hook( handle, name, comment ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: comment
        integer(c_int) :: err
        err = plume_protocol_offer_hook_interf(handle%impl, c_str(name), c_str(comment) )
    end function

    function plume_protocol_offer_int_writable( handle, name, avail, comment, hooks ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        integer(c_int) :: err
        if (present(hooks)) then
            err = plume_protocol_offer_int_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & .true._c_bool, c_str(hooks) )
        else
            err = plume_protocol_offer_int_writable_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
        end if
    end function

    function plume_protocol_offer_bool_writable( handle, name, avail, comment, hooks ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        integer(c_int) :: err
        if (present(hooks)) then
            err = plume_protocol_offer_bool_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & .true._c_bool, c_str(hooks) )
        else
            err = plume_protocol_offer_bool_writable_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
        end if
    end function

    function plume_protocol_offer_float_writable( handle, name, avail, comment, hooks ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        integer(c_int) :: err
        if (present(hooks)) then
            err = plume_protocol_offer_float_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & .true._c_bool, c_str(hooks) )
        else
            err = plume_protocol_offer_float_writable_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
        end if
    end function

    function plume_protocol_offer_double_writable( handle, name, avail, comment, hooks ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        integer(c_int) :: err
        if (present(hooks)) then
            err = plume_protocol_offer_double_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & .true._c_bool, c_str(hooks) )
        else
            err = plume_protocol_offer_double_writable_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
        end if
    end function

    function plume_protocol_offer_atlas_field_writable( handle, name, avail, comment, hooks ) result(err)
        use iso_c_binding, only: c_ptr, c_char, c_int, c_bool
        class(plume_protocol), intent(inout) :: handle
        character(kind=c_char,len=*), intent(in) :: name
        character(kind=c_char,len=*), intent(in) :: avail
        character(kind=c_char,len=*), intent(in) :: comment
        character(kind=c_char,len=*), intent(in), optional :: hooks
        integer(c_int) :: err
        if (present(hooks)) then
            err = plume_protocol_offer_atlas_field_ext_interf(handle%impl, c_str(name), c_str(avail), c_str(comment), &
                & .true._c_bool, c_str(hooks) )
        else
            err = plume_protocol_offer_atlas_field_writable_interf(handle%impl, c_str(name), c_str(avail), c_str(comment) )
        end if
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