#include "lexer.h"

// Basic character checks since ctype.h might be heavy
static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_hexdigit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_alphanum(char c) {
    return is_alpha(c) || is_digit(c) || (c == '_');
}

void lexer_init(Lexer* lex, const char* input, uint16_t length) {
    lex->input = input;
    lex->length = length;
    lex->pos = 0;
    lex->line = 1;
    lex->has_error = false;
    lex->error_msg[0] = '\0';
}

static char peek(Lexer* lex) {
    if (lex->pos >= lex->length) return '\0';
    return lex->input[lex->pos];
}

static char peek_next(Lexer* lex) {
    if (lex->pos + 1 >= lex->length) return '\0';
    return lex->input[lex->pos + 1];
}

static char advance(Lexer* lex) {
    char c = peek(lex);
    if (c != '\0') {
        if (c == '\n') {
            lex->line++;
        }
        lex->pos++;
    }
    return c;
}

static void skip_whitespace_and_comments(Lexer* lex) {
    while (1) {
        char c = peek(lex);
        if (is_whitespace(c)) {
            advance(lex);
        } else if (c == '-' && peek_next(lex) == '-') {
            // Comment
            advance(lex); // -
            advance(lex); // -
            
            // Check for block comment --[[ ]]
            if (peek(lex) == '[' && peek_next(lex) == '[') {
                advance(lex); // [
                advance(lex); // [
                // skip until ]]
                while (peek(lex) != '\0') {
                    if (peek(lex) == ']' && peek_next(lex) == ']') {
                        advance(lex);
                        advance(lex);
                        break;
                    }
                    advance(lex);
                }
            } else {
                // line comment
                while (peek(lex) != '\0' && peek(lex) != '\n') {
                    advance(lex);
                }
            }
        } else {
            break;
        }
    }
}

static void make_error(Lexer* lex, const char* msg, Token* out_tok) {
    out_tok->type = TOK_ERROR;
    out_tok->line = lex->line;
    lex->has_error = true;
    
    // Copy error msg safely
    int i = 0;
    while (msg[i] != '\0' && i < sizeof(lex->error_msg) - 1) {
        lex->error_msg[i] = msg[i];
        i++;
    }
    lex->error_msg[i] = '\0';
}

static void read_string(Lexer* lex, char quote, Token* out_tok) {
    out_tok->line = lex->line;
    advance(lex); // consume quote
    
    int i = 0;
    while (peek(lex) != '\0') {
        char c = peek(lex);
        if (c == quote) {
            advance(lex);
            out_tok->type = TOK_STRING;
            out_tok->value.string[i] = '\0';
            return;
        } else if (c == '\\') {
            advance(lex);
            char nc = advance(lex);
            char esc = nc;
            if (nc == 'n') esc = '\n';
            else if (nc == 'r') esc = '\r';
            else if (nc == 't') esc = '\t';
            // otherwise keep nc
            if (i < MAX_TOKEN_STR_LEN - 1) {
                out_tok->value.string[i++] = esc;
            }
        } else if (c == '\n') {
            make_error(lex, "Unterminated string", out_tok);
            return;
        } else {
            advance(lex);
            if (i < MAX_TOKEN_STR_LEN - 1) {
                out_tok->value.string[i++] = c;
            }
        }
    }
    make_error(lex, "Unterminated string", out_tok);
}

static void read_number(Lexer* lex, Token* out_tok) {
    out_tok->line = lex->line;
    bool is_hex = false;
    
    char buffer[16];
    int buf_len = 0;
    
    if (peek(lex) == '0' && (peek_next(lex) == 'x' || peek_next(lex) == 'X')) {
        buffer[buf_len++] = advance(lex); // 0
        buffer[buf_len++] = advance(lex); // x
        is_hex = true;
    }
    
    while (peek(lex) != '\0') {
        char c = peek(lex);
        if (is_hex && is_hexdigit(c)) {
            if (buf_len < 15) buffer[buf_len++] = advance(lex);
            else advance(lex);
        } else if (!is_hex && is_digit(c)) {
            if (buf_len < 15) buffer[buf_len++] = advance(lex);
            else advance(lex);
        } else {
            break;
        }
    }
    buffer[buf_len] = '\0';
    
    int16_t num = 0;
    if (is_hex) {
        for (int i = 2; i < buf_len; i++) {
            char c = buffer[i];
            int val = 0;
            if (c >= '0' && c <= '9') val = c - '0';
            else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
            num = num * 16 + val;
        }
    } else {
        bool neg = false;
        int start = 0;
        if (buffer[0] == '-') { neg = true; start = 1; }
        for (int i = start; i < buf_len; i++) {
            num = num * 10 + (buffer[i] - '0');
        }
        if (neg) num = -num;
    }
    
    out_tok->type = TOK_NUMBER;
    out_tok->value.number = num;
}

static void read_ident(Lexer* lex, Token* out_tok) {
    out_tok->line = lex->line;
    int i = 0;
    
    while (peek(lex) != '\0' && is_alphanum(peek(lex))) {
        char c = advance(lex);
        if (i < MAX_TOKEN_STR_LEN - 1) {
            out_tok->value.ident[i++] = c;
        }
    }
    out_tok->value.ident[i] = '\0';
    
    out_tok->type = token_check_keyword(out_tok->value.ident);
}

void lexer_next_token(Lexer* lex, Token* out_tok) {
    if (lex->has_error) {
        out_tok->type = TOK_ERROR;
        return;
    }
    
    skip_whitespace_and_comments(lex);
    out_tok->line = lex->line;
    
    char c = peek(lex);
    if (c == '\0') {
        out_tok->type = TOK_EOF;
        return;
    }
    
    if (c == '"' || c == '\'') { read_string(lex, c, out_tok); return; }
    if (is_digit(c)) { read_number(lex, out_tok); return; }
    if (is_alpha(c) || c == '_') { read_ident(lex, out_tok); return; }
    
    advance(lex);
    
    switch (c) {
        case '+': out_tok->type = TOK_PLUS; return;
        case '-': out_tok->type = TOK_MINUS; return;
        case '*': out_tok->type = TOK_STAR; return;
        case '/': out_tok->type = TOK_SLASH; return;
        case '%': out_tok->type = TOK_PERCENT; return;
        case '^': out_tok->type = TOK_CARET; return;
        case '#': out_tok->type = TOK_HASH; return;
        case '(': out_tok->type = TOK_LPAREN; return;
        case ')': out_tok->type = TOK_RPAREN; return;
        case '{': out_tok->type = TOK_LBRACE; return;
        case '}': out_tok->type = TOK_RBRACE; return;
        case '[': out_tok->type = TOK_LBRACKET; return;
        case ']': out_tok->type = TOK_RBRACKET; return;
        case ';': out_tok->type = TOK_SEMI; return;
        case ':': out_tok->type = TOK_COLON; return;
        case ',': out_tok->type = TOK_COMMA; return;
        
        case '=': {
            if (peek(lex) == '=') { advance(lex); out_tok->type = TOK_EQEQ; }
            else { out_tok->type = TOK_EQ; }
            return;
        }
        case '~': {
            if (peek(lex) == '=') { advance(lex); out_tok->type = TOK_TILDEEQ; return; }
            make_error(lex, "Unexpected character '~'", out_tok);
            return;
        }
        case '<': {
            if (peek(lex) == '=') { advance(lex); out_tok->type = TOK_LTEQ; }
            else { out_tok->type = TOK_LT; }
            return;
        }
        case '>': {
            if (peek(lex) == '=') { advance(lex); out_tok->type = TOK_GTEQ; }
            else { out_tok->type = TOK_GT; }
            return;
        }
        case '.': {
            if (peek(lex) == '.') {
                advance(lex);
                if (peek(lex) == '.') { advance(lex); out_tok->type = TOK_DOTDOTDOT; }
                else { out_tok->type = TOK_DOTDOT; }
            } else { out_tok->type = TOK_DOT; }
            return;
        }
    }
    
    make_error(lex, "Unexpected character", out_tok);
}

