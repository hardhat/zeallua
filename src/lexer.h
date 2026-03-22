#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdbool.h>

typedef struct {
    const char* input;
    uint16_t length;
    uint16_t pos;
    uint16_t line;
    uint16_t column;
    bool has_error;
    char error_msg[64];
} Lexer;

void lexer_init(Lexer* lex, const char* input, uint16_t length);
void lexer_next_token(Lexer* lex, Token* out_tok);

#endif
