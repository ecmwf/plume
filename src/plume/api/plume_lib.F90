! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plume_lib_module

    use iso_c_binding
    use fckit_array_module
    use fckit_c_interop_module

    implicit none
    private

    ! public interface
    public :: plume_initialise
    public :: plume_finalise    

    
    interface
    
    function plume_initialise_interf( argc, argv ) result(err) &
        & bind(C,name="plume_initialise")
        use iso_c_binding, only: c_int, c_ptr
        integer(c_int), value :: argc
        type(c_ptr) :: argv(15)
        integer(c_int) :: err
    end function    
    
    function plume_finalise_interf( ) result(err) &
        & bind(C,name="plume_finalise")
        use iso_c_binding
        integer(c_int) :: err
    end function
    
    end interface
    
    interface plume_initialise
      module procedure plume_initialise_func
    end interface
    
    interface plume_finalise
      module procedure plume_finalise_func
    end interface
    

    contains
    
    function plume_initialise_func( ) result(err)
        use iso_c_binding, only: c_int, c_ptr
        integer(c_int) :: argc
        type(c_ptr) :: argv(15)
        integer :: err
        call get_c_commandline_arguments(argc,argv)
        err = plume_initialise_interf( argc,argv )
    end function

    function plume_finalise_func( ) result(err)
        integer :: err
        err = plume_finalise_interf( )
    end function
        
    end module