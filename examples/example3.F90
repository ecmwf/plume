! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
program my_program

use iso_c_binding, only : c_null_char, c_double
use plume_module
use atlas_module, only : atlas_Field, atlas_integer


implicit none

character(1024) :: plume_config_path

! plume structures
type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

! data
type(atlas_Field) :: field

integer :: config_param_1 = 9
real(c_double) :: config_param_2 = 99.99
type(atlas_Field) :: config_param_3


integer :: iter

write(*,*) "*** Running Example 3 (Fortran interface) ***\n"

CALL get_command_argument(1, plume_config_path)

! init
call plume_check(plume_initialise())

! make offers
call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "this is param I"))
call plume_check(offers%offer_int("J", "always", "this is param J"))
call plume_check(offers%offer_int("K", "always", "this is param K"))
call plume_check(offers%offer_atlas_field("field_dummy_1", "always", "this is param K"))

call plume_check(offers%offer_int("config-param-1", "always", "this is param config-param-1"))
call plume_check(offers%offer_double("config-param-2", "always", "this is param config-param-2"))
call plume_check(offers%offer_atlas_field("config-param-3", "always", "this is param config-param-3"))


! negotiate
call plume_check(manager%initialise())
call plume_check(manager%configure(trim(plume_config_path)//c_null_char))
call plume_check(manager%negotiate(offers))

! fill in data
call plume_check(data%initialise())

! make space for new parameters in the data
call plume_check(data%create_int("I", 0) )
call plume_check(data%create_int("J", 10) )
call plume_check(data%create_int("K", 100) )

field = atlas_Field("field_dummy_1", atlas_integer(), (/0,10/))
call plume_check( data%provide_atlas_field_shared("field_dummy_1", field) )

! Some more parameters that will be requested through the configuration
config_param_3 = atlas_Field("config-param-3", atlas_integer(), (/0,10/))
call plume_check(data%provide_int("config-param-1", config_param_1) )
call plume_check(data%provide_double("config-param-2", config_param_2) )
call plume_check(data%provide_atlas_field_shared("config-param-3", config_param_3) )


! pass data to the plugins
call plume_check(manager%feed_plugins(data))

! Run the model for 10 iterations
do iter=1,10

  ! update the internal parameters
  call plume_check(data%update_int("I", 0+iter) )
  call plume_check(data%update_int("J", 10+iter) )
  call plume_check(data%update_int("K", 100+iter) )

  ! run the model..
  call plume_check(manager%run())
enddo

! finalise
call plume_check(data%finalise())
call plume_check(manager%finalise())
call plume_check(offers%finalise())
call plume_check(plume_finalise())

write(*,*) "*** Example 3 complete. ***"
write(*,*)

end program my_program