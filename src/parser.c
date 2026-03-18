#include "parser.h"

// Forward declarations
static Block* parse_block(Parser* p);
static Stmt* parse_stmt(Parser* p);
static Expr* parse_expr(Parser* p);
static Expr* parse_prefix_expr(Parser* p);
static Expr* parse_function_expr(Parser* p);
static IdentList* parse_param_list(Parser* p);
static Stmt* parse_function_stmt(Parser* p);
static void expr_to_lvalue(Parser* p, Expr* expr, LValue* out_lv);
static ExprList* parse_args(Parser* p);

static bool check(Parser* p, TokenType t) {
    return p->curr.type == t;
}

static void advance(Parser* p) {
    p->curr = p->next;
    lexer_next_token(p->lex, &p->next);
}

static bool match(Parser* p, TokenType t) {
    if (check(p, t)) {
        advance(p);
        return true;
    }
    return false;
}

static void expect(Parser* p, TokenType t) {
    if (check(p, t)) {
        advance(p);
    } else {
        p->has_error = true;
        // In real impl, format err msg
    }
}

// Memory linked list helpers
static void block_add_stmt(Block* block, Stmt* stmt) {
    if (!block->head) {
        block->head = stmt;
        block->tail = stmt;
    } else {
        block->tail->next = stmt;
        block->tail = stmt;
    }
    stmt->next = 0;
}

static ExprList* exprlist_append(ExprList* list, Expr* expr) {
    ExprList* node = (ExprList*)ast_alloc(sizeof(ExprList));
    node->expr = expr;
    node->next = 0;
    if (!list) return node;
    ExprList* tail = list;
    while(tail->next) tail = tail->next;
    tail->next = node;
    return list;
}

static IdentList* identlist_append(IdentList* list, const char* ident) {
    IdentList* node = (IdentList*)ast_alloc(sizeof(IdentList));
    node->ident = ast_strdup(ident);
    node->next = 0;
    if (!list) return node;
    IdentList* tail = list;
    while(tail->next) tail = tail->next;
    tail->next = node;
    return list;
}

void parser_init(Parser* p, Lexer* lex) {
    p->lex = lex;
    p->has_error = false;
    p->error_msg[0] = '\0';
    lexer_next_token(lex, &p->curr);
    lexer_next_token(lex, &p->next);
}

static Expr* new_expr(ExprType type) {
    Expr* e = (Expr*)ast_alloc(sizeof(Expr));
    e->type = type;
    return e;
}

static Stmt* new_stmt(StmtType type) {
    Stmt* s = (Stmt*)ast_alloc(sizeof(Stmt));
    s->type = type;
    s->next = 0;
    return s;
}

Chunk* parser_parse(Parser* p) {
    Chunk* chunk = (Chunk*)ast_alloc(sizeof(Chunk));
    chunk->block = parse_block(p);
    if (!check(p, TOK_EOF)) p->has_error = true;
    return chunk;
}

// ... 
// Let's implement full parsing iteratively, first focus on simple expression and assignment parsing to ensure it links correctly.
static Expr* parse_primary_expr(Parser* p) {
    return parse_prefix_expr(p);
}

static Expr* parse_unary_expr(Parser* p) {
    if (match(p, TOK_NOT)) {
        Expr* e = new_expr(EXPR_UNOP); e->data.unop.op = UNOP_NOT; e->data.unop.expr = parse_unary_expr(p); return e;
    } else if (match(p, TOK_MINUS)) {
        Expr* e = new_expr(EXPR_UNOP); e->data.unop.op = UNOP_NEG; e->data.unop.expr = parse_unary_expr(p); return e;
    } else if (match(p, TOK_HASH)) {
        Expr* e = new_expr(EXPR_UNOP); e->data.unop.op = UNOP_LEN; e->data.unop.expr = parse_unary_expr(p); return e;
    }
    return parse_primary_expr(p);
}

static Expr* parse_mul_expr(Parser* p) {
    Expr* left = parse_unary_expr(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        BinOp op = check(p, TOK_STAR) ? BINOP_MUL : (check(p, TOK_SLASH) ? BINOP_DIV : BINOP_MOD);
        advance(p);
        Expr* right = parse_unary_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = op; bin->data.binop.right = right;
        left = bin;
    }
    return left;
}

static Expr* parse_add_expr(Parser* p) {
    Expr* left = parse_mul_expr(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        BinOp op = check(p, TOK_PLUS) ? BINOP_ADD : BINOP_SUB;
        advance(p);
        Expr* right = parse_mul_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = op; bin->data.binop.right = right;
        left = bin;
    }
    return left;
}

static Expr* parse_concat_expr(Parser* p) {
    Expr* left = parse_add_expr(p);
    if (match(p, TOK_DOTDOT)) {
        Expr* right = parse_concat_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = BINOP_CONCAT; bin->data.binop.right = right;
        return bin;
    }
    return left;
}

static Expr* parse_comparison_expr(Parser* p) {
    Expr* left = parse_concat_expr(p);
    while (check(p, TOK_LT) || check(p, TOK_GT) || check(p, TOK_LTEQ) || check(p, TOK_GTEQ) || check(p, TOK_EQEQ) || check(p, TOK_TILDEEQ)) {
        BinOp op = BINOP_EQ;
        if (check(p, TOK_LT)) op = BINOP_LT; else if (check(p, TOK_GT)) op = BINOP_GT;
        else if (check(p, TOK_LTEQ)) op = BINOP_LE; else if (check(p, TOK_GTEQ)) op = BINOP_GE;
        else if (check(p, TOK_EQEQ)) op = BINOP_EQ; else if (check(p, TOK_TILDEEQ)) op = BINOP_NE;
        advance(p);
        Expr* right = parse_concat_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = op; bin->data.binop.right = right;
        left = bin;
    }
    return left;
}

static Expr* parse_and_expr(Parser* p) {
    Expr* left = parse_comparison_expr(p);
    while (match(p, TOK_AND)) {
        Expr* right = parse_comparison_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = BINOP_AND; bin->data.binop.right = right;
        left = bin;
    }
    return left;
}

static Expr* parse_or_expr(Parser* p) {
    Expr* left = parse_and_expr(p);
    while (match(p, TOK_OR)) {
        Expr* right = parse_and_expr(p);
        Expr* bin = new_expr(EXPR_BINOP);
        bin->data.binop.left = left; bin->data.binop.op = BINOP_OR; bin->data.binop.right = right;
        left = bin;
    }
    return left;
}

static Expr* parse_expr(Parser* p) {
    return parse_or_expr(p);
}

static Expr* parse_table(Parser* p) {
    expect(p, TOK_LBRACE);
    Expr* t = new_expr(EXPR_TABLE);
    t->data.table_fields = 0;
    TableField* tail = 0;
    
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        TableField* tf = (TableField*)ast_alloc(sizeof(TableField));
        tf->next = 0;
        
        if (match(p, TOK_LBRACKET)) {
            tf->type = TF_INDEX;
            tf->data.index.key = parse_expr(p);
            expect(p, TOK_RBRACKET);
            expect(p, TOK_EQ);
            tf->data.index.value = parse_expr(p);
        } else if (check(p, TOK_IDENT) && p->next.type == TOK_EQ) {
            tf->type = TF_FIELD;
            tf->data.field.name = ast_strdup(p->curr.value.ident);
            advance(p); // ident
            advance(p); // =
            tf->data.field.value = parse_expr(p);
        } else {
            tf->type = TF_ARRAY;
            tf->data.array_expr = parse_expr(p);
        }
        
        if (!t->data.table_fields) t->data.table_fields = tf;
        else tail->next = tf;
        tail = tf;
        
        if (!match(p, TOK_COMMA) && !match(p, TOK_SEMI)) break;
    }
    expect(p, TOK_RBRACE);
    return t;
}

static IdentList* parse_param_list(Parser* p) {
    IdentList* params = 0;

    if (check(p, TOK_IDENT)) {
        params = identlist_append(params, p->curr.value.ident);
        advance(p);
        while (match(p, TOK_COMMA)) {
            if (check(p, TOK_IDENT)) {
                params = identlist_append(params, p->curr.value.ident);
                advance(p);
            } else {
                p->has_error = true;
                break;
            }
        }
    }

    return params;
}

static Expr* parse_function_expr(Parser* p) {
    Expr* expr = new_expr(EXPR_FUNCTION);

    expect(p, TOK_FUNCTION);
    expect(p, TOK_LPAREN);
    expr->data.function.params = parse_param_list(p);
    expect(p, TOK_RPAREN);
    expr->data.function.body = parse_block(p);
    expect(p, TOK_END);
    return expr;
}

static Expr* parse_prefix_expr(Parser* p) {
    Expr* expr = 0;
    if (check(p, TOK_FUNCTION)) {
        expr = parse_function_expr(p);
    } else if (match(p, TOK_LPAREN)) {
        expr = parse_expr(p);
        expect(p, TOK_RPAREN);
    } else if (check(p, TOK_IDENT)) {
        expr = new_expr(EXPR_VAR);
        expr->data.var_name = ast_strdup(p->curr.value.ident);
        advance(p);
    } else if (match(p, TOK_NIL)) {
        expr = new_expr(EXPR_NIL);
    } else if (match(p, TOK_TRUE)) {
        expr = new_expr(EXPR_BOOL); expr->data.boolean = true;
    } else if (match(p, TOK_FALSE)) {
        expr = new_expr(EXPR_BOOL); expr->data.boolean = false;
    } else if (check(p, TOK_NUMBER)) {
        expr = new_expr(EXPR_NUMBER); expr->data.number = p->curr.value.number; advance(p);
    } else if (check(p, TOK_STRING)) {
        expr = new_expr(EXPR_STRING); expr->data.string_val = ast_strdup(p->curr.value.string); advance(p);
    } else if (check(p, TOK_LBRACE)) {
        expr = parse_table(p);
    } else {
        p->has_error = true;
        return 0;
    }

    // Handle suffixes
    while (1) {
        if (check(p, TOK_LPAREN)) {
            advance(p);
            ExprList* args = parse_args(p);
            expect(p, TOK_RPAREN);
            Expr* call = new_expr(EXPR_CALL);
            call->data.call.func = expr;
            call->data.call.args = args;
            expr = call;
        } else if (match(p, TOK_LBRACKET)) {
            Expr* key = parse_expr(p);
            expect(p, TOK_RBRACKET);
            Expr* idx = new_expr(EXPR_INDEX);
            idx->data.index.base = expr; idx->data.index.key = key;
            expr = idx;
        } else if (match(p, TOK_DOT)) {
            if (check(p, TOK_IDENT)) {
                Expr* fld = new_expr(EXPR_FIELD);
                fld->data.field.base = expr; fld->data.field.field = ast_strdup(p->curr.value.ident);
                advance(p);
                expr = fld;
            } else p->has_error = true;
        } else if (match(p, TOK_COLON)) {
            if (check(p, TOK_IDENT)) {
                Expr* mcall = new_expr(EXPR_METHOD_CALL);
                mcall->data.method_call.obj = expr; mcall->data.method_call.method = ast_strdup(p->curr.value.ident);
                advance(p);
                expect(p, TOK_LPAREN);
                mcall->data.method_call.args = parse_args(p);
                expect(p, TOK_RPAREN);
                expr = mcall;
            } else p->has_error = true;
        } else if (check(p, TOK_STRING)) {
            ExprList* args = exprlist_append(0, new_expr(EXPR_STRING));
            args->expr->data.string_val = ast_strdup(p->curr.value.string); advance(p);
            Expr* call = new_expr(EXPR_CALL); call->data.call.func = expr; call->data.call.args = args; expr = call;
        } else if (check(p, TOK_LBRACE)) {
            ExprList* args = exprlist_append(0, parse_table(p));
            Expr* call = new_expr(EXPR_CALL); call->data.call.func = expr; call->data.call.args = args; expr = call;
        } else {
            break;
        }
    }
    return expr;
}

static ExprList* parse_args(Parser* p) {
    ExprList* args = 0;
    if (!check(p, TOK_RPAREN)) {
        args = exprlist_append(args, parse_expr(p));
        while (match(p, TOK_COMMA)) {
            args = exprlist_append(args, parse_expr(p));
        }
    }
    return args;
}

static void expr_to_lvalue(Parser* p, Expr* expr, LValue* out_lv) {
    if (!expr) { 
        out_lv->type = LVAL_VAR; 
        out_lv->data.var_name = "error"; 
        return; 
    }
    if (expr->type == EXPR_VAR) {
        out_lv->type = LVAL_VAR; 
        out_lv->data.var_name = expr->data.var_name;
    } else if (expr->type == EXPR_INDEX) {
        out_lv->type = LVAL_INDEX; 
        out_lv->data.index.base = expr->data.index.base; 
        out_lv->data.index.key = expr->data.index.key;
    } else if (expr->type == EXPR_FIELD) {
        out_lv->type = LVAL_FIELD; 
        out_lv->data.field.base = expr->data.field.base; 
        out_lv->data.field.field = expr->data.field.field;
    } else {
        p->has_error = true;
        out_lv->type = LVAL_VAR; 
        out_lv->data.var_name = "error";
    }
}

static Stmt* parse_assignment_or_call(Parser* p) {
    Expr* expr = parse_prefix_expr(p);
    
    if (check(p, TOK_EQ) || check(p, TOK_COMMA)) {
        LValueList* lvars = (LValueList*)ast_alloc(sizeof(LValueList));
        expr_to_lvalue(p, expr, &lvars->lval);
        lvars->next = 0;
        LValueList* tail = lvars;
        while (match(p, TOK_COMMA)) {
            LValueList* nn = (LValueList*)ast_alloc(sizeof(LValueList));
            expr_to_lvalue(p, parse_prefix_expr(p), &nn->lval);
            nn->next = 0;
            tail->next = nn;
            tail = nn;
        }
        expect(p, TOK_EQ);
        
        ExprList* exprs = exprlist_append(0, parse_expr(p));
        while (match(p, TOK_COMMA)) exprs = exprlist_append(exprs, parse_expr(p));
        
        Stmt* s = new_stmt(STMT_ASSIGN);
        s->data.assign.lvars = lvars; s->data.assign.exprs = exprs;
        return s;
    } else {
        if (expr && (expr->type == EXPR_CALL || expr->type == EXPR_METHOD_CALL)) {
            Stmt* s = new_stmt(STMT_CALL); s->data.call = expr; return s;
        } else {
            p->has_error = true;
            int i = 0; const char* msg = "Expected assignment or function call";
            while(msg[i]) { p->error_msg[i] = msg[i]; i++; }
            p->error_msg[i] = '\0';
            return new_stmt(STMT_CALL);
        }
    }
}

static Stmt* parse_local(Parser* p) {
    expect(p, TOK_LOCAL);
    Stmt* s;
    if (match(p, TOK_FUNCTION)) {
        s = new_stmt(STMT_LOCAL_FUNC);
        if (check(p, TOK_IDENT)) { s->data.local_func.name = ast_strdup(p->curr.value.ident); advance(p); }
        expect(p, TOK_LPAREN);
        IdentList* params = parse_param_list(p);
        expect(p, TOK_RPAREN);
        s->data.local_func.params = params;
        s->data.local_func.body = parse_block(p);
        expect(p, TOK_END);
    } else {
        s = new_stmt(STMT_LOCAL);
        IdentList* names = 0;
        if (check(p, TOK_IDENT)) { names = identlist_append(names, p->curr.value.ident); advance(p); }
        while (match(p, TOK_COMMA)) {
            if (check(p, TOK_IDENT)) { names = identlist_append(names, p->curr.value.ident); advance(p); }
        }
        s->data.local.names = names;
        ExprList* exprs = 0;
        if (match(p, TOK_EQ)) {
            exprs = exprlist_append(exprs, parse_expr(p));
            while (match(p, TOK_COMMA)) exprs = exprlist_append(exprs, parse_expr(p));
        }
        s->data.local.exprs = exprs;
    }
    return s;
}

static Stmt* parse_function_stmt(Parser* p) {
    Stmt* s = new_stmt(STMT_FUNC_DEF);

    expect(p, TOK_FUNCTION);
    s->data.func_def.name.base = 0;
    s->data.func_def.name.fields = 0;
    s->data.func_def.name.method = 0;

    if (check(p, TOK_IDENT)) {
        s->data.func_def.name.base = ast_strdup(p->curr.value.ident);
        advance(p);
    } else {
        p->has_error = true;
    }

    expect(p, TOK_LPAREN);
    s->data.func_def.params = parse_param_list(p);
    expect(p, TOK_RPAREN);
    s->data.func_def.body = parse_block(p);
    expect(p, TOK_END);
    return s;
}

static Stmt* parse_if(Parser* p) {
    expect(p, TOK_IF);
    Stmt* s = new_stmt(STMT_IF);
    s->data.if_stmt.cond = parse_expr(p);
    expect(p, TOK_THEN);
    s->data.if_stmt.then_block = parse_block(p);
    
    ElseIf* elseifs = 0; ElseIf* elif_tail = 0;
    while (match(p, TOK_ELSEIF)) {
        ElseIf* elif = (ElseIf*)ast_alloc(sizeof(ElseIf));
        elif->cond = parse_expr(p);
        expect(p, TOK_THEN);
        elif->block = parse_block(p);
        elif->next = 0;
        if (!elseifs) elseifs = elif; else elif_tail->next = elif;
        elif_tail = elif;
    }
    s->data.if_stmt.elseifs = elseifs;
    
    if (match(p, TOK_ELSE)) s->data.if_stmt.else_block = parse_block(p);
    else s->data.if_stmt.else_block = 0;
    expect(p, TOK_END);
    return s;
}

static Stmt* parse_while(Parser* p) {
    expect(p, TOK_WHILE);
    Stmt* s = new_stmt(STMT_WHILE);
    s->data.while_stmt.cond = parse_expr(p);
    expect(p, TOK_DO);
    s->data.while_stmt.block = parse_block(p);
    expect(p, TOK_END);
    return s;
}

static Stmt* parse_repeat(Parser* p) {
    expect(p, TOK_REPEAT);
    Stmt* s = new_stmt(STMT_REPEAT);
    s->data.repeat_stmt.block = parse_block(p);
    expect(p, TOK_UNTIL);
    s->data.repeat_stmt.cond = parse_expr(p);
    return s;
}

static Stmt* parse_for(Parser* p) {
    expect(p, TOK_FOR);

    if (check(p, TOK_IDENT) && p->next.type == TOK_EQ) {
        Stmt* s = new_stmt(STMT_FOR_NUM);
        s->data.for_num.var = ast_strdup(p->curr.value.ident);
        advance(p);
        expect(p, TOK_EQ);
        s->data.for_num.start = parse_expr(p);
        expect(p, TOK_COMMA);
        s->data.for_num.end = parse_expr(p);
        if (match(p, TOK_COMMA)) s->data.for_num.step = parse_expr(p);
        else s->data.for_num.step = 0;
        expect(p, TOK_DO);
        s->data.for_num.block = parse_block(p);
        expect(p, TOK_END);
        return s;
    }

    if (check(p, TOK_IDENT)) {
        Stmt* s = new_stmt(STMT_FOR_IN);
        IdentList* vars = 0;
        ExprList* exprs = 0;

        vars = identlist_append(vars, p->curr.value.ident);
        advance(p);
        expect(p, TOK_IN);
        exprs = exprlist_append(exprs, parse_expr(p));
        expect(p, TOK_DO);
        s->data.for_in.vars = vars;
        s->data.for_in.exprs = exprs;
        s->data.for_in.block = parse_block(p);
        expect(p, TOK_END);
        return s;
    }

    p->has_error = true;
    return new_stmt(STMT_DO);
}

static Stmt* parse_stmt(Parser* p) {
    if (check(p, TOK_FUNCTION)) return parse_function_stmt(p);
    if (check(p, TOK_IF)) return parse_if(p);
    if (check(p, TOK_WHILE)) return parse_while(p);
    if (check(p, TOK_REPEAT)) return parse_repeat(p);
    if (check(p, TOK_FOR)) return parse_for(p);
    // TODO: function, break support in loops
    if (check(p, TOK_LOCAL)) return parse_local(p);
    if (match(p, TOK_BREAK)) return new_stmt(STMT_BREAK);
    if (match(p, TOK_DO)) {
        Stmt* s = new_stmt(STMT_DO);
        s->data.do_block = parse_block(p);
        expect(p, TOK_END);
        return s;
    }
    if (match(p, TOK_RETURN)) {
        Stmt* s = new_stmt(STMT_RETURN);
        ExprList* exprs = 0;
        if (!check(p, TOK_END) && !check(p, TOK_ELSE) && !check(p, TOK_ELSEIF) && !check(p, TOK_UNTIL) && !check(p, TOK_EOF) && !check(p, TOK_SEMI)) {
            exprs = exprlist_append(exprs, parse_expr(p));
            while (match(p, TOK_COMMA)) exprs = exprlist_append(exprs, parse_expr(p));
        }
        s->data.return_exprs = exprs;
        return s;
    }
    return parse_assignment_or_call(p);
}

static Block* parse_block(Parser* p) {
    Block* block = (Block*)ast_alloc(sizeof(Block));
    block->head = 0; block->tail = 0;
    while (1) {
        while (match(p, TOK_SEMI));
        if (check(p, TOK_END) || check(p, TOK_ELSE) || check(p, TOK_ELSEIF) || check(p, TOK_UNTIL) || check(p, TOK_EOF)) break;
        if (check(p, TOK_RETURN)) {
            block_add_stmt(block, parse_stmt(p));
            break;
        }
        block_add_stmt(block, parse_stmt(p));
        if (p->has_error) {
            break;
        }
    }
    return block;
}
