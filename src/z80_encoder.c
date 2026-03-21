#define Z80_ENCODER_NO_COMPAT_MACROS
#include "z80_encoder.h"
#include <string.h>

Z80Encoder enc;

void z80_init(uint8_t* buffer, uint16_t capacity, uint16_t base_addr) {
    enc.buffer = buffer;
    enc.capacity = capacity;
    enc.size = 0;
    enc.base_addr = base_addr;
    enc.label_count = 0;
    enc.ref_count = 0;
}

void z80_add_label(const char* name) {
    if (enc.label_count < MAX_LABELS) {
        strncpy(enc.labels[enc.label_count].name, name, 31);
        enc.labels[enc.label_count].name[31] = '\0';
        enc.labels[enc.label_count].addr = enc.base_addr + enc.size;
        enc.label_count++;
    }
}

void z80_add_ref(const char* name, bool is_relative, bool is_word) {
    if (enc.ref_count < MAX_REFS) {
        strncpy(enc.refs[enc.ref_count].name, name, 31);
        enc.refs[enc.ref_count].name[31] = '\0';
        enc.refs[enc.ref_count].patch_addr = enc.size;
        enc.refs[enc.ref_count].is_relative = is_relative;
        enc.refs[enc.ref_count].is_word = is_word;
        enc.ref_count++;
        if (is_word) z80_emit_w(0);
        else z80_emit_b(0);
    }
}

void z80_resolve_refs(void) {
    for (uint16_t i = 0; i < enc.ref_count; i++) {
        uint16_t target = 0;
        bool found = false;
        for (uint16_t j = 0; j < enc.label_count; j++) {
            if (strcmp(enc.refs[i].name, enc.labels[j].name) == 0) {
                target = enc.labels[j].addr;
                found = true;
                break;
            }
        }
        if (found) {
            uint16_t patch = enc.refs[i].patch_addr;
            if (enc.refs[i].is_relative) {
                int8_t rel = (int8_t)((int16_t)target - (int16_t)(enc.base_addr + patch + 1));
                enc.buffer[patch] = (uint8_t)rel;
            } else if (enc.refs[i].is_word) {
                enc.buffer[patch] = target & 0xFF;
                enc.buffer[patch+1] = (target >> 8) & 0xFF;
            }
        }
    }
}

void z80_emit_b(uint8_t b) {
    if (enc.size < enc.capacity) enc.buffer[enc.size++] = b;
}

void z80_emit_w(uint16_t w) {
    z80_emit_b(w & 0xFF);
    z80_emit_b((w >> 8) & 0xFF);
}

void z80_nop(void) { z80_emit_b(0x00); }
void z80_halt(void) { z80_emit_b(0x76); }
void z80_ret(void) { z80_emit_b(0xC9); }

void z80_ld_rp_nn(RegPair rp, uint16_t nn) {
    z80_emit_b(0x01 | (rp << 4));
    z80_emit_w(nn);
}

void z80_ld_r_n(Register r, uint8_t n) {
    z80_emit_b(0x06 | (r << 3));
    z80_emit_b(n);
}

void z80_ld_r_r(Register dst, Register src) {
    z80_emit_b(0x40 | (dst << 3) | src);
}

void z80_ld_a_mem(uint16_t addr) {
    z80_emit_b(0x3A);
    z80_emit_w(addr);
}

void z80_ld_mem_a(uint16_t addr) {
    z80_emit_b(0x32);
    z80_emit_w(addr);
}

void z80_ld_hl_mem(uint16_t addr) {
    z80_emit_b(0x2A);
    z80_emit_w(addr);
}

void z80_ld_mem_hl(uint16_t addr) {
    z80_emit_b(0x22);
    z80_emit_w(addr);
}

void z80_ld_de_mem(uint16_t addr) {
    z80_emit_b(0xED);
    z80_emit_b(0x5B);
    z80_emit_w(addr);
}

void z80_ld_a_hl(void) { z80_emit_b(0x7E); }
void z80_ld_hl_a(void) { z80_emit_b(0x77); }

void z80_push(RegPair rp) { z80_emit_b(0xC5 | (rp << 4)); }
void z80_pop(RegPair rp) { z80_emit_b(0xC1 | (rp << 4)); }

void z80_inc_r(Register r) { z80_emit_b(0x04 | (r << 3)); }
void z80_dec_r(Register r) { z80_emit_b(0x05 | (r << 3)); }
void z80_inc_rp(RegPair rp) { z80_emit_b(0x03 | (rp << 4)); }
void z80_dec_rp(RegPair rp) { z80_emit_b(0x0B | (rp << 4)); }

void z80_add_hl_rp(RegPair rp) { z80_emit_b(0x09 | (rp << 4)); }
void z80_sbc_hl_rp(RegPair rp) {
    z80_emit_b(0xED);
    z80_emit_b(0x42 | (rp << 4));
}

void z80_add_a_r(Register r) { z80_emit_b(0x80 | r); }
void z80_adc_a_r(Register r) { z80_emit_b(0x88 | r); }
void z80_add_a_n(uint8_t n) { z80_emit_b(0xC6); z80_emit_b(n); }
void z80_sub_a_r(Register r) { z80_emit_b(0x90 | r); }
void z80_sub_a_n(uint8_t n) { z80_emit_b(0xD6); z80_emit_b(n); }
void z80_cp_a_r(Register r) { z80_emit_b(0xB8 | r); }
void z80_cp_a_n(uint8_t n) { z80_emit_b(0xFE); z80_emit_b(n); }

void z80_xor_a(void) { z80_emit_b(0xAF); }
void z80_or_a(void) { z80_emit_b(0xB7); }

void z80_jp(uint16_t addr) {
    z80_emit_b(0xC3);
    z80_emit_w(addr);
}

void z80_jp_cc(Condition cc, uint16_t addr) {
    z80_emit_b(0xC2 | (cc << 3));
    z80_emit_w(addr);
}

void z80_jr(int8_t offset) {
    z80_emit_b(0x18);
    z80_emit_b((uint8_t)offset);
}

void z80_jr_cc(Condition cc, int8_t offset) {
    z80_emit_b(0x20 | (cc << 3));
    z80_emit_b((uint8_t)offset);
}

void z80_call(uint16_t addr) {
    z80_emit_b(0xCD);
    z80_emit_w(addr);
}

void z80_rst(uint8_t vec) {
    z80_emit_b(0xC7 | (vec & 0x38));
}

void z80_ex_de_hl(void) { z80_emit_b(0xEB); }

void z80_djnz(int8_t offset) {
    z80_emit_b(0x10);
    z80_emit_b((uint8_t)offset);
}

void z80_ldir(void) {
    z80_emit_b(0xED);
    z80_emit_b(0xB0);
}

// Label variations
void z80_jp_label(const char* label) {
    z80_emit_b(0xC3);
    z80_add_ref(label, false, true);
}

void z80_jp_cc_label(Condition cc, const char* label) {
    z80_emit_b(0xC2 | (cc << 3));
    z80_add_ref(label, false, true);
}

void z80_jr_label(const char* label) {
    z80_emit_b(0x18);
    z80_add_ref(label, true, false);
}

void z80_jr_cc_label(Condition cc, const char* label) {
    z80_emit_b(0x20 | (cc << 3));
    z80_add_ref(label, true, false);
}

void z80_call_label(const char* label) {
    z80_emit_b(0xCD);
    z80_add_ref(label, false, true);
}

void z80_ld_rp_label(RegPair rp, const char* label) {
    z80_emit_b(0x01 | (rp << 4));
    z80_add_ref(label, false, true);
}

void z80_ld_mem_hl_label(const char* label) {
    z80_emit_b(0x22);
    z80_add_ref(label, false, true);
}

void z80_ld_hl_mem_label(const char* label) {
    z80_emit_b(0x2A);
    z80_add_ref(label, false, true);
}

void z80_ld_de_mem_label(const char* label) {
    z80_emit_b(0xED);
    z80_emit_b(0x5B);
    z80_add_ref(label, false, true);
}

void z80_djnz_label(const char* label) {
    z80_emit_b(0x10);
    z80_add_ref(label, true, false);
}
