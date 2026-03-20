#ifndef Z80_ENCODER_H
#define Z80_ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_LABELS 512
#define MAX_REFS 1024

typedef enum {
    REG_B = 0, REG_C = 1, REG_D = 2, REG_E = 3, REG_H = 4, REG_L = 5, REG_M = 6, REG_A = 7
} Register;

typedef enum {
    RP_BC = 0, RP_DE = 1, RP_HL = 2, RP_SP = 3, RP_AF = 3
} RegPair;

typedef enum {
    CC_NZ = 0, CC_Z = 1, CC_NC = 2, CC_C = 3
} Condition;

// Labels & Refs
typedef struct {
    char name[32];
    uint16_t addr;
} Z80Label;

typedef struct {
    char name[32];
    uint16_t patch_addr;
    bool is_relative;
    bool is_word;
} Z80Ref;

typedef struct {
    uint8_t* buffer;
    uint16_t capacity;
    uint16_t size;
    uint16_t base_addr;
    
    Z80Label labels[MAX_LABELS];
    uint16_t label_count;
    Z80Ref refs[MAX_REFS];
    uint16_t ref_count;
} Z80Encoder;

void z80_init(Z80Encoder* e, uint8_t* buffer, uint16_t capacity, uint16_t base_addr);

// Label management
void z80_add_label(Z80Encoder* e, const char* name);
void z80_add_ref(Z80Encoder* e, const char* name, bool is_relative, bool is_word);
void z80_resolve_refs(Z80Encoder* e);

// Basic emission
void z80_emit_b(Z80Encoder* e, uint8_t b);
void z80_emit_w(Z80Encoder* e, uint16_t w);

// Instructions
void z80_nop(Z80Encoder* e);
void z80_halt(Z80Encoder* e);
void z80_ret(Z80Encoder* e);
void z80_ld_rp_nn(Z80Encoder* e, RegPair rp, uint16_t nn);
void z80_ld_r_n(Z80Encoder* e, Register r, uint8_t n);
void z80_ld_r_r(Z80Encoder* e, Register dst, Register src);
void z80_ld_a_mem(Z80Encoder* e, uint16_t addr);
void z80_ld_mem_a(Z80Encoder* e, uint16_t addr);
void z80_ld_hl_mem(Z80Encoder* e, uint16_t addr);
void z80_ld_mem_hl(Z80Encoder* e, uint16_t addr);
void z80_ld_de_mem(Z80Encoder* e, uint16_t addr);
void z80_ld_a_hl(Z80Encoder* e);
void z80_ld_hl_a(Z80Encoder* e);
void z80_push(Z80Encoder* e, RegPair rp);
void z80_pop(Z80Encoder* e, RegPair rp);
void z80_inc_r(Z80Encoder* e, Register r);
void z80_dec_r(Z80Encoder* e, Register r);
void z80_inc_rp(Z80Encoder* e, RegPair rp);
void z80_dec_rp(Z80Encoder* e, RegPair rp);
void z80_add_hl_rp(Z80Encoder* e, RegPair rp);
void z80_sbc_hl_rp(Z80Encoder* e, RegPair rp);
void z80_add_a_r(Z80Encoder* e, Register r);
void z80_adc_a_r(Z80Encoder* e, Register r);
void z80_add_a_n(Z80Encoder* e, uint8_t n);
void z80_sub_a_r(Z80Encoder* e, Register r);
void z80_sub_a_n(Z80Encoder* e, uint8_t n);
void z80_cp_a_r(Z80Encoder* e, Register r);
void z80_cp_a_n(Z80Encoder* e, uint8_t n);
void z80_xor_a(Z80Encoder* e);
void z80_or_a(Z80Encoder* e);
void z80_jp(Z80Encoder* e, uint16_t addr);
void z80_jp_cc(Z80Encoder* e, Condition cc, uint16_t addr);
void z80_jr(Z80Encoder* e, int8_t offset);
void z80_jr_cc(Z80Encoder* e, Condition cc, int8_t offset);
void z80_call(Z80Encoder* e, uint16_t addr);
void z80_rst(Z80Encoder* e, uint8_t vec);
void z80_ex_de_hl(Z80Encoder* e);
void z80_djnz(Z80Encoder* e, int8_t offset);
void z80_ldir(Z80Encoder* e);

// Label variations (requested for cleaner syntax)
void z80_jp_label(Z80Encoder* e, const char* label);
void z80_jp_cc_label(Z80Encoder* e, Condition cc, const char* label);
void z80_jr_label(Z80Encoder* e, const char* label);
void z80_jr_cc_label(Z80Encoder* e, Condition cc, const char* label);
void z80_call_label(Z80Encoder* e, const char* label);
void z80_ld_rp_label(Z80Encoder* e, RegPair rp, const char* label);
void z80_ld_mem_hl_label(Z80Encoder* e, const char* label);
void z80_ld_hl_mem_label(Z80Encoder* e, const char* label);
void z80_ld_de_mem_label(Z80Encoder* e, const char* label);
void z80_djnz_label(Z80Encoder* e, const char* label);

#endif
