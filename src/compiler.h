#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "bytecode.h"

#define MAX_CODE_SIZE 2048
#define MAX_CONSTANTS 256
#define MAX_FUNCTIONS 64
#define MAX_GLOBALS 256
#define MAX_LOCALS 256
#define MAX_UPVALUES 64

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
    bool from_local;
    uint8_t index;
} UpvalueDesc;

typedef struct {
    const char* name;
    uint8_t code[MAX_CODE_SIZE];
    uint16_t code_len;
    Constant constants[MAX_CONSTANTS];
    uint16_t const_count;
    const char* locals[MAX_LOCALS];
    uint16_t local_count;
    uint8_t param_count;
    uint8_t initial_local_count;
    bool local_is_captured[MAX_LOCALS];
    uint8_t local_env_slot[MAX_LOCALS];
    uint8_t env_local_count;
    UpvalueDesc upvalues[MAX_UPVALUES];
    uint8_t upvalue_count;
} BytecodeFunction;

typedef struct {
    BytecodeFunction main;
    BytecodeFunction functions[MAX_FUNCTIONS];
    uint16_t func_count;
    const char* globals[MAX_GLOBALS];
    uint16_t global_count;
    bool has_error;
    uint16_t error_line;
    uint16_t error_column;
    char error_msg[64];
} CompiledChunk;

void compiler_init(void);
bool compiler_compile(Chunk* ast_chunk, CompiledChunk* out_chunk);

#endif
