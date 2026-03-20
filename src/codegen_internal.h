#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include "codegen.h"
#include "z80_encoder.h"

#define TYPE_NIL 0
#define TYPE_BOOL 1
#define TYPE_NUMBER 2
#define TYPE_STRING 3
#define TYPE_TABLE 4
#define TYPE_FUNCTION 5
#define TABLE_CAPACITY 8
#define TABLE_ENTRY_SIZE 6
#define TABLE_SIZE (4 + (TABLE_CAPACITY * TABLE_ENTRY_SIZE))
#define STRING_HEADER_BYTES 2
#define FUNCTION_HEADER_BYTES 6
#define TABLE_HEAP_BYTES (TABLE_SIZE * 8)
#define STRING_HEAP_BYTES 256
#define CLOSURE_HEAP_BYTES 512
#define CALL_STACK_BYTES 160
#define VSTACK_BYTES 512

extern Z80Encoder enc;

void make_indexed_label(char* dst, uint16_t cap, const char* prefix, uint16_t index);
void make_two_index_label(char* dst, uint16_t cap, const char* prefix, uint16_t first, uint16_t second);
void emit_string_object(Z80Encoder* e, const char* label, const char* text);
void emit_function_constant_pool(const char* pool_label, const char* string_prefix, BytecodeFunction* func);
void emit_io_and_arithmetic_ops(void);
void emit_compare_stack_and_data(CompiledChunk* chunk);

#endif