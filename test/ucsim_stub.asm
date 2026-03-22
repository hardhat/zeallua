; ucsim_stub.asm
; Minimal RST 08h trap for compiler regression tests under ucsim.

    org 0x0000
    jp _boot_up

    defs 0x0008 - $, 0
_syscall:
    ld a, l
    cp 0            ; read(dev=H, buf=DE, size=BC)
    jr z, _sys_read
    cp 15           ; exit(retval in H)
    jr z, _sys_exit
    cp 1            ; write(dev=H, buf=DE, size=BC)
    jr z, _sys_write
    xor a
    ret

_sys_read:
    ld a, h         ; Only provide stdin
    cp 1
    jr nz, _sys_read_empty
    push de
    ld hl, 0
    ld (_read_count), hl
    ld hl, (_input_ptr)

_sys_read_loop:
    ld a, b
    or c
    jr z, _sys_read_done
    ld a, (hl)
    or a
    jr z, _sys_read_done
    ld (de), a
    inc de
    inc hl
    dec bc
    push hl
    ld hl, (_read_count)
    inc hl
    ld (_read_count), hl
    pop hl
    jr _sys_read_loop

_sys_read_done:
    ld (_input_ptr), hl
    ld bc, (_read_count)
    pop de
    xor a
    ret

_sys_read_empty:
    ld bc, 0
    xor a
    ret

_sys_write:
    ld a, h         ; Only capture stdout
    cp 0
    jr nz, _sys_write_done

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

_input_ptr:
    dw _INPUT_BUFFER

_read_count:
    dw 0

_INPUT_BUFFER:
    db "zeal", 10, 0

    defs 0x0200 - $, 0
_OUTPUT_BUFFER:
    defs 256, 0
