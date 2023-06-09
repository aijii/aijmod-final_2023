!
!   Copyright (C) 2020 Shamit Som shamitsom@gmail.com
!
!   This program is free software: you can redistribute it and/or modify
!   it under the terms of the GNU General Public License as published by
!   the Free Software Foundation, either version 3 of the License, or
!   (at your option) any later version.
!
!   This program is distributed in the hope that it will be useful,
!   but WITHOUT ANY WARRANTY; without even the implied warranty of
!   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
!   GNU General Public License for more details.
!

! Hook and inject Pull2DFloat
.section Pull2DFloatRamInjectStart,"ax"

_Pull2DFloatPatch:
    fmov    fr4, fr0    ! need to start patch here to ensure word alignment

    mov.l   Pull2DFloatDynRamHook, r2   ! inject here
    jsr     @r2
    mov     r0, r5 ! use r5 to store X index

    mova    Pull2DFloatDynRamHook + 4, r0
    mov.l   @(r0,r3), r2 ! injected subroutine stores table datatype into r3
    jsr     @r2
    mov     r5, r0 ! recall X index to r0 before jumping final subroutine

    ! here and below is verbatim OEM code, but patch needs to include all of
    ! this, as we need to shift the end of the routine up by two bytes to fit
    ! a pointer to our injected code in the data hunk of this section, while
    ! still keeping the datatype-specific subroutine pointers in their original
    ! location... tight fit!

    tst     r3, r3
    bt/s    _Pull2DFloatPatchExit
    fmov    fr2, fr1

    add     #0xC, r4
    fmov.s  @r4+, fr0
    fmov.s  @r4, fr1
    fmac    fr0, fr2, fr1

_Pull2DFloatPatchExit:
    lds.l   @r15+, pr
    rts
    fmov    fr1, fr0

! this must line up exactly one dword before the `hPull2DFloatRamInjectEnd`
Pull2DFloatDynRamHook:
    .long _Pull2DFloatDynRamHook

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

! Hook and inject Pull3DFloat
.section Pull3DFloatRamInjectStart,"ax"

_Pull3DFloatRamPatch:
    !don't touch r2, r3, r4, fr0, fr1
    mov.l   Pull3DFloatDynRamHook, r5
    mova    Pull3DFloatDynRamHook + 4, r0
    jsr     @r5
    mov.l   @(0xC, r4), r1

    jsr     @r5
    mov.w   @(0,r4), r0

    tst     r5, r5
    bt/s    _Pull3DFloatRamPatchExit
    fmov    fr2, fr1

    add     #0x14, r4
    fmov.s  @r4+, fr0
    fmov.s  @r4, fr1
    fmac    fr0, fr2, fr1

_Pull3DFloatRamPatchExit:
    fmov    fr1, fr0
    lds.l   @r15+, mach
    lds.l   @r15+, pr
    rts
    lds.l   @r15+, macl

Pull3DFloatDynRamHook:
    .long _Pull3DFloatDynRamHook

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

! injected code, stores RAM table address in `r1` if valid table has been
! allocated in RAM, otherwise sets `r1` to point to the original ROM table
.section RomHole_Code,"ax"

_Pull2DFloatDynRamHook:
! on enter (simply don't write to these registers!):
!   - r4 contains table definition address
!   - r5 contains the X index

    mov.l   _pMaxRAMTables, r3      ! pointer to max RAM tables -> r3
    mov.l   @r3+, r0                ! max RAM tables -> r0, r3 points to ROM header array
    mov.l   @(0x8, r4), r1          ! table ROM address -> r1
    cmp/eq  #0, r0                  ! set T register if no tables allocated

! loop over ROMAddress array to check for match
_Pull2DFloatDynAddr:
    bt/s    _Pull2DFloatDynExit     ! exit loop and use ROM address if no match found
    mov.l   @r3+, r2                ! romaddress at current header -> r2, r3 points to next header
    cmp/eq  r2, r1                  ! check if header contains desired ROM address
    bf/s    _Pull2DFloatDynAddr     ! repeat check for next header
    dt      r0                      ! decrement loop counter

! found a matching address, now need to check if it's valid
_Pull2DFloatDynValid:
    mov.l   _pROMtoRAMArrayOffs, r0 ! pointer to offset from ROM to RAM headers -> r0
    mov.l   @r0, r0                 ! dword offset from ROM to RAM headers -> r0
    mov.l   @(r0, r3), r2           ! potential RAM table address -> r2
    mov.b   @(r0, r3), r0           ! flag bytes into -> r0
    cmp/eq  #0xFF, r0               ! 0xFF###### is valid, T -> 1 if valid
    bf      _Pull2DFloatDynExit     ! if invalid, jump directly to exit
    mov     r2, r1                  ! valid RAM table address -> r1

_Pull2DFloatDynExit:
! on exit:
!   - r1 should contain data table base address (determined in routine)
!   - r3 should contain data table datatype (determine here)
!   - r4 should contain the table definition address (untouched in routine)
!   - r5 should contain the X index (untouched in routine)
    mov.b   @(0x2,r4), r0
    rts
    extu.b  r0, r3

_Pull3DFloatDynRamHook:
! on enter (simply don't write to these registers!):
!   - r0 contains the address to the subroutine table needed after return
!   - r1 contains the table ROM address
!   - r2 contains the X index
!   - r3 contains the Y index
!   - r4 contains table definition address

    mov     r0, r6                  ! datatype subroutine table -> r6
    mov.l   _pMaxRAMTables, r5      ! pointer to max RAM tables -> r5
    mov.l   @r5+, r0                ! max RAM tables -> r0, r5 points to ROM header array
    cmp/eq  #0, r0                  ! set T register if no tables allocated

! loop over ROMAddress array to check for match
_Pull3DFloatDynAddr:
    bt/s    _Pull3DFloatDynExit     ! exit loop and use ROM address if no match found
    mov.l   @r5+, r7                ! romaddress at current header -> r7, r5 points to next header
    cmp/eq  r7, r1                  ! check if header contains desired ROM address
    bf/s    _Pull3DFloatDynAddr     ! repeat check for next header
    dt      r0                      ! decrement loop counter

! found a matching address, now need to check if it's valid
_Pull3DFloatDynValid:
    mov.l   _pROMtoRAMArrayOffs, r0 ! pointer to offset from ROM to RAM header -> r0
    mov.l   @r0, r0                 ! dword offset from ROM to RAM header -> r0
    mov.l   @(r0, r5), r7           ! potential RAM table address -> r7
    mov.b   @(r0, r5), r0           ! flag bytes into -> r0
    cmp/eq  #0xFF, r0               ! 0xFF###### is valid, T -> 1 if valid
    bf      _Pull3DFloatDynExit     ! if invalid, jump directly to exit
    mov     r7, r1                  ! valid RAM table address -> r1

_Pull3DFloatDynExit:
! on exit:
!   - r1 should contain data table base address (determined in routine)
!   - r2 should contain the X index (untouched in routine)
!   - r3 should contain the Y index (untouched in routine)
!   - r4 should contain the table definition address (untouched in routine)
!   - r5 should contain the address to subroutine corresponding to the table datatype (determine here)
    mov     #0x10, r5
    add     r4, r5
    mov.b   @r5, r0
    rts
    mov.l   @(r0, r6), r5

! need to compile `DynamicRuntime.c` manually and include here to get
! the correct symbols for the RAM Table headers, which are dynamically
! allocated to the end of `pRamVariables`. I can't think of a better way
! to handle this...
.include "DynamicRamtune.s"
