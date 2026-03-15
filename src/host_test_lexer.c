#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"

void test_lexer(const char* input) {
    Lexer lex;
    lexer_init(&lex, input, strlen(input));
    
    printf("Lexing: '%s'\n", input);
    Token tok;
    do {
        lexer_next_token(&lex, &tok);
        if (tok.type == TOK_ERROR) {
            printf("Error at line %d: %s\n", tok.line, lex.error_msg);
            break;
        }
        printf("Line %d | %s", tok.line, token_type_to_str(tok.type));
        if (tok.type == TOK_NUMBER) printf(" | value: %d\n", tok.value.number);
        else if (tok.type == TOK_STRING) printf(" | value: '%s'\n", tok.value.string);
        else if (tok.type == TOK_IDENT) printf(" | value: '%s'\n", tok.value.ident);
        else printf("\n");
        
    } while (tok.type != TOK_EOF);
    printf("----\n");
}

int main() {
    test_lexer("x = 10 + 20");
    test_lexer("local msg = \"hello world\"\nprint(msg)");
    test_lexer("if n <= 1 then return true else return false end");
    test_lexer("0x1A - 0Xb + -5");
    return 0;
}
