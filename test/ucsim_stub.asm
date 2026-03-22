; ucsim_stub.asm
; Minimal RST 08h trap for compiler regression tests under ucsim.

    org 0x0000
    jp _boot_up

    defs 0x0008 - $, 0
_syscall:
    ld a, l
    cp 0            ; read(dev=H, buf=DE, size=BC)
    jp z, _sys_read
    cp 2            ; open(path=BC, flags=H)
    jp z, _sys_open
    cp 3            ; close(dev=H)
    jp z, _sys_close
    cp 15           ; exit(retval in H)
    jp z, _sys_exit
    cp 1            ; write(dev=H, buf=DE, size=BC)
    jp z, _sys_write
    xor a
    ret

_sys_read:
    ld a, h
    cp 1
    jr z, _sys_read_stdin
    cp 2
    jr z, _sys_read_file
    jr _sys_read_empty

_sys_read_stdin:
    push de
    ld hl, 0
    ld (_read_count), hl
    ld hl, (_input_ptr)

_sys_read_stdin_loop:
    ld a, b
    or c
    jr z, _sys_read_stdin_done
    ld a, (hl)
    or a
    jr z, _sys_read_stdin_done
    ld (de), a
    inc de
    inc hl
    dec bc
    push hl
    ld hl, (_read_count)
    inc hl
    ld (_read_count), hl
    pop hl
    jr _sys_read_stdin_loop

_sys_read_stdin_done:
    ld (_input_ptr), hl
    ld bc, (_read_count)
    pop de
    xor a
    ret

_sys_read_file:
    push de
    ld hl, 0
    ld (_read_count), hl
    ld hl, (_file_ptr)

_sys_read_file_loop:
    ld a, b
    or c
    jr z, _sys_read_file_done
    ld a, (hl)
    or a
    jr z, _sys_read_file_done
    ld (de), a
    inc de
    inc hl
    dec bc
    push hl
    ld hl, (_read_count)
    inc hl
    ld (_read_count), hl
    pop hl
    jr _sys_read_file_loop

_sys_read_file_done:
    ld (_file_ptr), hl
    ld bc, (_read_count)
    pop de
    xor a
    ret

_sys_read_empty:
    ld bc, 0
    xor a
    ret

_sys_open:
    push bc
    ld hl, _FILE_NAME
_sys_open_cmp:
    ld a, (bc)
    cp (hl)
    jr nz, _sys_open_fail
    or a
    jr z, _sys_open_ok
    inc bc
    inc hl
    jr _sys_open_cmp

_sys_open_ok:
    pop bc
    ld hl, _FILE_BUFFER
    ld (_file_ptr), hl
    ld a, 2
    ret

_sys_open_fail:
    pop bc
    ld a, 0xFF
    ret

_sys_close:
    xor a
    ret

_sys_write:
    ld a, h         ; Capture stdout or file writes
    cp 0
    jr z, _sys_write_stdout
    cp 2
    jr z, _sys_write_file
    jr _sys_write_done

_sys_write_stdout:
    ld hl, 0
    ld (_read_count), hl

_sys_write_loop:
    ld a, c
    or b
    jr z, _sys_write_done
    ld a, (de)
    call _append_output
    inc de
    dec bc
    ld hl, (_read_count)
    inc hl
    ld (_read_count), hl
    jr _sys_write_loop

_sys_write_file:
    ld hl, 0
    ld (_read_count), hl
    ld hl, (_file_ptr)

_sys_write_file_loop:
    ld a, c
    or b
    jr z, _sys_write_file_done
    ld a, (de)
    ld (hl), a
    inc de
    inc hl
    dec bc
    push hl
    ld hl, (_read_count)
    inc hl
    ld (_read_count), hl
    pop hl
    jr _sys_write_file_loop

_sys_write_file_done:
    xor a
    ld (hl), a
    ld (_file_ptr), hl
    ld bc, (_read_count)
    xor a
    ret

_sys_write_done:
    ld bc, (_read_count)
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
    ld hl, _INPUT_BUFFER
    ld (_input_ptr), hl
    ld hl, _FILE_BUFFER
    ld (_file_ptr), hl
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

_FILE_NAME:
    db "hello.txt", 0

_file_ptr:
    dw _FILE_BUFFER

_FILE_BUFFER:
    db "Z80", 10, 0
