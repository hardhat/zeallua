#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "bytecode.h"

#define MAX_CODE_SIZE 2048
#define MAX_CONSTANTS 256
#define MAX_FUNCTIONS 64
#define MAX_GLOBALS 256
#define MAX_LOCALS 256

typedef enum {
    CONST_NUMBER,
    CONST_STRING,
    CONST_FUNCTION
} ConstantType;

typedef struct {
    ConstantType type;
    union {
        int16_t number;
        const char* string;
        uint16_t func_idx;
    } data;
} Constant;

typedef struct {
    const char* name;
    uint8_t code[MAX_CODE_SIZE];
    uint16_t code_len;
    Constant constants[MAX_CONSTANTS];
    uint16_t const_count;
    const char* locals[MAX_LOCALS];
    uint16_t local_count;
} BytecodeFunction;

typedef struct {
    BytecodeFunction main;
    BytecodeFunction functions[MAX_FUNCTIONS];
    uint16_t func_count;
    const char* globals[MAX_GLOBALS];
    uint16_t global_count;
} CompiledChunk;

void compiler_init(void);
bool compiler_compile(Chunk* ast_chunk, CompiledChunk* out_chunk);

#endif
