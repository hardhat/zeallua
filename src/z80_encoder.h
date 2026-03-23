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

extern Z80Encoder enc;

void z80_init(uint8_t* buffer, uint16_t capacity, uint16_t base_addr);

// Label management
void z80_add_label(const char* name);
void z80_add_ref(const char* name, bool is_relative, bool is_word);
bool z80_resolve_refs(void);

// Basic emission
void z80_emit_b(uint8_t b);
void z80_emit_w(uint16_t w);

// Instructions
void z80_nop(void);
void z80_halt(void);
void z80_ret(void);
void z80_ld_rp_nn(RegPair rp, uint16_t nn);
void z80_ld_r_n(Register r, uint8_t n);
void z80_ld_r_r(Register dst, Register src);
void z80_ld_a_mem(uint16_t addr);
void z80_ld_mem_a(uint16_t addr);
void z80_ld_hl_mem(uint16_t addr);
void z80_ld_mem_hl(uint16_t addr);
void z80_ld_de_mem(uint16_t addr);
void z80_ld_a_hl(void);
void z80_ld_hl_a(void);
void z80_push(RegPair rp);
void z80_pop(RegPair rp);
void z80_inc_r(Register r);
void z80_dec_r(Register r);
void z80_inc_rp(RegPair rp);
void z80_dec_rp(RegPair rp);
void z80_add_hl_rp(RegPair rp);
void z80_sbc_hl_rp(RegPair rp);
void z80_add_a_r(Register r);
void z80_adc_a_r(Register r);
void z80_add_a_n(uint8_t n);
void z80_sub_a_r(Register r);
void z80_sub_a_n(uint8_t n);
void z80_cp_a_r(Register r);
void z80_cp_a_n(uint8_t n);
void z80_xor_a(void);
void z80_or_a(void);
void z80_jp(uint16_t addr);
void z80_jp_cc(Condition cc, uint16_t addr);
void z80_jr(int8_t offset);
void z80_jr_cc(Condition cc, int8_t offset);
void z80_call(uint16_t addr);
void z80_rst(uint8_t vec);
void z80_ex_de_hl(void);
void z80_djnz(int8_t offset);
void z80_ldir(void);

// Label variations (requested for cleaner syntax)
void z80_jp_label(const char* label);
void z80_jp_cc_label(Condition cc, const char* label);
void z80_jr_label(const char* label);
void z80_jr_cc_label(Condition cc, const char* label);
void z80_call_label(const char* label);
void z80_ld_rp_label(RegPair rp, const char* label);
void z80_ld_mem_hl_label(const char* label);
void z80_ld_hl_mem_label(const char* label);
void z80_ld_de_mem_label(const char* label);
void z80_djnz_label(const char* label);

#ifndef Z80_ENCODER_NO_COMPAT_MACROS
#define Z80_LAST1_SELECT(_1, _2, NAME, ...) NAME
#define Z80_LAST1(...) Z80_LAST1_SELECT(__VA_ARGS__, Z80_LAST1_2, Z80_LAST1_1)(__VA_ARGS__)
#define Z80_LAST1_1(a) a
#define Z80_LAST1_2(_ignored, a) a

#define Z80_LAST2_SELECT(_1, _2, _3, NAME, ...) NAME
#define Z80_LAST2(...) Z80_LAST2_SELECT(__VA_ARGS__, Z80_LAST2_3, Z80_LAST2_2)(__VA_ARGS__)
#define Z80_LAST2_2(a, b) a, b
#define Z80_LAST2_3(_ignored, a, b) a, b

#define Z80_LAST3_SELECT(_1, _2, _3, _4, NAME, ...) NAME
#define Z80_LAST3(...) Z80_LAST3_SELECT(__VA_ARGS__, Z80_LAST3_4, Z80_LAST3_3)(__VA_ARGS__)
#define Z80_LAST3_3(a, b, c) a, b, c
#define Z80_LAST3_4(_ignored, a, b, c) a, b, c

#define z80_init(...) z80_init(Z80_LAST3(__VA_ARGS__))
#define z80_add_label(...) z80_add_label(Z80_LAST1(__VA_ARGS__))
#define z80_add_ref(...) z80_add_ref(Z80_LAST3(__VA_ARGS__))
#define z80_resolve_refs(...) z80_resolve_refs()
#define z80_emit_b(...) z80_emit_b(Z80_LAST1(__VA_ARGS__))
#define z80_emit_w(...) z80_emit_w(Z80_LAST1(__VA_ARGS__))
#define z80_nop(...) z80_nop()
#define z80_halt(...) z80_halt()
#define z80_ret(...) z80_ret()
#define z80_ld_rp_nn(...) z80_ld_rp_nn(Z80_LAST2(__VA_ARGS__))
#define z80_ld_r_n(...) z80_ld_r_n(Z80_LAST2(__VA_ARGS__))
#define z80_ld_r_r(...) z80_ld_r_r(Z80_LAST2(__VA_ARGS__))
#define z80_ld_a_mem(...) z80_ld_a_mem(Z80_LAST1(__VA_ARGS__))
#define z80_ld_mem_a(...) z80_ld_mem_a(Z80_LAST1(__VA_ARGS__))
#define z80_ld_hl_mem(...) z80_ld_hl_mem(Z80_LAST1(__VA_ARGS__))
#define z80_ld_mem_hl(...) z80_ld_mem_hl(Z80_LAST1(__VA_ARGS__))
#define z80_ld_de_mem(...) z80_ld_de_mem(Z80_LAST1(__VA_ARGS__))
#define z80_ld_a_hl(...) z80_ld_a_hl()
#define z80_ld_hl_a(...) z80_ld_hl_a()
#define z80_push(...) z80_push(Z80_LAST1(__VA_ARGS__))
#define z80_pop(...) z80_pop(Z80_LAST1(__VA_ARGS__))
#define z80_inc_r(...) z80_inc_r(Z80_LAST1(__VA_ARGS__))
#define z80_dec_r(...) z80_dec_r(Z80_LAST1(__VA_ARGS__))
#define z80_inc_rp(...) z80_inc_rp(Z80_LAST1(__VA_ARGS__))
#define z80_dec_rp(...) z80_dec_rp(Z80_LAST1(__VA_ARGS__))
#define z80_add_hl_rp(...) z80_add_hl_rp(Z80_LAST1(__VA_ARGS__))
#define z80_sbc_hl_rp(...) z80_sbc_hl_rp(Z80_LAST1(__VA_ARGS__))
#define z80_add_a_r(...) z80_add_a_r(Z80_LAST1(__VA_ARGS__))
#define z80_adc_a_r(...) z80_adc_a_r(Z80_LAST1(__VA_ARGS__))
#define z80_add_a_n(...) z80_add_a_n(Z80_LAST1(__VA_ARGS__))
#define z80_sub_a_r(...) z80_sub_a_r(Z80_LAST1(__VA_ARGS__))
#define z80_sub_a_n(...) z80_sub_a_n(Z80_LAST1(__VA_ARGS__))
#define z80_cp_a_r(...) z80_cp_a_r(Z80_LAST1(__VA_ARGS__))
#define z80_cp_a_n(...) z80_cp_a_n(Z80_LAST1(__VA_ARGS__))
#define z80_xor_a(...) z80_xor_a()
#define z80_or_a(...) z80_or_a()
#define z80_jp(...) z80_jp(Z80_LAST1(__VA_ARGS__))
#define z80_jp_cc(...) z80_jp_cc(Z80_LAST2(__VA_ARGS__))
#define z80_jr(...) z80_jr(Z80_LAST1(__VA_ARGS__))
#define z80_jr_cc(...) z80_jr_cc(Z80_LAST2(__VA_ARGS__))
#define z80_call(...) z80_call(Z80_LAST1(__VA_ARGS__))
#define z80_rst(...) z80_rst(Z80_LAST1(__VA_ARGS__))
#define z80_ex_de_hl(...) z80_ex_de_hl()
#define z80_djnz(...) z80_djnz(Z80_LAST1(__VA_ARGS__))
#define z80_ldir(...) z80_ldir()
#define z80_jp_label(...) z80_jp_label(Z80_LAST1(__VA_ARGS__))
#define z80_jp_cc_label(...) z80_jp_cc_label(Z80_LAST2(__VA_ARGS__))
#define z80_jr_label(...) z80_jr_label(Z80_LAST1(__VA_ARGS__))
#define z80_jr_cc_label(...) z80_jr_cc_label(Z80_LAST2(__VA_ARGS__))
#define z80_call_label(...) z80_call_label(Z80_LAST1(__VA_ARGS__))
#define z80_ld_rp_label(...) z80_ld_rp_label(Z80_LAST2(__VA_ARGS__))
#define z80_ld_mem_hl_label(...) z80_ld_mem_hl_label(Z80_LAST1(__VA_ARGS__))
#define z80_ld_hl_mem_label(...) z80_ld_hl_mem_label(Z80_LAST1(__VA_ARGS__))
#define z80_ld_de_mem_label(...) z80_ld_de_mem_label(Z80_LAST1(__VA_ARGS__))
#define z80_djnz_label(...) z80_djnz_label(Z80_LAST1(__VA_ARGS__))
#endif

#endif
