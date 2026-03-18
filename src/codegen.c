#include "codegen.h"
#include "z80_encoder.h"
#ifdef __SDCC
#include "zos_vfs.h"
#include "zos_sys.h"
#else
#include "zos_host_stub.h"
#endif
#include <string.h>

#define MAX_IMAGE_SIZE 16384
#define TYPE_NIL 0
#define TYPE_BOOL 1
#define TYPE_NUMBER 2
#define TYPE_TABLE 4
#define TABLE_CAPACITY 8
#define TABLE_ENTRY_SIZE 6
#define TABLE_SIZE (4 + (TABLE_CAPACITY * TABLE_ENTRY_SIZE))
#define TABLE_HEAP_BYTES (TABLE_SIZE * 8)

static uint8_t image[MAX_IMAGE_SIZE];
static Z80Encoder enc;

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

    z80_add_label(&enc, "_start");
    z80_ld_rp_label(&enc, RP_HL, "heap_space");
    z80_ld_mem_hl_label(&enc, "heap_ptr");
    z80_ld_rp_label(&enc, RP_HL, "vstack_end");
    z80_ld_mem_hl_label(&enc, "vsp_ptr");
    z80_ld_mem_hl_label(&enc, "fp_ptr");
    z80_ld_rp_label(&enc, RP_HL, "bytecode_main");
    z80_ld_mem_hl_label(&enc, "pc_ptr");
    z80_jr_label(&enc, "vm_loop");

    z80_add_label(&enc, "pc_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "vsp_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "fp_ptr"); z80_emit_w(&enc, 0);

    z80_add_label(&enc, "bytecode_main");
    for (i = 0; i < chunk->main.code_len; i++) {
        z80_emit_b(&enc, chunk->main.code[i]);
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
    z80_cp_a_n(&enc, 0x30); z80_jp_cc_label(&enc, CC_Z, "op_newtable");
    z80_cp_a_n(&enc, 0x31); z80_jp_cc_label(&enc, CC_Z, "op_gettable");
    z80_cp_a_n(&enc, 0x32); z80_jp_cc_label(&enc, CC_Z, "op_settable");
    z80_cp_a_n(&enc, 0x40); z80_jp_cc_label(&enc, CC_Z, "op_add");
    z80_cp_a_n(&enc, 0x41); z80_jp_cc_label(&enc, CC_Z, "op_sub");
    z80_cp_a_n(&enc, 0x42); z80_jp_cc_label(&enc, CC_Z, "op_mul");
    z80_cp_a_n(&enc, 0x43); z80_jp_cc_label(&enc, CC_Z, "op_div");
    z80_cp_a_n(&enc, 0x44); z80_jp_cc_label(&enc, CC_Z, "op_mod");
    z80_cp_a_n(&enc, 0x46); z80_jp_cc_label(&enc, CC_Z, "op_neg");
    z80_cp_a_n(&enc, 0x50); z80_jp_cc_label(&enc, CC_Z, "op_eq");
    z80_cp_a_n(&enc, 0x51); z80_jp_cc_label(&enc, CC_Z, "op_ne");
    z80_cp_a_n(&enc, 0x52); z80_jp_cc_label(&enc, CC_Z, "op_lt");
    z80_cp_a_n(&enc, 0x53); z80_jp_cc_label(&enc, CC_Z, "op_le");
    z80_cp_a_n(&enc, 0x54); z80_jp_cc_label(&enc, CC_Z, "op_gt");
    z80_cp_a_n(&enc, 0x55); z80_jp_cc_label(&enc, CC_Z, "op_ge");
    z80_cp_a_n(&enc, 0x60); z80_jp_cc_label(&enc, CC_Z, "op_not");
    z80_cp_a_n(&enc, 0x80); z80_jp_cc_label(&enc, CC_Z, "op_jump");
    z80_cp_a_n(&enc, 0x81); z80_jp_cc_label(&enc, CC_Z, "op_jump_false");
    z80_cp_a_n(&enc, 0xA0); z80_jp_cc_label(&enc, CC_Z, "op_print");
    z80_jp_label(&enc, "vm_loop");
}

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
    z80_ld_rp_label(&enc, RP_DE, "const_pool"); z80_add_hl_rp(&enc, RP_DE);
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
    z80_cp_a_n(&enc, 2); // Number
    z80_jp_cc_label(&enc, CC_NZ, "op_print_next");
    z80_call_label(&enc, "print_num");
    z80_add_label(&enc, "op_print_next");
    z80_ld_rp_label(&enc, RP_HL, "str_newline"); z80_call_label(&enc, "print_str");
    z80_pop(&enc, RP_BC); z80_djnz_label(&enc, "op_print_loop");
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
    z80_ex_de_hl(&enc); z80_call_label(&enc, "print_str");
    z80_ret(&enc);

    z80_add_label(&enc, "print_str"); // HL = null-terminated string
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

    z80_add_label(&enc, "div16_8"); // HL = HL / 10, returns rem in A
    z80_xor_a(&enc); z80_ld_r_n(&enc, REG_B, 16);
    z80_add_label(&enc, "div_l"); z80_add_hl_rp(&enc, RP_HL); z80_emit_b(&enc, 0x17); // rla
    z80_cp_a_n(&enc, 10); z80_jr_cc_label(&enc, CC_NC, "div_s");
    z80_sub_a_n(&enc, 10); z80_inc_r(&enc, REG_L);
    z80_add_label(&enc, "div_s"); z80_djnz_label(&enc, "div_l"); z80_ret(&enc);

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
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jp_cc_label(&enc, CC_NZ, "get_table_next");
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
    z80_ld_hl_mem_label(&enc, "table_key_temp");
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52);
    z80_jp_cc_label(&enc, CC_NZ, "set_table_next");
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

static void emit_compare_stack_and_data(CompiledChunk* chunk) {
    uint16_t i;

    z80_add_label(&enc, "op_eq");
    z80_call_label(&enc, "vstack_pop"); z80_push(&enc, RP_HL);
    z80_call_label(&enc, "vstack_pop"); z80_pop(&enc, RP_DE);
    z80_or_a(&enc); z80_emit_b(&enc, 0xED); z80_emit_b(&enc, 0x52); // sbc hl, de
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
    z80_cp_a_n(&enc, 1); // Boolean
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

    z80_add_label(&enc, "mul16"); // HL = HL * DE
    z80_push(&enc, RP_BC); z80_ld_r_n(&enc, REG_B, 16); z80_ld_r_r(&enc, REG_A, REG_H); z80_ld_r_n(&enc, REG_H, 0);
    z80_ex_de_hl(&enc); z80_ld_r_r(&enc, REG_C, REG_L); z80_ld_r_r(&enc, REG_L, REG_A); z80_ld_r_r(&enc, REG_A, REG_C); z80_ld_r_n(&enc, REG_C, 0);
    z80_add_label(&enc, "mul16_l"); z80_add_hl_rp(&enc, RP_HL); z80_emit_b(&enc, 0x17); // rla
    z80_jr_cc_label(&enc, CC_NC, "mul16_s");
    z80_add_hl_rp(&enc, RP_DE); z80_adc_a_r(&enc, REG_C);
    z80_add_label(&enc, "mul16_s"); z80_djnz_label(&enc, "mul16_l");
    z80_pop(&enc, RP_BC); z80_ret(&enc);

    z80_add_label(&enc, "vstack_push");
    z80_push(&enc, RP_HL);
    z80_ld_hl_mem_label(&enc, "vsp_ptr");
    z80_dec_rp(&enc, RP_HL);
    z80_pop(&enc, RP_DE);
    z80_ld_hl_a(&enc); // wait, ld_hl_a is ld (hl), a. correct.
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

    z80_add_label(&enc, "const_pool");
    for (i = 0; i < chunk->main.const_count; i++) {
        Constant* c = &chunk->main.constants[i];
        if (c->type == CONST_NUMBER) {
            z80_emit_b(&enc, 2);
            z80_emit_w(&enc, (uint16_t)c->data.number);
        } else {
            z80_emit_b(&enc, 0); z80_emit_w(&enc, 0);
        }
    }

    z80_add_label(&enc, "global_vars");
    for(int i=0; i<768; i++) z80_emit_b(&enc, 0);

    z80_add_label(&enc, "heap_ptr"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_key_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_key_type"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "table_val_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "table_val_type"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "table_entry_temp"); z80_emit_w(&enc, 0);
    z80_add_label(&enc, "heap_space"); for(int i=0; i<TABLE_HEAP_BYTES; i++) z80_emit_b(&enc, 0);

    z80_add_label(&enc, "vstack_end");

    z80_add_label(&enc, "str_newline"); z80_emit_b(&enc, 10); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "num_buffer"); for(int i=0; i<16; i++) z80_emit_b(&enc, 0);
    z80_add_label(&enc, "num_buffer_end"); z80_emit_b(&enc, 0);
    z80_add_label(&enc, "tmp_len"); z80_emit_w(&enc, 0);
}

static bool write_image_file(const char* out_filename) {
    zos_dev_t bin;
    uint16_t written;
    zos_err_t err;

    bin = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (bin < 0) return false;

    written = enc.size;
    err = write(bin, image, &written);
    close(bin);

    return err == ERR_SUCCESS && written == enc.size;
}

bool codegen_generate(CompiledChunk* chunk, const char* out_filename) {
    z80_init(&enc, image, MAX_IMAGE_SIZE, 0x4000);
    memset(image, 0, MAX_IMAGE_SIZE);

    emit_entry_and_dispatch(chunk);
    emit_io_and_arithmetic_ops();
    emit_scope_ops();
    emit_compare_stack_and_data(chunk);

    z80_resolve_refs(&enc);

    if (!export_symbols(&enc, out_filename)) {
        return false;
    }

    return write_image_file(out_filename);
}
