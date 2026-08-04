! (C) Copyright 2023- ECMWF.
!
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
!
! In applying this licence, ECMWF does not waive the privileges and immunities
! granted to it by virtue of its status as an intergovernmental organisation nor
! does it submit to any jurisdiction.
module plugin_writeback_fapi_module

use iso_c_binding, only : c_ptr, c_null_char
use plume_module, only : plume_check
use plume_data_module, only : plume_data_view

implicit none
private

type(plume_data_view), public :: writeback_fapi_data

contains

subroutine plugincore_setup_writeback_fapi(conf_cptr, data_cptr) &
bind(C, name="plugincore_setup_writeback_fapi")
    type(c_ptr), intent(in), value :: conf_cptr
    type(c_ptr), intent(in), value :: data_cptr
    call plume_check(writeback_fapi_data%initialise_from_ptr(data_cptr))
end subroutine

subroutine plugincore_run_writeback_fapi() &
bind(C, name="plugincore_run_writeback_fapi")
    use iso_c_binding, only: c_double, c_int
    use atlas_module, only: atlas_Field, atlas_integer
    use plume_data_module, only: plume_write_scope
    type(atlas_Field) :: current, update
    integer(c_int), pointer :: cdata(:), udata(:)
    integer :: i, n
    call plume_check(writeback_fapi_data%write_int("FORT_W_INT"//c_null_char, 99_c_int))
    call plume_check(writeback_fapi_data%write_double("FORT_W_DOUBLE"//c_null_char, real(1.23d0, kind=c_double)))

    ! Build a distinct field carrying updated values (sized from the provided field) and write it back. The
    ! write-back must copy the data into the model's own buffer in place rather than rebind the handle.
    call plume_check(writeback_fapi_data%get_shared_atlas_field("FORT_W_FIELD", current))
    call current%data(cdata)
    n = size(cdata)
    update = atlas_Field("FORT_W_FIELD", atlas_integer(), (/n/))
    call update%data(udata)
    do i = 1, n
        udata(i) = i * 100_c_int
    end do
    call plume_check(writeback_fapi_data%write_atlas_field_copy("FORT_W_FIELD", update))

    ! Copy-free, in-place write-back via the RAII WriteScope: stage the scope, alias the model's own buffer with
    ! view(), read-modify-write in place, and let the auto-commit fire when the block (and thus the scope) ends.
    block
        type(plume_write_scope)  :: scope
        type(atlas_Field)        :: staged
        integer(c_int), pointer  :: sdata(:)
        integer                  :: j
        integer(c_int)           :: scope_err
        call writeback_fapi_data%write_atlas_field_scope("FORT_W_SCOPE_FIELD", scope, scope_err)
        call plume_check(scope_err)
        call scope%view(staged)
        call staged%data(sdata)
        do j = 1, size(sdata)
            sdata(j) = sdata(j) + 1000_c_int  ! in-place read-modify-write on the model buffer
        end do
    end block  ! scope finalised here → auto-commit of the in-place write-back

    ! Same copy-free path but with an EXPLICIT commit(): the value must land, and the block-exit finaliser must be a
    ! safe no-op afterwards (the c_associated guard neutralises the scope so it is not committed/freed a second time).
    block
        type(plume_write_scope)  :: scope2
        type(atlas_Field)        :: staged2
        integer(c_int), pointer  :: sdata2(:)
        integer                  :: k
        integer(c_int)           :: scope2_err, commit_err
        call writeback_fapi_data%write_atlas_field_scope("FORT_W_SCOPE_FIELD2", scope2, scope2_err)
        call plume_check(scope2_err)
        call scope2%view(staged2)
        call staged2%data(sdata2)
        do k = 1, size(sdata2)
            sdata2(k) = sdata2(k) + 500_c_int
        end do
        commit_err = scope2%commit()  ! explicit commit
        call plume_check(commit_err)
    end block  ! finaliser runs but the guard makes the second commit a no-op (no double-commit/double-free)
end subroutine

subroutine plugincore_teardown_writeback_fapi() &
bind(C, name="plugincore_teardown_writeback_fapi")
    call plume_check(writeback_fapi_data%finalise())
end subroutine

end module
