#include "codegen_internal.h"
#include <string.h>
#ifndef __SDCC
#include <stdio.h>
#endif

static void emit_compare_ops(void) {
    z80_add_label(&enc, "op_eq");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_NZ, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");

    z80_add_label(&enc, "op_ne");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_Z, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");

    z80_add_label(&enc, "op_lt");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xAA); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "lt_same_sign");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_Z, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "lt_same_sign");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_NC, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");

    z80_add_label(&enc, "op_le");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xAA); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "le_same_sign");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_Z, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "le_same_sign");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_jp_cc_label(&enc, CC_C, "cmp_done");
    z80_jp_cc_label(&enc, CC_Z, "cmp_done");
    z80_ld_r_n(&enc, REG_L, 0);
    z80_jp_label(&enc, "cmp_done");

    z80_add_label(&enc, "op_gt");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xAA); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "gt_same_sign");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_jr_cc_label(&enc, CC_NZ, "gt_false");
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "gt_same_sign");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_cc_label(&enc, CC_C, "cmp_done");
    z80_jp_cc_label(&enc, CC_Z, "cmp_done");
    z80_inc_r(&enc, REG_L);
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "gt_false");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jp_label(&enc, "cmp_done");

    z80_add_label(&enc, "op_ge");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xAA); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "ge_same_sign");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_jr_cc_label(&enc, CC_NZ, "ge_false");
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "ge_same_sign");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_jp_cc_label(&enc, CC_NC, "cmp_done");
    z80_ld_r_n(&enc, REG_L, 0);
    z80_jp_label(&enc, "cmp_done");
    z80_add_label(&enc, "ge_false");
    z80_ld_rp_nn(&enc, RP_HL, 0);

    z80_add_label(&enc, "cmp_done");
    z80_ld_r_n(&enc, REG_A, 1); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_jump_false");
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, 1);
    z80_jr_cc_label(&enc, CC_Z, "jf_bool");
    z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "jf_take");
    z80_jr_label(&enc, "jf_skip");
    z80_add_label(&enc, "jf_bool");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "jf_take");
    z80_add_label(&enc, "jf_skip");
    z80_ld_hl_mem_label(&enc, "pc_ptr");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_jp_label(&enc, "vm_loop");
    z80_add_label(&enc, "jf_take");
    z80_jp_label(&enc, "op_jump");
}

static void emit_mul_and_value_stack_helpers(void) {
    z80_add_label(&enc, "mul16");
    z80_push(&enc, RP_BC);
    z80_ld_mem_hl_label(&enc, "mul_left");
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "mul_right");
    z80_ld_rp_label(&enc, RP_HL, "mul_sign");
    z80_xor_a(&enc);
    z80_ld_hl_a(&enc);

    z80_ld_hl_mem_label(&enc, "mul_left");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "mul_left_positive");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "mul_left");
    z80_ld_rp_label(&enc, RP_HL, "mul_sign");
    z80_ld_a_hl(&enc);
    z80_emit_b(&enc, 0xEE); z80_emit_b(&enc, 0x01);
    z80_ld_hl_a(&enc);
    z80_add_label(&enc, "mul_left_positive");

    z80_ld_hl_mem_label(&enc, "mul_right");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "mul_right_positive");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "mul_right");
    z80_ld_rp_label(&enc, RP_HL, "mul_sign");
    z80_ld_a_hl(&enc);
    z80_emit_b(&enc, 0xEE); z80_emit_b(&enc, 0x01);
    z80_ld_hl_a(&enc);
    z80_add_label(&enc, "mul_right_positive");

    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_mem_hl_label(&enc, "mul_result");
    z80_add_label(&enc, "mul16_loop_check");
    z80_ld_hl_mem_label(&enc, "mul_right");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "mul16_loop_body");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "mul16_done");
    z80_add_label(&enc, "mul16_loop_body");
    z80_ld_hl_mem_label(&enc, "mul_result");
    z80_ld_de_mem_label(&enc, "mul_left");
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_mem_hl_label(&enc, "mul_result");
    z80_ld_hl_mem_label(&enc, "mul_right");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "mul_right");
    z80_jr_label(&enc, "mul16_loop_check");
    z80_add_label(&enc, "mul16_done");
    z80_ld_rp_label(&enc, RP_HL, "mul_sign");
    z80_ld_a_hl(&enc);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "mul16_positive_return");
    z80_ld_hl_mem_label(&enc, "mul_result");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_jr_label(&enc, "mul16_return");
    z80_add_label(&enc, "mul16_positive_return");
    z80_ld_hl_mem_label(&enc, "mul_result");
    z80_add_label(&enc, "mul16_return");
    z80_pop(&enc, RP_BC); z80_ret(&enc);

    z80_add_label(&enc, "vstack_push");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "vsp_ptr");
    z80_dec_rp(&enc, RP_HL);
    z80_pop(&enc, RP_DE);
    z80_ld_hl_a(&enc);
    z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_E);
    z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_D);
    z80_ld_mem_hl_label(&enc, "vsp_ptr");
    z80_ret(&enc);

    z80_add_label(&enc, "vstack_pop");
    z80_ld_hl_mem_label(&enc, "vsp_ptr");
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_A, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "vsp_ptr");
    z80_ex_de_hl(&enc);
    z80_ret(&enc);
}

static void emit_string_and_call_stack_helpers(void) {
    z80_add_label(&enc, "strcmp_hl_de");
    z80_inc_rp(&enc, RP_HL);             /* skip HL string mark byte [+0] */
    z80_ld_r_r(&enc, REG_C, REG_M);     /* C = len_lo at [+1] */
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_B, REG_M);     /* B = len_hi at [+2] */
    z80_inc_rp(&enc, RP_HL);            /* HL = content at [+3] */
    z80_inc_rp(&enc, RP_DE);            /* skip DE string mark byte [+0] */
    z80_emit_b(&enc, 0x1A);             /* A = DE's len_lo at [+1] */
    z80_cp_a_r(&enc, REG_C);
    z80_jr_cc_label(&enc, CC_NZ, "strcmp_hl_de_done");
    z80_inc_rp(&enc, RP_DE);
    z80_emit_b(&enc, 0x1A);             /* A = DE's len_hi at [+2] */
    z80_cp_a_r(&enc, REG_B);
    z80_jr_cc_label(&enc, CC_NZ, "strcmp_hl_de_done");
    z80_inc_rp(&enc, RP_DE);            /* DE = content at [+3] */
    z80_add_label(&enc, "strcmp_hl_de_loop");
    z80_ld_r_r(&enc, REG_A, REG_B);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "strcmp_hl_de_loop_body");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "strcmp_hl_de_equal");
    z80_add_label(&enc, "strcmp_hl_de_loop_body");
    z80_emit_b(&enc, 0x1A);
    z80_cp_a_r(&enc, REG_M);
    z80_jr_cc_label(&enc, CC_NZ, "strcmp_hl_de_done");
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_DE);
    z80_dec_rp(&enc, RP_BC);
    z80_jr_label(&enc, "strcmp_hl_de_loop");
    z80_add_label(&enc, "strcmp_hl_de_equal");
    z80_xor_a(&enc);
    z80_add_label(&enc, "strcmp_hl_de_done");
    z80_ret(&enc);

    z80_add_label(&enc, "cstack_push_hl");
    z80_push(&enc, RP_DE);
    z80_ex_de_hl(&enc);
    z80_ld_hl_mem_label(&enc, "csp_ptr");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_D);
    z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_E);
    z80_ld_mem_hl_label(&enc, "csp_ptr");
    z80_pop(&enc, RP_DE);
    z80_ret(&enc);

    z80_add_label(&enc, "cstack_pop_hl");
    z80_ld_hl_mem_label(&enc, "csp_ptr");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "csp_ptr");
    z80_ex_de_hl(&enc);
    z80_ret(&enc);
}

static void make_const_string_label(char* dst, uint16_t cap, int16_t func_idx, uint16_t const_idx) {
    if (func_idx < 0) {
        make_indexed_label(dst, cap, "const_str_main_", const_idx);
    } else {
        make_two_index_label(dst, cap, "const_str_func_", (uint16_t)func_idx, const_idx);
    }
}

static bool find_first_string_constant(
    CompiledChunk* chunk,
    int16_t func_idx,
    uint16_t const_idx,
    const char* text,
    int16_t* first_func_idx,
    uint16_t* first_const_idx
) {
    uint16_t i;
    uint16_t f;

    for (i = 0; i < chunk->main.const_count; i++) {
        Constant* c = &chunk->main.constants[i];
        if (func_idx < 0 && i >= const_idx) break;
        if (c->type == CONST_STRING && strcmp(c->data.string, text) == 0) {
            *first_func_idx = -1;
            *first_const_idx = i;
            return true;
        }
    }

    for (f = 0; f < chunk->func_count; f++) {
        BytecodeFunction* func = &chunk->functions[f];
        uint16_t limit = func->const_count;
        if ((int16_t)f > func_idx) break;
        if ((int16_t)f == func_idx) limit = const_idx;
        for (i = 0; i < limit; i++) {
            Constant* c = &func->constants[i];
            if (c->type == CONST_STRING && strcmp(c->data.string, text) == 0) {
                *first_func_idx = (int16_t)f;
                *first_const_idx = i;
                return true;
            }
        }
    }

    return false;
}

static void emit_function_constant_pool_interned(
    CompiledChunk* chunk,
    const char* pool_label,
    int16_t func_idx,
    BytecodeFunction* func
) {
    char label[32];
    uint16_t i;

    z80_add_label(&enc, pool_label);
    for (i = 0; i < func->const_count; i++) {
        Constant* c = &func->constants[i];
        if (c->type == CONST_NUMBER) {
            z80_emit_b(&enc, TYPE_NUMBER);
            z80_emit_w(&enc, (uint16_t)c->data.number);
        } else if (c->type == CONST_STRING) {
            int16_t first_func_idx;
            uint16_t first_const_idx;

            z80_emit_b(&enc, TYPE_STRING);
            if (find_first_string_constant(chunk, func_idx, i, c->data.string, &first_func_idx, &first_const_idx)) {
                make_const_string_label(label, sizeof(label), first_func_idx, first_const_idx);
            } else {
                make_const_string_label(label, sizeof(label), func_idx, i);
            }
            z80_add_ref(&enc, label, false, true);
        } else if (c->type == CONST_FUNCTION) {
            z80_emit_b(&enc, TYPE_FUNCTION);
            make_indexed_label(label, sizeof(label), "func_record_", c->data.func_idx);
            z80_add_ref(&enc, label, false, true);
        } else {
            z80_emit_b(&enc, TYPE_NIL);
            z80_emit_w(&enc, 0);
        }
    }
}

static void emit_interned_string_objects(CompiledChunk* chunk) {
    char label[32];
    uint16_t i;
    uint16_t f;

    for (i = 0; i < chunk->main.const_count; i++) {
        Constant* c = &chunk->main.constants[i];
        int16_t first_func_idx;
        uint16_t first_const_idx;
        if (c->type != CONST_STRING) continue;
        if (find_first_string_constant(chunk, -1, i, c->data.string, &first_func_idx, &first_const_idx)) continue;
        make_const_string_label(label, sizeof(label), -1, i);
        emit_string_object(label, c->data.string);
    }

    for (f = 0; f < chunk->func_count; f++) {
        BytecodeFunction* func = &chunk->functions[f];
        for (i = 0; i < func->const_count; i++) {
            Constant* c = &func->constants[i];
            int16_t first_func_idx;
            uint16_t first_const_idx;
            if (c->type != CONST_STRING) continue;
            if (find_first_string_constant(chunk, (int16_t)f, i, c->data.string, &first_func_idx, &first_const_idx)) continue;
            make_const_string_label(label, sizeof(label), (int16_t)f, i);
            emit_string_object(label, c->data.string);
        }
    }
}

static uint16_t count_total_string_constant_refs(CompiledChunk* chunk) {
    uint16_t total = 0;
    uint16_t i;
    uint16_t f;

    for (i = 0; i < chunk->main.const_count; i++) {
        if (chunk->main.constants[i].type == CONST_STRING) total++;
    }

    for (f = 0; f < chunk->func_count; f++) {
        BytecodeFunction* func = &chunk->functions[f];
        for (i = 0; i < func->const_count; i++) {
            if (func->constants[i].type == CONST_STRING) total++;
        }
    }

    return total;
}

static uint16_t count_unique_string_constants(CompiledChunk* chunk) {
    uint16_t unique = 0;
    uint16_t i;
    uint16_t f;

    for (i = 0; i < chunk->main.const_count; i++) {
        Constant* c = &chunk->main.constants[i];
        int16_t first_func_idx;
        uint16_t first_const_idx;
        if (c->type != CONST_STRING) continue;
        if (!find_first_string_constant(chunk, -1, i, c->data.string, &first_func_idx, &first_const_idx)) unique++;
    }

    for (f = 0; f < chunk->func_count; f++) {
        BytecodeFunction* func = &chunk->functions[f];
        for (i = 0; i < func->const_count; i++) {
            Constant* c = &func->constants[i];
            int16_t first_func_idx;
            uint16_t first_const_idx;
            if (c->type != CONST_STRING) continue;
            if (!find_first_string_constant(chunk, (int16_t)f, i, c->data.string, &first_func_idx, &first_const_idx)) unique++;
        }
    }

    return unique;
}

static uint16_t estimate_deduped_string_bytes(CompiledChunk* chunk) {
    uint16_t saved = 0;
    uint16_t i;
    uint16_t f;

    for (i = 0; i < chunk->main.const_count; i++) {
        Constant* c = &chunk->main.constants[i];
        int16_t first_func_idx;
        uint16_t first_const_idx;
        if (c->type != CONST_STRING) continue;
        if (find_first_string_constant(chunk, -1, i, c->data.string, &first_func_idx, &first_const_idx)) {
            saved = (uint16_t)(saved + (uint16_t)strlen(c->data.string) + STRING_HEADER_BYTES + 1);
        }
    }

    for (f = 0; f < chunk->func_count; f++) {
        BytecodeFunction* func = &chunk->functions[f];
        for (i = 0; i < func->const_count; i++) {
            Constant* c = &func->constants[i];
            int16_t first_func_idx;
            uint16_t first_const_idx;
            if (c->type != CONST_STRING) continue;
            if (find_first_string_constant(chunk, (int16_t)f, i, c->data.string, &first_func_idx, &first_const_idx)) {
                saved = (uint16_t)(saved + (uint16_t)strlen(c->data.string) + STRING_HEADER_BYTES + 1);
            }
        }
    }

    return saved;
}

static void emit_constant_pools_and_runtime_data(CompiledChunk* chunk) {
    uint16_t i;

    emit_function_constant_pool_interned(chunk, "const_pool_main", -1, &chunk->main);
    for (i = 0; i < chunk->func_count; i++) {
        char pool_label[32];

        make_indexed_label(pool_label, sizeof(pool_label), "const_pool_func_", i);
        emit_function_constant_pool_interned(chunk, pool_label, (int16_t)i, &chunk->functions[i]);
    }

    emit_interned_string_objects(chunk);

#ifndef __SDCC
    if (codegen_is_verbose()) {
        uint16_t total = count_total_string_constant_refs(chunk);
        uint16_t unique = count_unique_string_constants(chunk);
        uint16_t dedup = (total >= unique) ? (uint16_t)(total - unique) : 0;
        uint16_t bytes_saved = estimate_deduped_string_bytes(chunk);
        fprintf(stderr, "[codegen] const-string interning: total=%u unique=%u deduped=%u bytes_saved=%u\n", total, unique, dedup, bytes_saved);
    }
#endif

    for (i = 0; i < chunk->func_count; i++) {
        char label[32];
        char code_label[32];
        char pool_label[32];

        make_indexed_label(label, sizeof(label), "func_record_", i);
        make_indexed_label(code_label, sizeof(code_label), "bytecode_func_", i);
        make_indexed_label(pool_label, sizeof(pool_label), "const_pool_func_", i);
        z80_add_label(&enc, label);
        z80_add_ref(&enc, code_label, false, true);
        z80_add_ref(&enc, pool_label, false, true);
        z80_emit_b(&enc, chunk->functions[i].upvalue_count);
        z80_emit_b(&enc, chunk->functions[i].env_local_count);
        for (uint8_t j = 0; j < chunk->functions[i].upvalue_count; j++) {
            z80_emit_b(&enc, chunk->functions[i].upvalues[j].from_local ? 0 : 1);
            z80_emit_b(&enc, chunk->functions[i].upvalues[j].index);
        }
    }

    emit_string_object("str_empty", "");
    emit_string_object("str_newline", "\n");
    emit_string_object("str_type_nil", "nil");
    emit_string_object("str_type_boolean", "boolean");
    emit_string_object("str_type_number", "number");
    emit_string_object("str_type_string", "string");
    emit_string_object("str_type_table", "table");
    emit_string_object("str_type_function", "function");
    emit_string_object("str_nil_value", "nil");
    emit_string_object("str_false_value", "false");
    emit_string_object("str_true_value", "true");
    emit_string_object("str_table_value", "table");
    emit_string_object("str_function_value", "function");

    z80_add_label(&enc, "bss_start");
    z80_add_label(&enc, "global_vars");
    for (i = 0; i < 768; i++) z80_emit_b(&enc, 0);

    z80_add_label(&enc, "call_frame_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "call_func_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "closure_candidate"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "closure_source_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "closure_dest_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "return_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "return_type"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "current_env_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "current_closure_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "closure_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "len_table_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "len_target"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_source"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_length"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_candidate"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "concat_left_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "concat_right_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "concat_left_len"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "concat_right_len"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "mul_left"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "mul_right"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "mul_result"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "mul_sign"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "pow_base"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "pow_exp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "pow_result"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "tonumber_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "tonumber_remaining"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "tonumber_result"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "tonumber_negative"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "readfile_dev"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "writefile_dev"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "writefile_data_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "open_flags"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "io_data_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "tostring_negative"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "heap_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "alloc_fail_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "alloc_table_fail_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "alloc_string_fail_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "alloc_closure_fail_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "alloc_total_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "free_table_list"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_reclaim_candidate"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "pending_table_reclaim_head"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "pending_table_reclaim_tail"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "gc_sweep_enabled"); z80_emit_b(&enc, 1);
    z80_add_label(&enc, "pending_table_reclaim_dropcnt"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_mark_table_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_sweep_table_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_mark_string_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_sweep_string_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_freelist_corrupt_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_freelist_debug_enabled"); z80_emit_b(&enc, 1);
    z80_add_label(&enc, "gc_trigger_soft_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_trigger_force_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_soft_hysteresis_latched"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "gc_force_hysteresis_latched"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "gc_cycle_count"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_scan_next"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_alloc_prev"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_alloc_cur"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "string_alloc_next"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "cstack_scan_closure_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "cstack_scan_env_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "cstack_frame_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "pending_table_reclaim_buf"); for (i = 0; i < (TABLE_RECLAIM_QUEUE_CAP * 2); i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "free_string_small_list"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "free_string_medium_list"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "free_string_large_list"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "free_closure_list"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_low_watermark"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "gc_high_watermark"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_key_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_key_type"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "table_val_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_val_type"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "table_entry_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "heap_space"); for (i = 0; i < TABLE_HEAP_BYTES; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "heap_end");
    z80_add_label(&enc, "string_space"); for (i = 0; i < STRING_HEAP_BYTES; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "string_end");
    z80_add_label(&enc, "closure_space"); for (i = 0; i < CLOSURE_HEAP_BYTES; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "closure_end");
    z80_add_label(&enc, "callstack_space"); for (i = 0; i < CALL_STACK_BYTES; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "callstack_end");
    z80_add_label(&enc, "vstack_space"); for (i = 0; i < VSTACK_BYTES; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "vstack_end");
    z80_add_label(&enc, "input_buffer"); for (i = 0; i < 64; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "num_buffer"); for (i = 0; i < 16; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "num_buffer_end"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "tmp_len"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "bss_end");
}

void emit_compare_stack_and_data(CompiledChunk* chunk) {
    emit_compare_ops();
    emit_mul_and_value_stack_helpers();
    emit_string_and_call_stack_helpers();
    emit_constant_pools_and_runtime_data(chunk);
}