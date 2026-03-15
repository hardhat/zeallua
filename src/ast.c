#include "ast.h"

static char ast_pool[AST_POOL_SIZE];
static uint16_t ast_pool_offset = 0;

static char str_pool[STR_POOL_SIZE];
static uint16_t str_pool_offset = 0;

void ast_reset(void) {
    ast_pool_offset = 0;
    str_pool_offset = 0;
}

void* ast_alloc(uint16_t size) {
    if (ast_pool_offset + size > AST_POOL_SIZE) {
        // Out of memory - will need robust error handling
        return 0; // NULL
    }
    void* ptr = &ast_pool[ast_pool_offset];
    ast_pool_offset += size;
    return ptr;
}

const char* ast_strdup(const char* str) {
    if (!str) return 0;
    
    // Calculate length
    uint16_t len = 0;
    while(str[len]) len++;
    len++; // null terminator
    
    if (str_pool_offset + len > STR_POOL_SIZE) {
        return 0; // OOM
    }
    
    char* ptr = &str_pool[str_pool_offset];
    for (uint16_t i = 0; i < len; i++) {
        ptr[i] = str[i];
    }
    str_pool_offset += len;
    return ptr;
}
