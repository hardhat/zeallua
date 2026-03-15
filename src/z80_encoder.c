#include "z80_encoder.h"

void z80_init(Z80Encoder* e, uint8_t* buffer, uint16_t capacity, uint16_t base_addr) {
    e->buffer = buffer;
    e->capacity = capacity;
    e->size = 0;
    e->base_addr = base_addr;
}

void z80_emit_b(Z80Encoder* e, uint8_t b) {
    if (e->size < e->capacity) e->buffer[e->size++] = b;
}

void z80_emit_w(Z80Encoder* e, uint16_t w) {
    z80_emit_b(e, w & 0xFF);
    z80_emit_b(e, (w >> 8) & 0xFF);
}

void z80_nop(Z80Encoder* e) { z80_emit_b(e, 0x00); }
void z80_halt(Z80Encoder* e) { z80_emit_b(e, 0x76); }
void z80_ret(Z80Encoder* e) { z80_emit_b(e, 0xC9); }

void z80_ld_rp_nn(Z80Encoder* e, RegPair rp, uint16_t nn) {
    z80_emit_b(e, 0x01 | (rp << 4));
    z80_emit_w(e, nn);
}

void z80_ld_r_n(Z80Encoder* e, Register r, uint8_t n) {
    z80_emit_b(e, 0x06 | (r << 3));
    z80_emit_b(e, n);
}

void z80_ld_r_r(Z80Encoder* e, Register dst, Register src) {
    z80_emit_b(e, 0x40 | (dst << 3) | src);
}

void z80_ld_a_mem(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0x3A);
    z80_emit_w(e, addr);
}

void z80_ld_mem_a(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0x32);
    z80_emit_w(e, addr);
}

void z80_ld_hl_mem(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0x2A);
    z80_emit_w(e, addr);
}

void z80_ld_mem_hl(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0x22);
    z80_emit_w(e, addr);
}

void z80_ld_de_mem(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0xED);
    z80_emit_b(e, 0x5B);
    z80_emit_w(e, addr);
}

void z80_ld_a_hl(Z80Encoder* e) { z80_emit_b(e, 0x7E); }
void z80_ld_hl_a(Z80Encoder* e) { z80_emit_b(e, 0x77); }

void z80_push(Z80Encoder* e, RegPair rp) { z80_emit_b(e, 0xC5 | (rp << 4)); }
void z80_pop(Z80Encoder* e, RegPair rp) { z80_emit_b(e, 0xC1 | (rp << 4)); }

void z80_inc_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0x04 | (r << 3)); }
void z80_dec_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0x05 | (r << 3)); }
void z80_inc_rp(Z80Encoder* e, RegPair rp) { z80_emit_b(e, 0x03 | (rp << 4)); }
void z80_dec_rp(Z80Encoder* e, RegPair rp) { z80_emit_b(e, 0x0B | (rp << 4)); }

void z80_add_hl_rp(Z80Encoder* e, RegPair rp) { z80_emit_b(e, 0x09 | (rp << 4)); }

void z80_add_a_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0x80 | r); }
void z80_adc_a_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0x88 | r); }
void z80_add_a_n(Z80Encoder* e, uint8_t n) { z80_emit_b(e, 0xC6); z80_emit_b(e, n); }
void z80_sub_a_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0x90 | r); }
void z80_sub_a_n(Z80Encoder* e, uint8_t n) { z80_emit_b(e, 0xD6); z80_emit_b(e, n); }
void z80_cp_a_r(Z80Encoder* e, Register r) { z80_emit_b(e, 0xB8 | r); }
void z80_cp_a_n(Z80Encoder* e, uint8_t n) { z80_emit_b(e, 0xFE); z80_emit_b(e, n); }

void z80_xor_a(Z80Encoder* e) { z80_emit_b(e, 0xAF); }
void z80_or_a(Z80Encoder* e) { z80_emit_b(e, 0xB7); }

void z80_jp(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0xC3);
    z80_emit_w(e, addr);
}

void z80_jp_cc(Z80Encoder* e, Condition cc, uint16_t addr) {
    z80_emit_b(e, 0xC2 | (cc << 3));
    z80_emit_w(e, addr);
}

void z80_jr(Z80Encoder* e, int8_t offset) {
    z80_emit_b(e, 0x18);
    z80_emit_b(e, (uint8_t)offset);
}

void z80_jr_cc(Z80Encoder* e, Condition cc, int8_t offset) {
    z80_emit_b(e, 0x20 | (cc << 3));
    z80_emit_b(e, (uint8_t)offset);
}

void z80_call(Z80Encoder* e, uint16_t addr) {
    z80_emit_b(e, 0xCD);
    z80_emit_w(e, addr);
}

void z80_rst(Z80Encoder* e, uint8_t vec) {
    z80_emit_b(e, 0xC7 | (vec & 0x38));
}

void z80_ex_de_hl(Z80Encoder* e) { z80_emit_b(e, 0xEB); }

void z80_djnz(Z80Encoder* e, int8_t offset) {
    z80_emit_b(e, 0x10);
    z80_emit_b(e, (uint8_t)offset);
}
