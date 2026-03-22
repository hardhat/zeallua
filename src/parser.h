#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer* lex;
    Token curr;
    Token next;
    bool has_error;
    uint16_t error_line;
    uint16_t error_column;
    char error_msg[64];
} Parser;

void parser_init(Parser* p, Lexer* lex);
Chunk* parser_parse(Parser* p);

#endif
