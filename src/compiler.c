#include "compiler.h"
#include <string.h>

static BytecodeFunction* current_func;
static CompiledChunk* current_chunk;

static void emit_byte(uint8_t b) {
    if (current_func->code_len < MAX_CODE_SIZE) {
        current_func->code[current_func->code_len++] = b;
    }
}

static void emit_op(OpCode op) {
    emit_byte((uint8_t)op);
}

static void emit_i16(int16_t n) {
    emit_byte((uint8_t)(n & 0xFF));
    emit_byte((uint8_t)((n >> 8) & 0xFF));
}

static uint8_t add_constant(Constant* c) {
    for (uint16_t i = 0; i < current_func->const_count; i++) {
        // Simple deduplication for numbers
        if (current_func->constants[i].type == c->type) {
            if (c->type == CONST_NUMBER && current_func->constants[i].data.number == c->data.number) return (uint8_t)i;
            if (c->type == CONST_STRING && strcmp(current_func->constants[i].data.string, c->data.string) == 0) return (uint8_t)i;
        }
    }
    uint8_t idx = (uint8_t)current_func->const_count;
    current_func->constants[current_func->const_count++] = *c;
    return idx;
}

static uint8_t add_global(const char* name) {
    for (uint16_t i = 0; i < current_chunk->global_count; i++) {
        if (strcmp(current_chunk->globals[i], name) == 0) return (uint8_t)i;
    }
    uint8_t idx = (uint8_t)current_chunk->global_count;
    current_chunk->globals[current_chunk->global_count++] = ast_strdup(name);
    return idx;
}

static int resolve_local(const char* name) {
    for (int i = current_func->local_count - 1; i >= 0; i--) {
        if (strcmp(current_func->locals[i], name) == 0) return i;
    }
    return -1;
}

static void compile_expr(Expr* expr);

static void compile_expr(Expr* expr) {
    if (!expr) return;
    switch (expr->type) {
        case EXPR_NIL:    emit_op(OP_LOADNIL); break;
        case EXPR_BOOL:   emit_op(expr->data.boolean ? OP_LOADTRUE : OP_LOADFALSE); break;
        case EXPR_NUMBER: {
            Constant c; c.type = CONST_NUMBER; c.data.number = expr->data.number;
            uint8_t idx = add_constant(&c);
            emit_op(OP_LOADCONST);
            emit_byte(idx);
        } break;
        case EXPR_STRING: {
            Constant c; c.type = CONST_STRING; c.data.string = expr->data.string_val;
            uint8_t idx = add_constant(&c);
            emit_op(OP_LOADCONST);
            emit_byte(idx);
        } break;
        case EXPR_VAR: {
            int local_idx = resolve_local(expr->data.var_name);
            if (local_idx != -1) {
                emit_op(OP_GETLOCAL);
                emit_byte((uint8_t)local_idx);
            } else {
                uint8_t idx = add_global(expr->data.var_name);
                emit_op(OP_GETGLOBAL);
                emit_byte(idx);
            }
        } break;
        case EXPR_TABLE: {
            emit_op(OP_NEWTABLE);
            TableField* f = expr->data.table_fields;
            int i = 1;
            while (f) {
                emit_op(OP_DUP);
                switch (f->type) {
                    case TF_INDEX:
                        compile_expr(f->data.index.key);
                        compile_expr(f->data.index.value);
                        emit_op(OP_SETTABLE);
                        break;
                    case TF_FIELD: {
                        compile_expr(f->data.field.value);
                        Constant c; c.type = CONST_STRING; c.data.string = f->data.field.name;
                        uint8_t idx = add_constant(&c);
                        emit_op(OP_SETFIELD);
                        emit_byte(idx);
                    } break;
                    case TF_ARRAY: {
                        Constant c; c.type = CONST_NUMBER; c.data.number = i++;
                        uint8_t idx = add_constant(&c);
                        emit_op(OP_LOADCONST);
                        emit_byte(idx);
                        compile_expr(f->data.array_expr);
                        emit_op(OP_SETTABLE);
                    } break;
                }
                f = f->next;
            }
        } break;
        case EXPR_BINOP: {
            compile_expr(expr->data.binop.left);
            compile_expr(expr->data.binop.right);
            switch (expr->data.binop.op) {
                case BINOP_ADD: emit_op(OP_ADD); break;
                case BINOP_SUB: emit_op(OP_SUB); break;
                case BINOP_MUL: emit_op(OP_MUL); break;
                case BINOP_DIV: emit_op(OP_DIV); break;
                case BINOP_MOD: emit_op(OP_MOD); break;
                case BINOP_EQ:  emit_op(OP_EQ); break;
                case BINOP_NE:  emit_op(OP_NE); break;
                case BINOP_LT:  emit_op(OP_LT); break;
                case BINOP_LE:  emit_op(OP_LE); break;
                case BINOP_GT:  emit_op(OP_GT); break;
                case BINOP_GE:  emit_op(OP_GE); break;
                case BINOP_CONCAT: emit_op(OP_CONCAT); break;
                default: break;
            }
        } break;
        case EXPR_UNOP: {
            compile_expr(expr->data.unop.expr);
            switch (expr->data.unop.op) {
                case UNOP_NEG: emit_op(OP_NEG); break;
                case UNOP_NOT: emit_op(OP_NOT); break;
                case UNOP_LEN: emit_op(OP_LEN); break;
            }
        } break;
        case EXPR_CALL: {
            // Check for built-ins
            if (expr->data.call.func->type == EXPR_VAR && strcmp(expr->data.call.func->data.var_name, "print") == 0) {
                uint8_t arg_count = 0;
                ExprList* arg = expr->data.call.args;
                while (arg) {
                    compile_expr(arg->expr);
                    arg_count++;
                    arg = arg->next;
                }
                emit_op(OP_PRINT);
                emit_byte(arg_count);
                emit_op(OP_LOADNIL); // print returns nil
                return;
            }
            compile_expr(expr->data.call.func);
            uint8_t arg_count = 0;
            ExprList* arg = expr->data.call.args;
            while (arg) {
                compile_expr(arg->expr);
                arg_count++;
                arg = arg->next;
            }
            emit_op(OP_CALL);
            emit_byte(arg_count);
        } break;
        case EXPR_INDEX: {
            compile_expr(expr->data.index.base);
            compile_expr(expr->data.index.key);
            emit_op(OP_GETTABLE);
        } break;
        case EXPR_FIELD: {
            compile_expr(expr->data.field.base);
            Constant c; c.type = CONST_STRING; c.data.string = expr->data.field.field;
            uint8_t idx = add_constant(&c);
            emit_op(OP_GETFIELD);
            emit_byte(idx);
        } break;
        default: break;
    }
}

static void compile_stmt(Stmt* stmt) {
    if (!stmt) return;
    switch (stmt->type) {
        case STMT_ASSIGN: {
            ExprList* el = stmt->data.assign.exprs;
            int count = 0;
            while (el) {
                compile_expr(el->expr);
                count++;
                el = el->next;
            }
            LValueList* lv = stmt->data.assign.lvars;
            while (lv) {
                if (lv->lval.type == LVAL_VAR) {
                    int local_idx = resolve_local(lv->lval.data.var_name);
                    if (local_idx != -1) {
                        emit_op(OP_SETLOCAL);
                        emit_byte((uint8_t)local_idx);
                    } else {
                        uint8_t idx = add_global(lv->lval.data.var_name);
                        emit_op(OP_SETGLOBAL);
                        emit_byte(idx);
                    }
                } else if (lv->lval.type == LVAL_INDEX) {
                    compile_expr(lv->lval.data.index.base);
                    compile_expr(lv->lval.data.index.key);
                    emit_op(OP_ROT3);
                    emit_op(OP_SETTABLE);
                } else if (lv->lval.type == LVAL_FIELD) {
                    compile_expr(lv->lval.data.field.base);
                    Constant c; c.type = CONST_STRING; c.data.string = lv->lval.data.field.field;
                    uint8_t idx = add_constant(&c);
                    emit_op(OP_SETFIELD);
                    emit_byte(idx);
                }
                lv = lv->next;
            }
        } break;
        case STMT_CALL: {
            compile_expr(stmt->data.call);
            emit_op(OP_POP);
        } break;
        case STMT_IF: {
            compile_expr(stmt->data.if_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            uint16_t jump_pos = current_func->code_len;
            emit_i16(0); 
            
            Stmt* s = stmt->data.if_stmt.then_block->head;
            while (s) { compile_stmt(s); s = s->next; }
            
            uint16_t end_pos = current_func->code_len;
            int16_t offset = (int16_t)(end_pos - jump_pos - 2);
            current_func->code[jump_pos] = (uint8_t)(offset & 0xFF);
            current_func->code[jump_pos+1] = (uint8_t)((offset >> 8) & 0xFF);
        } break;
        case STMT_WHILE: {
            uint16_t loop_start = current_func->code_len;
            compile_expr(stmt->data.while_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            uint16_t exit_jump = current_func->code_len;
            emit_i16(0);
            
            Stmt* s = stmt->data.while_stmt.block->head;
            while (s) { compile_stmt(s); s = s->next; }
            
            emit_op(OP_JUMP);
            int16_t offset = (int16_t)(loop_start - current_func->code_len - 2);
            emit_i16(offset);
            
            uint16_t end_pos = current_func->code_len;
            int16_t exit_offset = (int16_t)(end_pos - exit_jump - 2);
            current_func->code[exit_jump] = (uint8_t)(exit_offset & 0xFF);
            current_func->code[exit_jump+1] = (uint8_t)((exit_offset >> 8) & 0xFF);
        } break;
        case STMT_RETURN: {
            ExprList* el = stmt->data.return_exprs;
            if (el) compile_expr(el->expr);
            else emit_op(OP_LOADNIL);
            emit_op(OP_RETURN);
            emit_byte(el ? 1 : 0);
        } break;
        case STMT_LOCAL: {
            IdentList* name = stmt->data.local.names;
            ExprList* val = stmt->data.local.exprs;
            while (name) {
                if (val) {
                    compile_expr(val->expr);
                    val = val->next;
                } else {
                    emit_op(OP_LOADNIL);
                }
                current_func->locals[current_func->local_count++] = ast_strdup(name->ident);
                // The value is already on top of stack, it becomes the local value.
                // In Z80 VM, locals will be accessed relative to FP.
                name = name->next;
            }
        } break;
        default: break;
    }
}

void compiler_init(void) {
}

bool compiler_compile(Chunk* ast_chunk, CompiledChunk* out_chunk) {
    current_chunk = out_chunk;
    current_chunk->func_count = 0;
    current_chunk->global_count = 0;
    
    current_func = &current_chunk->main;
    current_func->name = "main";
    current_func->code_len = 0;
    current_func->const_count = 0;
    current_func->local_count = 0;
    
    if (!ast_chunk || !ast_chunk->block) return false;
    
    Stmt* curr = ast_chunk->block->head;
    while (curr) {
        compile_stmt(curr);
        curr = curr->next;
    }
    
    emit_op(OP_HALT);
    return true;
}
