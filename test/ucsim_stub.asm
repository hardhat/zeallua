; ucsim_stub.asm
; A basic test stub to simulate the Zeal-8-bit-OS kernel for ucsim_z80
    .area _BOOT (ABS)

    .org 0x0000
    jp 0x4000       ; Jump to user application loaded by SDCC at 0x4000

    .org 0x0008
    ; Syscall entry point (RST 08h)
    ; L contains the syscall number.
    ; For automated testing, we might trap here or simulate limited I/O.
    ld a, l
    cp #0           ; sys_read
    jr z, _sys_read
    cp #1           ; sys_write
    jr z, _sys_write
    cp #15          ; sys_exit
    jr z, _sys_exit
    ; Unknown syscall or unhandled
    halt

_sys_read:
    ; Not yet implemented in stub
    ret

_sys_write:
    ; A simple mock to "write" to the simulator.
    ; Often simulators capture writes to specific I/O ports for console out.
    ; For ucsim, we can just return success for now.
    xor a           ; return ERR_SUCCESS
    ret

_sys_exit:
    ; Exit the simulator
    halt

