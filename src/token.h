#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

typedef enum {
    // Literals
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NIL,

    // Keywords
    TOK_AND, TOK_BREAK, TOK_DO, TOK_ELSE, TOK_ELSEIF, TOK_END,
    TOK_FOR, TOK_FUNCTION, TOK_IF, TOK_IN, TOK_LOCAL, TOK_NOT,
    TOK_OR, TOK_REPEAT, TOK_RETURN, TOK_THEN, TOK_UNTIL, TOK_WHILE,

    // Operators
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_CARET,
    TOK_HASH, TOK_EQEQ, TOK_TILDEEQ, TOK_LTEQ, TOK_GTEQ, TOK_LT,
    TOK_GT, TOK_EQ, TOK_DOTDOT, TOK_DOTDOTDOT,

    // Delimiters
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET,
    TOK_RBRACKET, TOK_SEMI, TOK_COLON, TOK_COMMA, TOK_DOT,

    // Special
    TOK_EOF,
    TOK_ERROR
} TokenType;

#define MAX_TOKEN_STR_LEN 32

typedef struct {
    TokenType type;
    union {
        int16_t number;
        char    string[MAX_TOKEN_STR_LEN];
        char    ident[MAX_TOKEN_STR_LEN];
    } value;
    uint16_t line;
    uint16_t column;
} Token;

// Utility functions
TokenType token_check_keyword(const char* s);
const char* token_type_to_str(TokenType t);

#endif
