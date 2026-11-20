! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.

! ===========================================================================================
! Example 4: hook points, and per-hook access to parameters (read-only / writable)
!
! A hook point is a named place of the model time step from which the model calls Plume. This
! toy model has two of them, around its "physics":
!
!      for each step:
!          manager%run("pre-physics")     <- plugins bound to "pre-physics" run here
!          ... physics ...
!          manager%run("post-physics")    <- plugins bound to "post-physics" run here
!
! It shares three parameters with the plugins. Each of them is offered at both hook points, with
! an access mode that depends on the hook point:
!
!      parameter    | pre-physics | post-physics
!      -------------+-------------+-------------
!      step         | read-only   | read-only
!      forcing      | writable    | read-only
!      temperature  | read-only   | writable
!
! What a plugin actually gets is the outcome of the negotiation between these offers and what
! the plugin requests (here through plume_config_hooks_writeback.yml, for the plugin in
! plugin_hooks.{h,cc}). A plugin is only granted write access to a parameter at a hook point if
! the model offers the parameter writable *at that hook point*.
!
! See README.md in this directory for a detailed description.
! ===========================================================================================
program my_program

use iso_c_binding, only : c_null_char, c_int, c_double, c_bool
use plume_module

implicit none

character(1024) :: plume_config_path

! plume structures
type(plume_protocol) :: offers
type(plume_manager) :: manager
type(plume_data) :: data

! Model variables. They are provided to Plume by reference: plugins read them - and, where they
! have been granted write access, write them - directly in the memory of the model.
integer(c_int), target :: step = 0
real(c_double), target :: forcing = 0.0_c_double
real(c_double), target :: temperature = 280.0_c_double

logical(c_bool) :: hook_active

write(*,*) "*** Running Example 4 (Fortran interface) ***"

CALL get_command_argument(1, plume_config_path)

! init
call plume_check(plume_initialise())

! ----------------------------------- Offers -----------------------------------
call plume_check(offers%initialise())

! 1) Register the hook points that this model is able to call Plume from.
!    The "default" hook point is always registered on top of these: it is the one that the
!    argument-less manager%run() targets, and the one that plugins declaring no hook point are
!    bound to.
call plume_check(offers%offer_hook("pre-physics", "before the physics is computed"))
call plume_check(offers%offer_hook("post-physics", "after the physics has been computed"))

! 2) Offer the parameters: offer_<type>(name, avail, comment [, hooks] [, writable])
!
!    Without the optional arguments, a parameter is offered read-only at every hook point.
call plume_check(offers%offer_int("step", "always", "current step of the model"))

!    "hooks" is a comma-separated list of hook points: the parameter is offered at these hook
!    points only. "writable" tells whether plugins may write the parameter back there. Offering
!    the same parameter several times merges the definitions hook point by hook point, which is
!    how a parameter is offered writable at some hook points and read-only at others.
!
!    "forcing" may be set by plugins before the physics; afterwards it can only be read.
call plume_check(offers%offer_double("forcing", "always", "forcing applied by the physics", &
                                   & hooks="pre-physics", writable=.true.))
call plume_check(offers%offer_double("forcing", "always", "forcing applied by the physics", &
                                   & hooks="post-physics"))

!    "temperature" can only be read before the physics; plugins may correct it afterwards.
call plume_check(offers%offer_double("temperature", "always", "temperature of the model", &
                                   & hooks="pre-physics"))
call plume_check(offers%offer_double("temperature", "always", "temperature of the model", &
                                   & hooks="post-physics", writable=.true.))

!    Note: a parameter that is not offered at a hook point at all (e.g. offered with
!    hooks="post-physics" only) cannot be requested there, not even read-only: a plugin needing
!    it at "pre-physics" would be rejected.

! ----------------------------------- Negotiation -----------------------------------
! Each plugin is accepted or rejected as a whole. With hook points, a plugin is accepted if:
!  - all the hook points it asks for are registered by the model;
!  - each parameter it requests is offered at every hook point it needs it at;
!  - wherever it requests a parameter writable, the model offers it writable at that hook point
!    (and the write-back policy of the configuration is not "disabled").
call plume_check(manager%initialise())
call plume_check(manager%configure(trim(plume_config_path)//c_null_char))
call plume_check(manager%negotiate(offers))

! After negotiation the model can query the outcome, hook point by hook point:
!  - is_hook_active(): is any plugin bound to this hook point? (if not, the model can skip
!    whatever it does only to prepare that call);
!  - active_params_at_hook(): parameters that the plugins use at this hook point, i.e. what the
!    model has to keep up to date before calling it;
!  - writable_params_at_hook(): parameters that the plugins may write at this hook point, i.e.
!    what the model may have to ingest after calling it.
! The lists are returned as comma-separated strings.
write(*,*) "[model] registered hook points: ", manager%registered_hooks()
call report_hook("pre-physics")
call report_hook("post-physics")
call report_hook("default")

! ----------------------------------- Data -----------------------------------
call plume_check(data%initialise())
call plume_check(data%provide_int("step", step))
call plume_check(data%provide_double("forcing", forcing))
call plume_check(data%provide_double("temperature", temperature))

! pass data to the plugins
call plume_check(manager%feed_plugins(data))

! ----------------------------------- Run -----------------------------------
do step = 1, 5

  ! --- hook point "pre-physics" ---
  ! Only "forcing" is open for writing during this call: a plugin trying to write "temperature"
  ! here would get an error.
  call plume_check(manager%run("pre-physics"))
  call ingest_writeback("forcing", "pre-physics")

  ! the "physics" of this model, which uses the forcing set by the plugin
  temperature = temperature + forcing
  write(*,*) "[model] step ", step, ": physics done, temperature=", temperature

  ! --- the default hook point ---
  ! No plugin of this example is bound to it, but a model that adopts hook points should keep
  ! this call: plugins that declare no hook point (e.g. all the plugins written before hook
  ! points existed) only run here.
  call plume_check(manager%run())

  ! --- hook point "post-physics" ---
  ! The access modes are now swapped: "temperature" is open for writing, "forcing" is not.
  call plume_check(manager%run("post-physics"))
  call ingest_writeback("temperature", "post-physics")

  write(*,*) "[model] step ", step, ": end of step, temperature=", temperature
enddo

! finalise
call plume_check(data%finalise())
call plume_check(manager%finalise())
call plume_check(offers%finalise())
call plume_check(plume_finalise())

write(*,*) "*** Example 4 complete. ***"
write(*,*)

contains

! Report the outcome of the negotiation for a hook point
subroutine report_hook(hook)
  character(len=*), intent(in) :: hook
  call plume_check(manager%is_hook_active(hook, hook_active))
  write(*,*) "[model] hook point '", hook, "' active: ", hook_active, &
           & "; params used: [", manager%active_params_at_hook(hook), &
           & "]; params that may be written back: [", manager%writable_params_at_hook(hook), "]"
end subroutine

! Ingest a parameter if the plugins have written it back during the last manager%run(hook) call.
!
! Each manager%run(hook) call is one write-back cycle: when it returns, the parameters that the
! plugins have written during that call are "pending" (data%pending_writebacks() returns their
! names as a comma-separated string). The model must acknowledge each of them, which tells Plume
! that the model has taken the new value on board. A parameter that is still pending cannot be
! written again: the next manager%run(hook) call that opens it for writing would fail. Ingesting
! right after each manager%run(hook) call is therefore the pattern to follow.
subroutine ingest_writeback(name, hook)
  character(len=*), intent(in) :: name
  character(len=*), intent(in) :: hook
  character(:), allocatable :: pending

  pending = data%pending_writebacks()
  if (index(pending, name) > 0) then

    ! This is where a model would copy the new value back into its own data structures. Nothing
    ! to do in this example: the parameters are provided to Plume by reference (see provide_*
    ! above), so the plugins have written directly into the model variables.
    write(*,*) "[model] '", name, "' was written back at hook point '", hook, "'"

    call plume_check(data%acknowledge_writeback(name))
  end if
end subroutine

end program
