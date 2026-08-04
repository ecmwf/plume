! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

! Compile-time enforcement fixture for the Fortran plugin/model data-access contract (PLUME-75).
!
! plume_data_view (plugin-facing) binds ONLY the plugin-legal procedures; plume_data (model-facing)
! extends it with the model-only mutators (update_*/provide_*/create_*/...). A Fortran plugin therefore
! holds a plume_data_view and MUST NOT be able to call a mutator.
!
! This program deliberately calls a model-only binding (update_int) on a plume_data_view. It MUST FAIL
! to compile. The build target is EXCLUDE_FROM_ALL and the accompanying CTest expects the build to fail
! (WILL_FAIL). If enforcement ever regresses, this program compiles and the test then fails.
program should_not_compile_fapi
    use iso_c_binding, only : c_int, c_null_char
    use plume_data_module, only : plume_data_view
    implicit none

    type(plume_data_view) :: view
    integer :: err

    ! update_int is bound on plume_data, NOT on plume_data_view — this line must not compile.
    err = view%update_int("x"//c_null_char, 1_c_int)

end program should_not_compile_fapi
