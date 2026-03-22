#include "codegen_internal.h"
#ifdef __SDCC
#include "zos_vfs.h"
#include "zos_sys.h"
#else
#include "zos_host_stub.h"
#endif
#include <string.h>

#define MAX_IMAGE_SIZE 16384
static uint8_t image[MAX_IMAGE_SIZE];

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
    z80_jp_label(&enc, "vm_loop");
}

#if 0
static void emit_io_and_arithmetic_ops(void) {
    z80_add_label(&enc, "vm_halt");
    z80_ld_r_n(&enc, REG_A, 0); // system call 0: exit
    z80_xor_a(&enc); z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_rst(&enc, 0x08);

    z80_add_label(&enc, "op_pop"); z80_call_label(&enc, "vstack_pop"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_dup");
    z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_AF); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_push");
    z80_pop(&enc, RP_HL); z80_pop(&enc, RP_AF);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_loadnil");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_r_n(&enc, REG_A, 0);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_loadtrue");
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_ld_r_n(&enc, REG_A, 1);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_loadfalse");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_r_n(&enc, REG_A, 1);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_loadconst");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_de_mem_label(&enc, "cp_ptr"); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_E, REG_M); z80_inc_rp(&enc, RP_HL); z80_ld_r_r(&enc, REG_D, REG_M); z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_rot3");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_key_temp");
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_hl_a(&enc);
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_entry_temp");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_val_temp");
    z80_ld_rp_label(&enc, RP_HL, "table_val_type");
    z80_ld_hl_a(&enc);
    z80_ld_hl_mem_label(&enc, "table_entry_temp");
    z80_ld_r_n(&enc, REG_A, TYPE_TABLE);
    z80_call_label(&enc, "vstack_push");
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_a_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_call_label(&enc, "vstack_push");
    z80_ld_hl_mem_label(&enc, "table_val_temp");
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_HL, "table_val_type");
    z80_ld_a_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_print");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_B, REG_A); // B = number of args
    z80_add_label(&enc, "op_print_loop");
    z80_ld_r_r(&enc, REG_A, REG_B); z80_or_a(&enc); z80_jp_cc_label(&enc, CC_Z, "vm_loop");
    z80_push(&enc, RP_BC);
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, TYPE_NUMBER);
    z80_jp_cc_label(&enc, CC_Z, "op_print_number");
    z80_call_label(&enc, "coerce_to_string");
    z80_call_label(&enc, "print_str");
    z80_jp_label(&enc, "op_print_next");
    z80_add_label(&enc, "op_print_number");
    z80_call_label(&enc, "print_num");
    z80_add_label(&enc, "op_print_next");
    z80_ld_rp_label(&enc, RP_HL, "str_newline"); z80_call_label(&enc, "print_str");
    z80_pop(&enc, RP_BC); z80_djnz_label(&enc, "op_print_loop");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_type");
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, TYPE_NIL); z80_jr_cc_label(&enc, CC_Z, "type_nil");
    z80_cp_a_n(&enc, TYPE_BOOL); z80_jr_cc_label(&enc, CC_Z, "type_boolean");
    z80_cp_a_n(&enc, TYPE_NUMBER); z80_jr_cc_label(&enc, CC_Z, "type_number");
    z80_cp_a_n(&enc, TYPE_STRING); z80_jr_cc_label(&enc, CC_Z, "type_string");
    z80_cp_a_n(&enc, TYPE_TABLE); z80_jr_cc_label(&enc, CC_Z, "type_table");
    z80_ld_rp_label(&enc, RP_HL, "str_type_function");
    z80_jr_label(&enc, "type_push");
    z80_add_label(&enc, "type_nil");
    z80_ld_rp_label(&enc, RP_HL, "str_type_nil");
    z80_jr_label(&enc, "type_push");
    z80_add_label(&enc, "type_boolean");
    z80_ld_rp_label(&enc, RP_HL, "str_type_boolean");
    z80_jr_label(&enc, "type_push");
    z80_add_label(&enc, "type_number");
    z80_ld_rp_label(&enc, RP_HL, "str_type_number");
    z80_jr_label(&enc, "type_push");
    z80_add_label(&enc, "type_string");
    z80_ld_rp_label(&enc, RP_HL, "str_type_string");
    z80_jr_label(&enc, "type_push");
    z80_add_label(&enc, "type_table");
    z80_ld_rp_label(&enc, RP_HL, "str_type_table");
    z80_add_label(&enc, "type_push");
    z80_ld_r_n(&enc, REG_A, TYPE_STRING);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_tonumber");
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, TYPE_NUMBER); z80_jr_cc_label(&enc, CC_Z, "tonumber_push");
    z80_cp_a_n(&enc, TYPE_STRING); z80_jr_cc_label(&enc, CC_Z, "tonumber_parse");
    z80_jr_label(&enc, "tonumber_nil");
    z80_add_label(&enc, "tonumber_parse");
    z80_call_label(&enc, "string_to_number");
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "tonumber_nil");
    z80_add_label(&enc, "tonumber_push");
    z80_ld_r_n(&enc, REG_A, TYPE_NUMBER);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");
    z80_add_label(&enc, "tonumber_nil");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_r_n(&enc, REG_A, TYPE_NIL);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_tostring");
    z80_call_label(&enc, "vstack_pop");
    z80_call_label(&enc, "coerce_to_string");
    z80_ld_r_n(&enc, REG_A, TYPE_STRING);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_concat");
    z80_call_label(&enc, "vstack_pop");
    z80_call_label(&enc, "coerce_to_string");
    z80_ld_mem_hl_label(&enc, "concat_right_ptr");
    z80_call_label(&enc, "vstack_pop");
    z80_call_label(&enc, "coerce_to_string");
    z80_ld_de_mem_label(&enc, "concat_right_ptr");
    z80_call_label(&enc, "alloc_concat_string");
    z80_ld_r_n(&enc, REG_A, TYPE_STRING);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "alloc_runtime_space"); // HL = byte count, returns HL = allocation or 0
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "alloc_runtime_space_nonzero");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "alloc_runtime_space_fail");
    z80_add_label(&enc, "alloc_runtime_space_nonzero");
    z80_ld_de_mem_label(&enc, "closure_ptr");
    z80_ex_de_hl(&enc);
    z80_push(&enc, RP_HL);
    z80_add_hl_rp(&enc, RP_DE);
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_DE, "closure_end");
    z80_or_a(&enc);
    z80_sbc_hl_rp(&enc, RP_DE);
    z80_jr_cc_label(&enc, CC_C, "alloc_runtime_space_ok");
    z80_jr_cc_label(&enc, CC_Z, "alloc_runtime_space_ok");
    z80_pop(&enc, RP_HL);
    z80_pop(&enc, RP_HL);
    z80_add_label(&enc, "alloc_runtime_space_fail");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ret(&enc);
    z80_add_label(&enc, "alloc_runtime_space_ok");
    z80_pop(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "closure_ptr");
    z80_pop(&enc, RP_HL);
    z80_ret(&enc);

    z80_add_label(&enc, "get_captured_cell_ptr"); // A = env slot index, returns HL = cell pointer
    z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_n(&enc, REG_H, 0);
    z80_ld_r_r(&enc, REG_E, REG_L);
    z80_ld_r_r(&enc, REG_D, REG_H);
    z80_add_hl_rp(&enc, RP_HL);
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_de_mem_label(&enc, "current_env_ptr");
    z80_add_hl_rp(&enc, RP_DE);
    z80_ret(&enc);

    z80_add_label(&enc, "get_upvalue_cell_ptr"); // A = upvalue index, returns HL = cell pointer
    z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_n(&enc, REG_H, 0);
    z80_add_hl_rp(&enc, RP_HL);
    z80_ld_de_mem_label(&enc, "current_closure_ptr");
    z80_add_hl_rp(&enc, RP_DE);
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_ret(&enc);

    z80_add_label(&enc, "instantiate_closure"); // HL = prototype record, returns HL = closure/prototype
    z80_ld_mem_hl_label(&enc, "closure_source_ptr");
    z80_push(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_a_hl(&enc);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "instantiate_closure_alloc");
    z80_pop(&enc, RP_HL);
    z80_ret(&enc);
    z80_add_label(&enc, "instantiate_closure_alloc");
    z80_ld_r_r(&enc, REG_C, REG_A);
    z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_n(&enc, REG_H, 0);
    z80_add_hl_rp(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, FUNCTION_HEADER_BYTES);
    z80_add_hl_rp(&enc, RP_DE);
    z80_call_label(&enc, "alloc_runtime_space");
    z80_ld_mem_hl_label(&enc, "closure_candidate");
    z80_ld_de_mem_label(&enc, "closure_candidate");
    z80_pop(&enc, RP_HL);
    z80_ld_r_n(&enc, REG_B, FUNCTION_HEADER_BYTES);
    z80_add_label(&enc, "closure_header_copy");
    z80_ld_a_hl(&enc);
    z80_emit_b(&enc, 0x12);
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_DE);
    z80_djnz_label(&enc, "closure_header_copy");
    z80_ld_r_r(&enc, REG_B, REG_C);
    z80_add_label(&enc, "closure_capture_loop");
    z80_ld_r_r(&enc, REG_A, REG_B);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "closure_capture_done");
    z80_ld_a_hl(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_C, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_push(&enc, RP_HL);
    z80_push(&enc, RP_DE);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "closure_capture_parent");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_call_label(&enc, "get_captured_cell_ptr");
    z80_jr_label(&enc, "closure_capture_store");
    z80_add_label(&enc, "closure_capture_parent");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_call_label(&enc, "get_upvalue_cell_ptr");
    z80_add_label(&enc, "closure_capture_store");
    z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_emit_b(&enc, 0x12);
    z80_inc_rp(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0x12);
    z80_inc_rp(&enc, RP_DE);
    z80_pop(&enc, RP_HL);
    z80_djnz_label(&enc, "closure_capture_loop");
    z80_add_label(&enc, "closure_capture_done");
    z80_ld_hl_mem_label(&enc, "closure_candidate");
    z80_ret(&enc);

    z80_add_label(&enc, "op_closure");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_n(&enc, REG_H, 0); z80_ld_r_r(&enc, REG_E, REG_L); z80_ld_r_r(&enc, REG_D, REG_H); z80_add_hl_rp(&enc, RP_HL); z80_add_hl_rp(&enc, RP_DE);
    z80_ld_de_mem_label(&enc, "cp_ptr"); z80_add_hl_rp(&enc, RP_DE);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_call_label(&enc, "instantiate_closure");
    z80_ld_r_n(&enc, REG_A, TYPE_FUNCTION);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_call");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_r_r(&enc, REG_B, REG_A);
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_call_label(&enc, "cstack_push_hl");
    z80_ld_hl_mem_label(&enc, "fp_ptr"); z80_call_label(&enc, "cstack_push_hl");
    z80_ld_hl_mem_label(&enc, "cp_ptr"); z80_call_label(&enc, "cstack_push_hl");
    z80_ld_hl_mem_label(&enc, "current_env_ptr"); z80_call_label(&enc, "cstack_push_hl");
    z80_ld_hl_mem_label(&enc, "current_closure_ptr"); z80_call_label(&enc, "cstack_push_hl");
    z80_ld_hl_mem_label(&enc, "vsp_ptr");
    z80_add_label(&enc, "call_frame_seek");
    z80_ld_r_r(&enc, REG_A, REG_B); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "call_frame_found");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_djnz_label(&enc, "call_frame_seek");
    z80_add_label(&enc, "call_frame_found");
    z80_ld_mem_hl_label(&enc, "call_frame_temp");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "fp_ptr");
    z80_ld_hl_mem_label(&enc, "call_frame_temp");
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "call_func_temp");
    z80_ld_hl_mem_label(&enc, "call_func_temp");
    z80_ld_mem_hl_label(&enc, "current_closure_ptr");
    z80_ld_hl_mem_label(&enc, "call_func_temp");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_hl_mem_label(&enc, "call_func_temp");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "cp_ptr");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_mem_hl_label(&enc, "current_env_ptr");
    z80_ld_hl_mem_label(&enc, "call_func_temp");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_a_hl(&enc);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "call_env_ready");
    z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_n(&enc, REG_H, 0);
    z80_ld_r_r(&enc, REG_E, REG_L);
    z80_ld_r_r(&enc, REG_D, REG_H);
    z80_add_hl_rp(&enc, RP_HL);
    z80_add_hl_rp(&enc, RP_DE);
    z80_call_label(&enc, "alloc_runtime_space");
    z80_ld_mem_hl_label(&enc, "current_env_ptr");
    z80_add_label(&enc, "call_env_ready");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_return");
    z80_ld_hl_mem_label(&enc, "pc_ptr"); z80_inc_rp(&enc, RP_HL); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "return_temp");
    z80_ld_rp_label(&enc, RP_HL, "return_type");
    z80_ld_hl_a(&enc);
    z80_ld_hl_mem_label(&enc, "fp_ptr");
    z80_ld_mem_hl_label(&enc, "vsp_ptr");
    z80_call_label(&enc, "cstack_pop_hl"); z80_ld_mem_hl_label(&enc, "current_closure_ptr");
    z80_call_label(&enc, "cstack_pop_hl"); z80_ld_mem_hl_label(&enc, "current_env_ptr");
    z80_call_label(&enc, "cstack_pop_hl"); z80_ld_mem_hl_label(&enc, "cp_ptr");
    z80_call_label(&enc, "cstack_pop_hl"); z80_ld_mem_hl_label(&enc, "fp_ptr");
    z80_call_label(&enc, "cstack_pop_hl"); z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_ld_hl_mem_label(&enc, "return_temp");
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_HL, "return_type");
    z80_ld_a_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "print_num");
    z80_ld_rp_label(&enc, RP_DE, "num_buffer_end");
    z80_ld_rp_nn(&enc, RP_BC, 10);
    z80_add_label(&enc, "print_num_loop");
    z80_call_label(&enc, "div16_8");
    z80_add_a_n(&enc, '0'); z80_dec_rp(&enc, RP_DE);
    z80_emit_b(&enc, 0x12); // ld (de), a
    z80_ld_r_r(&enc, REG_A, REG_H); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "print_num_loop");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "print_num_loop");
    z80_ex_de_hl(&enc); z80_call_label(&enc, "print_zstr");
    z80_ret(&enc);

    z80_add_label(&enc, "print_zstr"); // HL = null-terminated string
    z80_push(&enc, RP_HL); z80_ld_r_n(&enc, REG_B, 0);
    z80_add_label(&enc, "ps_len"); z80_ld_a_hl(&enc); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "ps_out");
    z80_inc_rp(&enc, RP_HL); z80_inc_r(&enc, REG_B); z80_jr_label(&enc, "ps_len");
    z80_add_label(&enc, "ps_out"); z80_pop(&enc, RP_DE); // DE = start
    z80_ld_r_r(&enc, REG_C, REG_B); z80_ld_r_n(&enc, REG_B, 0); z80_push(&enc, RP_BC); z80_pop(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tmp_len");
    z80_ld_rp_label(&enc, RP_HL, "tmp_len");
    z80_ld_r_n(&enc, REG_B, 0); // stdout
    z80_ld_r_n(&enc, REG_A, 2); // write
    z80_rst(&enc, 0x08); z80_ret(&enc);

    z80_add_label(&enc, "print_str"); // HL = length-prefixed string object
    z80_ld_r_r(&enc, REG_C, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_B, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ex_de_hl(&enc);
    z80_push(&enc, RP_BC); z80_pop(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tmp_len");
    z80_ld_rp_label(&enc, RP_HL, "tmp_len");
    z80_ld_r_n(&enc, REG_B, 0); // stdout
    z80_ld_r_n(&enc, REG_A, 2); // write
    z80_rst(&enc, 0x08); z80_ret(&enc);

    z80_add_label(&enc, "div16_8"); // HL = HL / 10, returns rem in A
    z80_xor_a(&enc); z80_ld_r_n(&enc, REG_B, 16);
    z80_add_label(&enc, "div_l"); z80_add_hl_rp(&enc, RP_HL); z80_emit_b(&enc, 0x17); // rla
    z80_cp_a_n(&enc, 10); z80_jr_cc_label(&enc, CC_NC, "div_s");
    z80_sub_a_n(&enc, 10); z80_inc_r(&enc, REG_L);
    z80_add_label(&enc, "div_s"); z80_djnz_label(&enc, "div_l"); z80_ret(&enc);

    z80_add_label(&enc, "divmod10_u16");
    z80_ld_rp_nn(&enc, RP_DE, 0);
    z80_add_label(&enc, "divmod10_u16_loop");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "divmod10_u16_subtract");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_cp_a_n(&enc, 10);
    z80_jr_cc_label(&enc, CC_C, "divmod10_u16_done");
    z80_add_label(&enc, "divmod10_u16_subtract");
    z80_push(&enc, RP_DE);
    z80_ld_rp_nn(&enc, RP_DE, 10);
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_pop(&enc, RP_DE);
    z80_inc_rp(&enc, RP_DE);
    z80_jr_label(&enc, "divmod10_u16_loop");
    z80_add_label(&enc, "divmod10_u16_done");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_ex_de_hl(&enc);
    z80_ret(&enc);

    z80_add_label(&enc, "number_to_string");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "number_to_string_positive");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_DE, "tostring_negative");
    z80_ld_r_n(&enc, REG_A, 1);
    z80_emit_b(&enc, 0x12);
    z80_jr_label(&enc, "number_to_string_start");
    z80_add_label(&enc, "number_to_string_positive");
    z80_ld_rp_label(&enc, RP_DE, "tostring_negative");
    z80_xor_a(&enc);
    z80_emit_b(&enc, 0x12);
    z80_add_label(&enc, "number_to_string_start");
    z80_ld_rp_label(&enc, RP_DE, "num_buffer_end");
    z80_add_label(&enc, "number_to_string_loop");
    z80_push(&enc, RP_DE);
    z80_call_label(&enc, "divmod10_u16");
    z80_pop(&enc, RP_DE);
    z80_add_a_n(&enc, '0'); z80_dec_rp(&enc, RP_DE);
    z80_emit_b(&enc, 0x12);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "number_to_string_loop");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "number_to_string_loop");
    z80_ld_rp_label(&enc, RP_HL, "tostring_negative");
    z80_ld_a_hl(&enc); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "number_to_string_done");
    z80_dec_rp(&enc, RP_DE);
    z80_ld_r_n(&enc, REG_A, '-');
    z80_emit_b(&enc, 0x12);
    z80_add_label(&enc, "number_to_string_done");
    z80_ld_rp_label(&enc, RP_HL, "num_buffer_end");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_ex_de_hl(&enc);
    z80_call_label(&enc, "alloc_raw_string");
    z80_ret(&enc);

    z80_add_label(&enc, "alloc_string_copy");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_call_label(&enc, "alloc_raw_string");
    z80_ret(&enc);

    z80_add_label(&enc, "alloc_raw_string"); // HL = source, DE = length
    z80_ld_mem_hl_label(&enc, "string_source");
    z80_ex_de_hl(&enc);
    z80_call_label(&enc, "alloc_string_space");
    z80_ld_r_r(&enc, REG_A, REG_D);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "alloc_raw_string_copy");
    z80_ld_r_r(&enc, REG_A, REG_E);
    z80_or_a(&enc);
    z80_ret(&enc);
    z80_add_label(&enc, "alloc_raw_string_copy");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "string_length");
    z80_ld_r_r(&enc, REG_B, REG_H);
    z80_ld_r_r(&enc, REG_C, REG_L);
    z80_ld_hl_mem_label(&enc, "string_source");
    z80_call_label(&enc, "copy_bc_bytes");
    z80_xor_a(&enc);
    z80_emit_b(&enc, 0x12);
    z80_pop(&enc, RP_HL);
    z80_ret(&enc);

    z80_add_label(&enc, "alloc_string_space"); // HL = length, returns HL = object, DE = payload or 0 on empty/fail
    z80_ld_mem_hl_label(&enc, "string_length");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "alloc_string_space_non_empty");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "alloc_string_space_empty");
    z80_add_label(&enc, "alloc_string_space_non_empty");
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL);
    z80_ex_de_hl(&enc);
    z80_ld_hl_mem_label(&enc, "string_ptr");
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_mem_hl_label(&enc, "string_candidate");
    z80_ex_de_hl(&enc);
    z80_ld_rp_label(&enc, RP_HL, "string_end");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jr_cc_label(&enc, CC_C, "alloc_string_space_empty");
    z80_ld_hl_mem_label(&enc, "string_ptr");
    z80_push(&enc, RP_HL);
    z80_ld_de_mem_label(&enc, "string_length");
    z80_ld_r_r(&enc, REG_M, REG_E);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_D);
    z80_inc_rp(&enc, RP_HL);
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "string_candidate");
    z80_ld_mem_hl_label(&enc, "string_ptr");
    z80_pop(&enc, RP_DE);
    z80_pop(&enc, RP_HL);
    z80_ret(&enc);
    z80_add_label(&enc, "alloc_string_space_empty");
    z80_ld_rp_label(&enc, RP_HL, "str_empty");
    z80_ld_rp_nn(&enc, RP_DE, 0);
    z80_ret(&enc);

    z80_add_label(&enc, "alloc_concat_string"); // HL = left string, DE = right string
    z80_ld_mem_hl_label(&enc, "concat_left_ptr");
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "concat_right_ptr");
    z80_ld_hl_mem_label(&enc, "concat_left_ptr");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "concat_left_len");
    z80_ld_hl_mem_label(&enc, "concat_right_ptr");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "concat_right_len");
    z80_ld_hl_mem_label(&enc, "concat_left_len");
    z80_ld_de_mem_label(&enc, "concat_right_len");
    z80_add_hl_rp(&enc, RP_DE);
    z80_call_label(&enc, "alloc_string_space");
    z80_ld_r_r(&enc, REG_A, REG_D);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "alloc_concat_copy");
    z80_ld_r_r(&enc, REG_A, REG_E);
    z80_or_a(&enc);
    z80_ret(&enc);
    z80_add_label(&enc, "alloc_concat_copy");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "concat_left_len");
    z80_ld_r_r(&enc, REG_B, REG_H);
    z80_ld_r_r(&enc, REG_C, REG_L);
    z80_ld_hl_mem_label(&enc, "concat_left_ptr");
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL);
    z80_call_label(&enc, "copy_bc_bytes");
    z80_ld_hl_mem_label(&enc, "concat_right_len");
    z80_ld_r_r(&enc, REG_B, REG_H);
    z80_ld_r_r(&enc, REG_C, REG_L);
    z80_ld_hl_mem_label(&enc, "concat_right_ptr");
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_HL);
    z80_call_label(&enc, "copy_bc_bytes");
    z80_xor_a(&enc);
    z80_emit_b(&enc, 0x12);
    z80_pop(&enc, RP_HL);
    z80_ret(&enc);

    z80_add_label(&enc, "copy_bc_bytes");
    z80_add_label(&enc, "copy_bc_bytes_check");
    z80_ld_r_r(&enc, REG_A, REG_B);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "copy_bc_bytes_body");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "copy_bc_bytes_done");
    z80_add_label(&enc, "copy_bc_bytes_body");
    z80_ld_a_hl(&enc);
    z80_emit_b(&enc, 0x12);
    z80_inc_rp(&enc, RP_HL);
    z80_inc_rp(&enc, RP_DE);
    z80_dec_rp(&enc, RP_BC);
    z80_jr_label(&enc, "copy_bc_bytes_check");
    z80_add_label(&enc, "copy_bc_bytes_done");
    z80_ret(&enc);

    z80_add_label(&enc, "coerce_to_string");
    z80_cp_a_n(&enc, TYPE_STRING);
    z80_jr_cc_label(&enc, CC_Z, "coerce_to_string_return");
    z80_cp_a_n(&enc, TYPE_NUMBER); z80_jr_cc_label(&enc, CC_Z, "coerce_to_string_number");
    z80_cp_a_n(&enc, TYPE_NIL); z80_jr_cc_label(&enc, CC_Z, "coerce_to_string_nil");
    z80_cp_a_n(&enc, TYPE_BOOL); z80_jr_cc_label(&enc, CC_Z, "coerce_to_string_bool");
    z80_cp_a_n(&enc, TYPE_TABLE); z80_jr_cc_label(&enc, CC_Z, "coerce_to_string_table");
    z80_ld_rp_label(&enc, RP_HL, "str_function_value");
    z80_ret(&enc);
    z80_add_label(&enc, "coerce_to_string_nil");
    z80_ld_rp_label(&enc, RP_HL, "str_nil_value");
    z80_ret(&enc);
    z80_add_label(&enc, "coerce_to_string_bool");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "coerce_to_string_true");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "coerce_to_string_true");
    z80_ld_rp_label(&enc, RP_HL, "str_false_value");
    z80_ret(&enc);
    z80_add_label(&enc, "coerce_to_string_true");
    z80_ld_rp_label(&enc, RP_HL, "str_true_value");
    z80_ret(&enc);
    z80_add_label(&enc, "coerce_to_string_table");
    z80_ld_rp_label(&enc, RP_HL, "str_table_value");
    z80_ret(&enc);
    z80_add_label(&enc, "coerce_to_string_number");
    z80_call_label(&enc, "number_to_string");
    z80_add_label(&enc, "coerce_to_string_return");
    z80_ret(&enc);

    z80_add_label(&enc, "string_to_number");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_push(&enc, RP_HL);
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "tonumber_remaining");
    z80_pop(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tonumber_ptr");
    z80_ld_rp_label(&enc, RP_DE, "tonumber_negative");
    z80_xor_a(&enc);
    z80_emit_b(&enc, 0x12);
    z80_ld_hl_mem_label(&enc, "tonumber_remaining");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "string_to_number_have_chars");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "string_to_number_fail");
    z80_add_label(&enc, "string_to_number_have_chars");
    z80_ld_hl_mem_label(&enc, "tonumber_ptr");
    z80_ld_a_hl(&enc);
    z80_cp_a_n(&enc, '-');
    z80_jr_cc_label(&enc, CC_NZ, "string_to_number_after_sign");
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tonumber_ptr");
    z80_ld_hl_mem_label(&enc, "tonumber_remaining");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tonumber_remaining");
    z80_ld_rp_label(&enc, RP_DE, "tonumber_negative");
    z80_ld_r_n(&enc, REG_A, 1);
    z80_emit_b(&enc, 0x12);
    z80_add_label(&enc, "string_to_number_after_sign");
    z80_ld_hl_mem_label(&enc, "tonumber_remaining");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "string_to_number_init");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "string_to_number_fail");
    z80_add_label(&enc, "string_to_number_init");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_mem_hl_label(&enc, "tonumber_result");
    z80_add_label(&enc, "string_to_number_loop");
    z80_ld_hl_mem_label(&enc, "tonumber_remaining");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "string_to_number_read_char");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "string_to_number_done_digits");
    z80_add_label(&enc, "string_to_number_read_char");
    z80_ld_hl_mem_label(&enc, "tonumber_ptr");
    z80_ld_a_hl(&enc);
    z80_sub_a_n(&enc, '0');
    z80_cp_a_n(&enc, 10);
    z80_jr_cc_label(&enc, CC_NC, "string_to_number_fail");
    z80_ld_r_r(&enc, REG_C, REG_A);
    z80_ld_hl_mem_label(&enc, "tonumber_result");
    z80_ld_rp_nn(&enc, RP_DE, 10);
    z80_call_label(&enc, "mul16");
    z80_ld_r_r(&enc, REG_A, REG_C);
    z80_ld_r_r(&enc, REG_E, REG_A);
    z80_ld_r_n(&enc, REG_D, 0);
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_mem_hl_label(&enc, "tonumber_result");
    z80_ld_hl_mem_label(&enc, "tonumber_ptr");
    z80_inc_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tonumber_ptr");
    z80_ld_hl_mem_label(&enc, "tonumber_remaining");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "tonumber_remaining");
    z80_jr_label(&enc, "string_to_number_loop");
    z80_add_label(&enc, "string_to_number_done_digits");
    z80_ld_hl_mem_label(&enc, "tonumber_result");
    z80_ld_rp_label(&enc, RP_DE, "tonumber_negative");
    z80_emit_b(&enc, 0x1A);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "string_to_number_success");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_add_label(&enc, "string_to_number_success");
    z80_ld_r_n(&enc, REG_A, 1);
    z80_ret(&enc);
    z80_add_label(&enc, "string_to_number_fail");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_xor_a(&enc);
    z80_ret(&enc);

    z80_add_label(&enc, "op_add");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_AF); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_pop(&enc, RP_AF);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_sub");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_AF); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_pop(&enc, RP_AF);
    z80_call_label(&enc, "vstack_push"); z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_jump");
    z80_ld_hl_mem_label(&enc, "pc_ptr");
    z80_ld_a_hl(&enc); z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_H, REG_M); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_de_mem_label(&enc, "pc_ptr");
    z80_inc_rp(&enc, RP_DE); z80_inc_rp(&enc, RP_DE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_mul");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_call_label(&enc, "mul16");
    z80_ld_r_n(&enc, REG_A, 2); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_div");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_D); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "div_nonzero");
    z80_ld_r_r(&enc, REG_A, REG_E); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "div_zero");
    z80_add_label(&enc, "div_nonzero");
    z80_ld_rp_nn(&enc, RP_BC, 0);
    z80_add_label(&enc, "div_loop16");
    z80_xor_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jr_cc_label(&enc, CC_C, "div_done16");
    z80_inc_rp(&enc, RP_BC);
    z80_jr_label(&enc, "div_loop16");
    z80_add_label(&enc, "div_done16");
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_H, REG_B); z80_ld_r_r(&enc, REG_L, REG_C);
    z80_jr_label(&enc, "div_push16");
    z80_add_label(&enc, "div_zero");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_add_label(&enc, "div_push16");
    z80_ld_r_n(&enc, REG_A, 2); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_mod");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_A, REG_D); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "mod_nonzero");
    z80_ld_r_r(&enc, REG_A, REG_E); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "mod_zero");
    z80_add_label(&enc, "mod_nonzero");
    z80_add_label(&enc, "mod_loop16");
    z80_xor_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jr_cc_label(&enc, CC_C, "mod_done16");
    z80_jr_label(&enc, "mod_loop16");
    z80_add_label(&enc, "mod_done16");
    z80_add_hl_rp(&enc, RP_DE);
    z80_jr_label(&enc, "mod_push16");
    z80_add_label(&enc, "mod_zero");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_add_label(&enc, "mod_push16");
    z80_ld_r_n(&enc, REG_A, 2); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_pow");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "pow_exp");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "pow_base");
    z80_ld_hl_mem_label(&enc, "pow_exp");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_emit_b(&enc, 0xE6); z80_emit_b(&enc, 0x80);
    z80_jr_cc_label(&enc, CC_Z, "pow_non_negative");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jr_label(&enc, "pow_push");
    z80_add_label(&enc, "pow_non_negative");
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_ld_mem_hl_label(&enc, "pow_result");
    z80_add_label(&enc, "pow_loop_check");
    z80_ld_hl_mem_label(&enc, "pow_exp");
    z80_ld_r_r(&enc, REG_A, REG_H);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_NZ, "pow_loop_body");
    z80_ld_r_r(&enc, REG_A, REG_L);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "pow_done");
    z80_add_label(&enc, "pow_loop_body");
    z80_ld_hl_mem_label(&enc, "pow_result");
    z80_ld_de_mem_label(&enc, "pow_base");
    z80_call_label(&enc, "mul16");
    z80_ld_mem_hl_label(&enc, "pow_result");
    z80_ld_hl_mem_label(&enc, "pow_exp");
    z80_dec_rp(&enc, RP_HL);
    z80_ld_mem_hl_label(&enc, "pow_exp");
    z80_jr_label(&enc, "pow_loop_check");
    z80_add_label(&enc, "pow_done");
    z80_ld_hl_mem_label(&enc, "pow_result");
    z80_add_label(&enc, "pow_push");
    z80_ld_r_n(&enc, REG_A, TYPE_NUMBER);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_neg");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_L, REG_A);
    z80_ld_r_r(&enc, REG_A, REG_H); z80_emit_b(&enc, 0x2F); z80_ld_r_r(&enc, REG_H, REG_A);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_n(&enc, REG_A, 2); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_not");
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, 1);
    z80_jr_cc_label(&enc, CC_Z, "not_bool");
    z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "not_true");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jr_label(&enc, "not_push");
    z80_add_label(&enc, "not_bool");
    z80_ld_r_r(&enc, REG_A, REG_H); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_NZ, "not_false");
    z80_ld_r_r(&enc, REG_A, REG_L); z80_or_a(&enc); z80_jr_cc_label(&enc, CC_Z, "not_true");
    z80_add_label(&enc, "not_false");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jr_label(&enc, "not_push");
    z80_add_label(&enc, "not_true");
    z80_ld_rp_nn(&enc, RP_HL, 1);
    z80_add_label(&enc, "not_push");
    z80_ld_r_n(&enc, REG_A, 1); z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_len");
    z80_call_label(&enc, "vstack_pop");
    z80_cp_a_n(&enc, TYPE_STRING);
    z80_jr_cc_label(&enc, CC_Z, "len_string");
    z80_cp_a_n(&enc, TYPE_TABLE);
    z80_jr_cc_label(&enc, CC_Z, "len_table");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_jr_label(&enc, "len_push");

    z80_add_label(&enc, "len_string");
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_ex_de_hl(&enc);
    z80_jr_label(&enc, "len_push");

    z80_add_label(&enc, "len_table");
    z80_ld_mem_hl_label(&enc, "len_table_ptr");
    z80_ld_rp_nn(&enc, RP_DE, 1);
    z80_add_label(&enc, "len_table_outer");
    z80_ex_de_hl(&enc);
    z80_ld_mem_hl_label(&enc, "len_target");
    z80_ex_de_hl(&enc);
    z80_ld_hl_mem_label(&enc, "len_table_ptr");
    z80_ld_r_r(&enc, REG_B, REG_M);
    z80_ld_r_r(&enc, REG_A, REG_B);
    z80_or_a(&enc);
    z80_jr_cc_label(&enc, CC_Z, "len_table_not_found");
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_add_label(&enc, "len_table_loop");
    z80_push(&enc, RP_HL);
    z80_push(&enc, RP_BC);
    z80_ld_r_r(&enc, REG_C, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_B, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_a_hl(&enc);
    z80_cp_a_n(&enc, TYPE_NUMBER);
    z80_jr_cc_label(&enc, CC_NZ, "len_table_next");
    z80_ld_hl_mem_label(&enc, "len_target");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x42);
    z80_jr_cc_label(&enc, CC_Z, "len_table_found");
    z80_add_label(&enc, "len_table_next");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, TABLE_ENTRY_SIZE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_djnz_label(&enc, "len_table_loop");
    z80_jr_label(&enc, "len_table_not_found");
    z80_add_label(&enc, "len_table_found");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_ld_de_mem_label(&enc, "len_target");
    z80_inc_rp(&enc, RP_DE);
    z80_jr_label(&enc, "len_table_outer");
    z80_add_label(&enc, "len_table_not_found");
    z80_ld_hl_mem_label(&enc, "len_target");
    z80_dec_rp(&enc, RP_HL);

    z80_add_label(&enc, "len_push");
    z80_ld_r_n(&enc, REG_A, TYPE_NUMBER);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_newtable");
    z80_ld_hl_mem_label(&enc, "heap_ptr");
    z80_push(&enc, RP_HL);
    z80_xor_a(&enc);
    z80_ld_hl_a(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_hl_a(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_n(&enc, REG_A, TABLE_CAPACITY);
    z80_ld_hl_a(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_xor_a(&enc);
    z80_ld_hl_a(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, TABLE_CAPACITY * TABLE_ENTRY_SIZE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_mem_hl_label(&enc, "heap_ptr");
    z80_pop(&enc, RP_HL);
    z80_ld_r_n(&enc, REG_A, TYPE_TABLE);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_gettable");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_key_temp");
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_hl_a(&enc);
    z80_call_label(&enc, "vstack_pop");
    z80_ld_r_r(&enc, REG_B, REG_M);
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_A, REG_B); z80_or_a(&enc);
    z80_jp_cc_label(&enc, CC_Z, "get_table_not_found");
    z80_add_label(&enc, "get_table_loop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_BC);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_C, REG_M);
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_a_hl(&enc);
    z80_cp_a_r(&enc, REG_C);
    z80_jp_cc_label(&enc, CC_NZ, "get_table_next");
    z80_cp_a_n(&enc, TYPE_STRING);
    z80_jp_cc_label(&enc, CC_Z, "get_table_string_compare");
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jp_cc_label(&enc, CC_NZ, "get_table_next");
    z80_jp_label(&enc, "get_table_match");
    z80_add_label(&enc, "get_table_string_compare");
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_call_label(&enc, "strcmp_hl_de");
    z80_jp_cc_label(&enc, CC_NZ, "get_table_next");
    z80_add_label(&enc, "get_table_match");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, 3);
    z80_add_hl_rp(&enc, RP_DE);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_a_hl(&enc);
    z80_ex_de_hl(&enc);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");
    z80_add_label(&enc, "get_table_next");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, TABLE_ENTRY_SIZE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_djnz_label(&enc, "get_table_loop");
    z80_add_label(&enc, "get_table_not_found");
    z80_ld_rp_nn(&enc, RP_HL, 0);
    z80_ld_r_n(&enc, REG_A, TYPE_NIL);
    z80_call_label(&enc, "vstack_push");
    z80_jp_label(&enc, "vm_loop");

    z80_add_label(&enc, "op_settable");
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_val_temp");
    z80_ld_rp_label(&enc, RP_HL, "table_val_type");
    z80_ld_hl_a(&enc);
    z80_call_label(&enc, "vstack_pop");
    z80_ld_mem_hl_label(&enc, "table_key_temp");
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_hl_a(&enc);
    z80_call_label(&enc, "vstack_pop");
    z80_push(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_B, REG_M);
    z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL); z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_A, REG_B); z80_or_a(&enc);
    z80_jp_cc_label(&enc, CC_Z, "set_table_new_entry");
    z80_add_label(&enc, "set_table_loop");
    z80_push(&enc, RP_HL); z80_push(&enc, RP_BC);
    z80_ld_r_r(&enc, REG_E, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_D, REG_M);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_C, REG_M);
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_a_hl(&enc);
    z80_cp_a_r(&enc, REG_C);
    z80_jp_cc_label(&enc, CC_NZ, "set_table_next");
    z80_cp_a_n(&enc, TYPE_STRING);
    z80_jp_cc_label(&enc, CC_Z, "set_table_string_compare");
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jp_cc_label(&enc, CC_NZ, "set_table_next");
    z80_jp_label(&enc, "set_table_match");
    z80_add_label(&enc, "set_table_string_compare");
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_call_label(&enc, "strcmp_hl_de");
    z80_jp_cc_label(&enc, CC_NZ, "set_table_next");
    z80_add_label(&enc, "set_table_match");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_pop(&enc, RP_BC);
    z80_ld_rp_nn(&enc, RP_DE, 3);
    z80_add_hl_rp(&enc, RP_DE);
    z80_jp_label(&enc, "set_table_write_value");
    z80_add_label(&enc, "set_table_next");
    z80_pop(&enc, RP_BC);
    z80_pop(&enc, RP_HL);
    z80_ld_rp_nn(&enc, RP_DE, TABLE_ENTRY_SIZE);
    z80_add_hl_rp(&enc, RP_DE);
    z80_djnz_label(&enc, "set_table_loop");
    z80_add_label(&enc, "set_table_new_entry");
    z80_ld_mem_hl_label(&enc, "table_entry_temp");
    z80_pop(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_A, REG_M);
    z80_cp_a_n(&enc, TABLE_CAPACITY);
    z80_jp_cc_label(&enc, CC_NC, "set_table_full");
    z80_inc_r(&enc, REG_M);
    z80_ld_hl_mem_label(&enc, "table_entry_temp");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_ex_de_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_E);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_D);
    z80_inc_rp(&enc, RP_HL);
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_HL, "table_key_type");
    z80_ld_a_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_ld_hl_a(&enc);
    z80_inc_rp(&enc, RP_HL);
    z80_jp_label(&enc, "set_table_write_value");
    z80_add_label(&enc, "set_table_full");
    z80_jp_label(&enc, "vm_loop");
    z80_add_label(&enc, "set_table_write_value");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "table_val_temp");
    z80_ex_de_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_E);
    z80_inc_rp(&enc, RP_HL);
    z80_ld_r_r(&enc, REG_M, REG_D);
    z80_inc_rp(&enc, RP_HL);
    z80_push(&enc, RP_HL);
    z80_ld_rp_label(&enc, RP_HL, "table_val_type");
    z80_ld_a_hl(&enc);
    z80_pop(&enc, RP_HL);
    z80_ld_hl_a(&enc);
    z80_jp_label(&enc, "vm_loop");
}
#endif

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
    emit_io_and_arithmetic_ops();
    emit_scope_ops();
    emit_compare_stack_and_data(chunk);

    z80_resolve_refs();

    if (!export_symbols(&enc, out_filename)) {
        return false;
    }

    return write_image_file(out_filename);
}
