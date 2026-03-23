#include "codegen_internal.h"
#ifdef __SDCC
#include "zos_vfs.h"
#include "zos_sys.h"
#else
#include "zos_host_stub.h"
#endif
#include <string.h>

#define MAX_IMAGE_SIZE 49152
static uint8_t image[MAX_IMAGE_SIZE];
static bool codegen_verbose = false;

static bool find_label_addr(const Z80Encoder* e, const char* name, uint16_t* addr_out) {
    uint16_t i;

    for (i = 0; i < e->label_count; i++) {
        if (strcmp(e->labels[i].name, name) == 0) {
            *addr_out = e->labels[i].addr;
            return true;
        }
    }

    return false;
}

static bool write_all(zos_dev_t dev, const void* buf, uint16_t size) {
    const uint8_t* ptr = (const uint8_t*)buf;

    while (size > 0) {
        uint16_t chunk = size;
        zos_err_t err = write(dev, ptr, &chunk);
        if (err != ERR_SUCCESS || chunk == 0) {
            return false;
        }

        ptr += chunk;
        size -= chunk;
    }

    return true;
}

static void append_char(char* dst, uint16_t* len, uint16_t cap, char ch) {
    if (*len + 1 < cap) {
        dst[*len] = ch;
        (*len)++;
        dst[*len] = '\0';
    }
}

static void append_str(char* dst, uint16_t* len, uint16_t cap, const char* src) {
    while (*src != '\0') {
        append_char(dst, len, cap, *src);
        src++;
    }
}

static void append_hex16(char* dst, uint16_t* len, uint16_t cap, uint16_t value) {
    static const char hex[] = "0123456789ABCDEF";
    append_char(dst, len, cap, hex[(value >> 12) & 0x0F]);
    append_char(dst, len, cap, hex[(value >> 8) & 0x0F]);
    append_char(dst, len, cap, hex[(value >> 4) & 0x0F]);
    append_char(dst, len, cap, hex[value & 0x0F]);
}

static void append_uint16(char* dst, uint16_t* len, uint16_t cap, uint16_t value) {
    char digits[5];
    uint16_t count = 0;

    if (value == 0) {
        append_char(dst, len, cap, '0');
        return;
    }

    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (count > 0) {
        count--;
        append_char(dst, len, cap, digits[count]);
    }
}

void make_indexed_label(char* dst, uint16_t cap, const char* prefix, uint16_t index) {
    uint16_t len = 0;
    dst[0] = '\0';
    append_str(dst, &len, cap, prefix);
    append_uint16(dst, &len, cap, index);
}

void make_two_index_label(char* dst, uint16_t cap, const char* prefix, uint16_t first, uint16_t second) {
    uint16_t len = 0;
    dst[0] = '\0';
    append_str(dst, &len, cap, prefix);
    append_uint16(dst, &len, cap, first);
    append_char(dst, &len, cap, '_');
    append_uint16(dst, &len, cap, second);
}

void emit_string_object(const char* label, const char* text) {
    uint16_t len = (uint16_t)strlen(text);

    z80_add_label(label);
    z80_emit_b(0xFF);  /* mark byte: 0xFF = permanent (constant pool, never freed by GC) */
    z80_emit_w(len);
    while (*text) {
        z80_emit_b((uint8_t)*text);
        text++;
    }
    z80_emit_b(0);
}

void emit_function_constant_pool(const char* pool_label, const char* string_prefix, BytecodeFunction* func) {
    char label[32];
    uint16_t i;

    z80_add_label(&enc, pool_label);
    for (i = 0; i < func->const_count; i++) {
        Constant* c = &func->constants[i];
        if (c->type == CONST_NUMBER) {
            z80_emit_b(&enc, TYPE_NUMBER);
            z80_emit_w(&enc, (uint16_t)c->data.number);
        } else if (c->type == CONST_STRING) {
            z80_emit_b(&enc, TYPE_STRING);
            make_indexed_label(label, sizeof(label), string_prefix, i);
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

    for (i = 0; i < func->const_count; i++) {
        Constant* c = &func->constants[i];
        if (c->type == CONST_STRING) {
            make_indexed_label(label, sizeof(label), string_prefix, i);
            emit_string_object(label, c->data.string);
        }
    }
}

void codegen_init(void) {
}

void codegen_set_verbose(bool verbose) {
    codegen_verbose = verbose;
}

bool codegen_is_verbose(void) {
    return codegen_verbose;
}

static bool export_symbols(Z80Encoder* e, const char* bin_filename) {
    char sym_filename[256];
    char line[80];
    char* dot;
    zos_dev_t sym;
    uint16_t i;

    strncpy(sym_filename, bin_filename, sizeof(sym_filename) - 1);
    sym_filename[sizeof(sym_filename) - 1] = '\0';

    dot = strrchr(sym_filename, '.');
    if (dot) *dot = '\0';
    strcat(sym_filename, ".sym");

    sym = open(sym_filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (sym < 0) return false;

    for (i = 0; i < e->label_count; i++) {
        uint16_t line_len = 0;
        uint16_t name_len = 0;

        line[0] = '\0';
        append_str(line, &line_len, sizeof(line), e->labels[i].name);

        while (e->labels[i].name[name_len] != '\0') {
            name_len++;
        }

        while (name_len < 24) {
            append_char(line, &line_len, sizeof(line), ' ');
            name_len++;
        }

        append_str(line, &line_len, sizeof(line), " EQU 0x");
        append_hex16(line, &line_len, sizeof(line), e->labels[i].addr);
        append_char(line, &line_len, sizeof(line), '\n');

        if (!write_all(sym, line, line_len)) {
            close(sym);
            return false;
        }
    }

    close(sym);
    return true;
}

static void emit_entry_and_dispatch(CompiledChunk* chunk) {
    uint16_t i;
    char label[32];

    z80_add_label(&enc, "_start");
    z80_ld_rp_label(&enc, RP_HL, "bss_end");
    z80_ld_rp_label(&enc, RP_DE, "bss_start");
    z80_or_a(&enc);
    z80_sbc_hl_rp(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "clear_bss_non_empty");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "clear_bss_done");
    z80_add_label(&enc, "clear_bss_non_empty");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_B, REG_H);
    z80_ld_r_r(&enc, REG_C, REG_L);
    z80_ld_rp_label(&enc, RP_HL, "bss_start");
    z80_ld_rp_label(&enc, RP_DE, "bss_start");
    z80_inc_rp(&enc, RP_DE);
    z80_xor_a(&enc);
    z80_ld_hl_a(&enc);
    z80_ld_r_r(&enc, REG_A, REG_B);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "clear_bss_copy");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "clear_bss_done");
    z80_add_label(&enc, "clear_bss_copy");
    z80_ldir(&enc);
    z80_add_label(&enc, "clear_bss_done");

    z80_ld_rp_label(&enc, RP_HL, "heap_space");
    z80_ld_mem_hl_label(&enc, "heap_ptr");
    z80_ld_rp_label(&enc, RP_HL, "string_space");
    z80_ld_mem_hl_label(&enc, "string_ptr");
    z80_ld_rp_label(&enc, RP_HL, "closure_space");
    z80_ld_mem_hl_label(&enc, "closure_ptr");
    if (chunk->main.env_local_count > 0) {
        z80_ld_rp_nn(&enc, RP_HL, (uint16_t)(chunk->main.env_local_count * 3));
        z80_call_label(&enc, "alloc_runtime_space");
        z80_ld_mem_hl_label(&enc, "current_env_ptr");
    }
    z80_ld_rp_label(&enc, RP_HL, "vstack_end");
    z80_ld_mem_hl_label(&enc, "vsp_ptr");
    z80_ld_mem_hl_label(&enc, "fp_ptr");
    z80_ld_rp_label(&enc, RP_HL, "const_pool_main");
    z80_ld_mem_hl_label(&enc, "cp_ptr");
    z80_ld_rp_label(&enc, RP_HL, "callstack_end");
    z80_ld_mem_hl_label(&enc, "csp_ptr");
    z80_ld_rp_label(&enc, RP_HL, "bytecode_main");
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "pc_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "vsp_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "fp_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "cp_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "csp_ptr"); z80_emit_w(&enc, 0);

    z80_add_label(&enc, "bytecode_main");
    for (i = 0; i < chunk->main.code_len; i++) {
        z80_emit_b(&enc, chunk->main.code[i]);
    }
    for (i = 0; i < chunk->func_count; i++) {
        make_indexed_label(label, sizeof(label), "bytecode_func_", i);
        z80_add_label(&enc, label);
        for (uint16_t j = 0; j < chunk->functions[i].code_len; j++) {
            z80_emit_b(&enc, chunk->functions[i].code[j]);
        }
    }

    z80_add_label(&enc, "vm_loop");
    z80_ld_hl_mem_label(&enc, "pc_ptr");
    z80_ld_a_hl(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    
    // Simple dispatcher
    z80_cp_a_n(&enc, 0xFF); z80_jp_cc_label(&enc, CC_Z, "vm_halt");
    z80_cp_a_n(&enc, 0x01); z80_jp_cc_label(&enc, CC_Z, "op_pop");
    z80_cp_a_n(&enc, 0x02); z80_jp_cc_label(&enc, CC_Z, "op_dup");
    z80_cp_a_n(&enc, 0x03); z80_jp_cc_label(&enc, CC_Z, "op_rot3");
    z80_cp_a_n(&enc, 0x10); z80_jp_cc_label(&enc, CC_Z, "op_loadnil");
    z80_cp_a_n(&enc, 0x11); z80_jp_cc_label(&enc, CC_Z, "op_loadtrue");
    z80_cp_a_n(&enc, 0x12); z80_jp_cc_label(&enc, CC_Z, "op_loadfalse");
    z80_cp_a_n(&enc, 0x13); z80_jp_cc_label(&enc, CC_Z, "op_loadconst");
    z80_cp_a_n(&enc, 0x20); z80_jp_cc_label(&enc, CC_Z, "op_getlocal");
    z80_cp_a_n(&enc, 0x21); z80_jp_cc_label(&enc, CC_Z, "op_setlocal");
    z80_cp_a_n(&enc, 0x22); z80_jp_cc_label(&enc, CC_Z, "op_getglobal");
    z80_cp_a_n(&enc, 0x23); z80_jp_cc_label(&enc, CC_Z, "op_setglobal");
    z80_cp_a_n(&enc, 0x24); z80_jp_cc_label(&enc, CC_Z, "op_getupval");
    z80_cp_a_n(&enc, 0x25); z80_jp_cc_label(&enc, CC_Z, "op_setupval");
    z80_cp_a_n(&enc, 0x26); z80_jp_cc_label(&enc, CC_Z, "op_getcaptured");
    z80_cp_a_n(&enc, 0x27); z80_jp_cc_label(&enc, CC_Z, "op_setcaptured");
    z80_cp_a_n(&enc, 0x30); z80_jp_cc_label(&enc, CC_Z, "op_newtable");
    z80_cp_a_n(&enc, 0x31); z80_jp_cc_label(&enc, CC_Z, "op_gettable");
    z80_cp_a_n(&enc, 0x32); z80_jp_cc_label(&enc, CC_Z, "op_settable");
    z80_cp_a_n(&enc, 0x40); z80_jp_cc_label(&enc, CC_Z, "op_add");
    z80_cp_a_n(&enc, 0x41); z80_jp_cc_label(&enc, CC_Z, "op_sub");
    z80_cp_a_n(&enc, 0x42); z80_jp_cc_label(&enc, CC_Z, "op_mul");
    z80_cp_a_n(&enc, 0x43); z80_jp_cc_label(&enc, CC_Z, "op_div");
    z80_cp_a_n(&enc, 0x44); z80_jp_cc_label(&enc, CC_Z, "op_mod");
    z80_cp_a_n(&enc, 0x45); z80_jp_cc_label(&enc, CC_Z, "op_pow");
    z80_cp_a_n(&enc, 0x46); z80_jp_cc_label(&enc, CC_Z, "op_neg");
    z80_cp_a_n(&enc, 0x50); z80_jp_cc_label(&enc, CC_Z, "op_eq");
    z80_cp_a_n(&enc, 0x51); z80_jp_cc_label(&enc, CC_Z, "op_ne");
    z80_cp_a_n(&enc, 0x52); z80_jp_cc_label(&enc, CC_Z, "op_lt");
    z80_cp_a_n(&enc, 0x53); z80_jp_cc_label(&enc, CC_Z, "op_le");
    z80_cp_a_n(&enc, 0x54); z80_jp_cc_label(&enc, CC_Z, "op_gt");
    z80_cp_a_n(&enc, 0x55); z80_jp_cc_label(&enc, CC_Z, "op_ge");
    z80_cp_a_n(&enc, 0x60); z80_jp_cc_label(&enc, CC_Z, "op_not");
    z80_cp_a_n(&enc, 0x70); z80_jp_cc_label(&enc, CC_Z, "op_concat");
    z80_cp_a_n(&enc, 0x71); z80_jp_cc_label(&enc, CC_Z, "op_len");
    z80_cp_a_n(&enc, 0x80); z80_jp_cc_label(&enc, CC_Z, "op_jump");
    z80_cp_a_n(&enc, 0x81); z80_jp_cc_label(&enc, CC_Z, "op_jump_false");
    z80_cp_a_n(&enc, 0x90); z80_jp_cc_label(&enc, CC_Z, "op_call");
    z80_cp_a_n(&enc, 0x91); z80_jp_cc_label(&enc, CC_Z, "op_return");
    z80_cp_a_n(&enc, 0x92); z80_jp_cc_label(&enc, CC_Z, "op_closure");
    z80_cp_a_n(&enc, 0xA0); z80_jp_cc_label(&enc, CC_Z, "op_print");
    z80_cp_a_n(&enc, 0xA1); z80_jp_cc_label(&enc, CC_Z, "op_type");
    z80_cp_a_n(&enc, 0xA2); z80_jp_cc_label(&enc, CC_Z, "op_tonumber");
    z80_cp_a_n(&enc, 0xA3); z80_jp_cc_label(&enc, CC_Z, "op_tostring");
    z80_cp_a_n(&enc, 0xA4); z80_jp_cc_label(&enc, CC_Z, "op_input");
    z80_cp_a_n(&enc, 0xA5); z80_jp_cc_label(&enc, CC_Z, "op_readfile");
    z80_cp_a_n(&enc, 0xA6); z80_jp_cc_label(&enc, CC_Z, "op_writefile");
    z80_cp_a_n(&enc, 0xA7); z80_jp_cc_label(&enc, CC_Z, "op_open");
    z80_cp_a_n(&enc, 0xA8); z80_jp_cc_label(&enc, CC_Z, "op_read");
    z80_cp_a_n(&enc, 0xA9); z80_jp_cc_label(&enc, CC_Z, "op_write");
    z80_cp_a_n(&enc, 0xAA); z80_jp_cc_label(&enc, CC_Z, "op_close");
    z80_jp_label(&enc, "vm_loop");
}

static void emit_scope_ops(void) {
    z80_add_label(&enc, "op_getlocal");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_de_mem_label(&enc, "fp_ptr"); z80_ex_de_hl(&enc); z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_dec_rp(&enc, RP_HL); z80_dec_rp(&enc, RP_HL); z80_dec_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_E, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_a_hl(&enc); z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_setlocal");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_C, REG_A); z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_AF);
    z80_ld_r_r(&enc, REG_L, REG_C); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_de_mem_label(&enc, "fp_ptr"); z80_ex_de_hl(&enc); z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_dec_rp(&enc, RP_HL); z80_dec_rp(&enc, RP_HL); z80_dec_rp(&enc, RP_HL);
    z80_pop(&enc, RP_AF); z80_pop(&enc, RP_DE); z80_ld_r_r(&enc, REG_M, REG_D); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_M, REG_E); z80_inc_rp(&enc, RP_HL); z80_ld_hl_a(&enc);
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_getupval");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_call_label(&enc, "get_upvalue_cell_ptr");
    z80_ld_r_r(&enc, REG_D, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_E, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_a_hl(&enc); z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_setupval");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_C, REG_A); z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_AF);
    z80_ld_r_r(&enc, REG_A, REG_C); z80_call_label(&enc, "get_upvalue_cell_ptr");
    z80_pop(&enc, RP_AF); z80_pop(&enc, RP_DE); z80_ld_r_r(&enc, REG_M, REG_D); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_M, REG_E); z80_inc_rp(&enc, RP_HL); z80_ld_hl_a(&enc);
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_getcaptured");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_call_label(&enc, "get_captured_cell_ptr");
    z80_ld_r_r(&enc, REG_D, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_E, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_a_hl(&enc); z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_setcaptured");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_C, REG_A); z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_AF);
    z80_ld_r_r(&enc, REG_A, REG_C); z80_call_label(&enc, "get_captured_cell_ptr");
    z80_pop(&enc, RP_AF); z80_pop(&enc, RP_DE); z80_ld_r_r(&enc, REG_M, REG_D); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_M, REG_E); z80_inc_rp(&enc, RP_HL); z80_ld_hl_a(&enc);
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_getglobal");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_rp_label(&enc, RP_DE, "global_vars"); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_E, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_D, REG_M); z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_setglobal");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_C, REG_A); z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_AF);
    z80_ld_r_r(&enc, REG_L, REG_C); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_rp_label(&enc, RP_DE, "global_vars"); z80_add_hl_rp(&enc, RP_DE);
    z80_pop(&enc, RP_AF); z80_ld_hl_a(&enc); z80_inc_rp(&enc, RP_HL); z80_pop(&enc, RP_DE); z80_ld_r_r(&enc, REG_M, REG_E); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_M, REG_D);
    z80_jp_label(&enc, "vm_loop");
}

static bool write_image_file(const char* out_filename) {
    zos_dev_t bin;
    uint16_t bss_start_addr;
    uint16_t written;
    zos_err_t err;

    bin = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (bin < 0) return false;

    if (!find_label_addr(&enc, "bss_start", &bss_start_addr)) {
        close(bin);
        return false;
    }

    written = bss_start_addr - enc.base_addr;
    err = write(bin, image, &written);
    close(bin);

    return err == ERR_SUCCESS && written == (uint16_t)(bss_start_addr - enc.base_addr);
}

bool codegen_generate(CompiledChunk* chunk, const char* out_filename) {
    z80_init(image, MAX_IMAGE_SIZE, 0x4000);
    memset(image, 0, MAX_IMAGE_SIZE);

    emit_entry_and_dispatch(chunk);
    emit_io_and_arithmetic_ops_split();
    emit_scope_ops();
    emit_compare_stack_and_data(chunk);

    if (!z80_resolve_refs()) {
        return false;
    }

    if (!export_symbols(&enc, out_filename)) {
        return false;
    }

    return write_image_file(out_filename);
}
