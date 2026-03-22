#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lexer.h"
#include "parser.h"

void test_parser(const char* input) {
    Lexer lex;
    lexer_init(&lex, input, strlen(input));
    
    Parser p;
    parser_init(&p, &lex);
    
    ast_reset();
    Chunk* chunk = parser_parse(&p);
    
    printf("Parsing: '%s'\n", input);
    if (p.has_error) {
        printf("<input>:%u:%u: error: %s\n", (unsigned)p.error_line, (unsigned)p.error_column, p.error_msg);
    } else if (lex.has_error) {
        printf("<input>:%u:%u: error: %s\n", (unsigned)p.curr.line, (unsigned)p.curr.column, lex.error_msg);
    } else {
        printf("Success! Block head = %p\n", (void*)chunk->block->head);
    }
    printf("----\n");
}

int main() {
    test_parser("x = 10 + 20");
    test_parser("local msg = \"hello world\"\nprint(msg)");
    test_parser("if n <= 1 then return true else return false end");
    test_parser("0x1A - 0Xb + -5");
    return 0;
}
