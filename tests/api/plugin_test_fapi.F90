! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plugin_interface_module_fapi

use iso_c_binding, only : c_ptr, c_null_char, c_loc

! wrapper of model data
use fckit_configuration_module, only : fckit_configuration
use plume_module, only : plume_check
use plume_data_module, only : plume_data

implicit none
private

! Model data available for the plugincore
type(plume_data), public :: fapi_data
type(fckit_configuration), public :: fapi_configuration

contains

! Forward to user-defined methods
subroutine plugincore_setup_fapi(conf_cptr, data_cptr) &
bind(C, name="plugincore_setup_fapi")
    type(c_ptr), intent(in), value :: data_cptr
    type(c_ptr), intent(in), value :: conf_cptr
    call plume_check(fapi_data%initialise_from_ptr(data_cptr))
    call fapi_configuration%reset_c_ptr(conf_cptr)
    
    write(*,*) "plugincore_setup_fapi called, with data"
    call plume_check(fapi_data%print())
end subroutine

subroutine plugincore_run_fapi() &
bind(C, name="plugincore_run_fapi")
    write(*,*) "plugincore_run_fapi called, with data"
    call plume_check(fapi_data%print())
end subroutine

subroutine plugincore_teardown_fapi() &
bind(C, name="plugincore_teardown_fapi")
    write(*,*) "plugincore_teardown_fapi called, with data"
    call plume_check(fapi_data%print())
    call plume_check(fapi_data%finalise())
end subroutine


end module