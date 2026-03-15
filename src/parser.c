#include "parser.h"

// Will be implemented in the next step.
// For now, this is a placeholder that compiles so we can verify the structure
void parser_init(Parser* p, Lexer* lex) {
    p->lex = lex;
    p->has_error = false;
    p->error_msg[0] = '\0';
    lexer_next_token(lex, &p->curr);
    lexer_next_token(lex, &p->next);
}

Chunk* parser_parse(Parser* p) {
    // Return empty chunk for testing
    Chunk* chunk = (Chunk*)ast_alloc(sizeof(Chunk));
    if (chunk) {
        chunk->block = (Block*)ast_alloc(sizeof(Block));
        if (chunk->block) {
            chunk->block->head = 0;
            chunk->block->tail = 0;
        }
    }
    return chunk;
}
