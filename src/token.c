#include "token.h"

int z_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static const struct {
    const char* str;
    TokenType type;
} keywords[] = {
    {"and", TOK_AND}, {"break", TOK_BREAK}, {"do", TOK_DO},
    {"else", TOK_ELSE}, {"elseif", TOK_ELSEIF}, {"end", TOK_END},
    {"false", TOK_FALSE}, {"for", TOK_FOR}, {"function", TOK_FUNCTION},
    {"if", TOK_IF}, {"in", TOK_IN}, {"local", TOK_LOCAL},
    {"nil", TOK_NIL}, {"not", TOK_NOT}, {"or", TOK_OR},
    {"repeat", TOK_REPEAT}, {"return", TOK_RETURN}, {"then", TOK_THEN},
    {"true", TOK_TRUE}, {"until", TOK_UNTIL}, {"while", TOK_WHILE},
    {0, TOK_ERROR}
};

TokenType token_check_keyword(const char* s) {
    for (int i = 0; keywords[i].str != 0; i++) {
        if (z_strcmp(keywords[i].str, s) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

const char* const type_names[] = {
    "TOK_NUMBER", "TOK_STRING", "TOK_IDENT", "TOK_TRUE", "TOK_FALSE", "TOK_NIL",
    "TOK_AND", "TOK_BREAK", "TOK_DO", "TOK_ELSE", "TOK_ELSEIF", "TOK_END",
    "TOK_FOR", "TOK_FUNCTION", "TOK_IF", "TOK_IN", "TOK_LOCAL", "TOK_NOT",
    "TOK_OR", "TOK_REPEAT", "TOK_RETURN", "TOK_THEN", "TOK_UNTIL", "TOK_WHILE",
    "TOK_PLUS", "TOK_MINUS", "TOK_STAR", "TOK_SLASH", "TOK_PERCENT", "TOK_CARET",
    "TOK_HASH", "TOK_EQEQ", "TOK_TILDEEQ", "TOK_LTEQ", "TOK_GTEQ", "TOK_LT",
    "TOK_GT", "TOK_EQ", "TOK_DOTDOT", "TOK_DOTDOTDOT",
    "TOK_LPAREN", "TOK_RPAREN", "TOK_LBRACE", "TOK_RBRACE", "TOK_LBRACKET",
    "TOK_RBRACKET", "TOK_SEMI", "TOK_COLON", "TOK_COMMA", "TOK_DOT",
    "TOK_EOF", "TOK_ERROR"
};

const char* token_type_to_str(TokenType t) {
    if (t >= 0 && t <= TOK_ERROR) {
        return type_names[t];
    }
    return "UNKNOWN";
}
