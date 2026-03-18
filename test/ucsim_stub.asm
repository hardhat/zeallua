; ucsim_stub.asm
; Minimal RST 08h trap for compiler regression tests under ucsim.

    org 0x0000
    jp _boot_up

    defs 0x0008 - $, 0
_syscall:
    cp 0            ; exit
    jr z, _sys_exit
    cp 2            ; write(dev=B, buf=DE, size_ptr=HL)
    jr z, _sys_write
    xor a
    ret

_sys_write:
    ld a, b         ; Only capture stdout
    cp 0
    jr nz, _sys_write_done

    ld c, (hl)
    inc hl
    ld b, (hl)

_sys_write_loop:
    ld a, c
    or b
    jr z, _sys_write_done
    ld a, (de)
    call _append_output
    inc de
    dec bc
    jr _sys_write_loop

_sys_write_done:
    xor a
    ret

_append_output:
    push hl
    ld hl, (_output_ptr)
    ld (hl), a
    inc hl
    ld (_output_ptr), hl
    xor a
    ld (hl), a
    pop hl
    ret

_sys_exit:
    xor a
    halt

_boot_up:
    ld sp, 0xFFFE
    ld hl, _OUTPUT_BUFFER
    ld (_output_ptr), hl
    xor a
    ld (_OUTPUT_BUFFER), a
    jp 0x4000

_output_ptr:
    dw _OUTPUT_BUFFER

    defs 0x0200 - $, 0
_OUTPUT_BUFFER:
    defs 256, 0
