! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
program my_program

use iso_c_binding, only : c_bool, c_null_char
use plume_module
use atlas_module, only : atlas_Field, atlas_integer

implicit none

character(1024) :: plume_config_path

type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

integer :: param_i = 0
integer :: param_j = 10
integer :: param_k = 100

character(*), parameter :: field_name = "field_dummy_1"//c_null_char
type(atlas_Field) :: field

logical(c_bool) :: is_param_requested
integer :: iter


write(*,*) "*** Running Example 2 (Fortran interface) ***\n"

CALL get_command_argument(1, plume_config_path)

! init plugins library
call plume_check(plume_initialise())

! make offers
call plume_check(offers%initialise())
call plume_check(offers%offer_int("I", "always", "this is param I"))
call plume_check(offers%offer_int("J", "always", "this is param J"))
call plume_check(offers%offer_int("K", "always", "this is param K"))
call plume_check(offers%offer_atlas_field(field_name, "on-request", "this is an atlas field"))

! negotiate
call plume_check(manager%initialise())
call plume_check(manager%configure(trim(plume_config_path)//c_null_char))
call plume_check(manager%negotiate(offers))

! fill in data
call plume_check(data%initialise())
call plume_check(data%provide_int("I", param_i) )
call plume_check(data%provide_int("J", param_j) )
call plume_check(data%provide_int("K", param_k) )

! NOTE: After the negotiation, the manager knows which parameters 
! have been requested by all activated plugins (i.e. plugins that 
! negotiated with success)
! 
! => This information can be used to insert "on-request" params 
!    (i.e parameters that are inserted only if requested by plugins)
!
! For example, if "field_dummy_1" has been requested, then insert it:
call plume_check( manager%is_param_requested(field_name, is_param_requested) )
if ( is_param_requested .eqv. .true. ) then
  field = atlas_Field(field_name, atlas_integer(), (/0,10/))
  call plume_check( data%provide_atlas_field_shared(field_name, field) )
endif

call plume_check(data%print())

! pass data to the plugins
call plume_check(manager%feed_plugins(data))

! Run the model for 10 iterations
do iter=1,10
  call plume_check(manager%run())

  ! e.g. update parameters
  param_i = param_i + 1
  param_j = param_j + 1
  param_k = param_k + 1
enddo

! finalise
call plume_check(data%finalise())
call plume_check(manager%finalise())
call plume_check(offers%finalise())
call plume_check(plume_finalise())

write(*,*) "*** Example 2 complete. ***"
write(*,*)

contains

end program
    