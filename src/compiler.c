#include "compiler.h"
#include <string.h>

static BytecodeFunction* current_func;
static CompiledChunk* current_chunk;
static const char function_slot_name[] = "__func";

static void init_bytecode_function(BytecodeFunction* func, const char* name) {
    func->name = name;
    func->code_len = 0;
    func->const_count = 0;
    func->local_count = 0;
}

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
            if (c->type == CONST_FUNCTION && current_func->constants[i].data.func_idx == c->data.func_idx) return (uint8_t)i;
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

static void compile_block(Block* block);
static void compile_expr(Expr* expr);
static void compile_stmt(Stmt* stmt);

static uint16_t compile_function_body(const char* name, const char* self_name, IdentList* params, Block* body) {
    uint16_t func_idx = current_chunk->func_count;
    BytecodeFunction* saved_func = current_func;
    BytecodeFunction* func;

    current_chunk->func_count++;
    func = &current_chunk->functions[func_idx];
    init_bytecode_function(func, name ? name : "<function>");
    current_func = func;

    current_func->locals[current_func->local_count++] = self_name ? ast_strdup(self_name) : function_slot_name;

    while (params) {
        current_func->locals[current_func->local_count++] = ast_strdup(params->ident);
        params = params->next;
    }

    compile_block(body);
    emit_op(OP_LOADNIL);
    emit_op(OP_RETURN);
    emit_byte(0);

    current_func = saved_func;
    return func_idx;
}

static void emit_function_constant(uint16_t func_idx) {
    Constant c;
    uint8_t idx;

    c.type = CONST_FUNCTION;
    c.data.func_idx = func_idx;
    idx = add_constant(&c);
    emit_op(OP_LOADCONST);
    emit_byte(idx);
}

static void emit_number_constant(int16_t value) {
    Constant c;
    uint8_t idx;

    c.type = CONST_NUMBER;
    c.data.number = value;
    idx = add_constant(&c);
    emit_op(OP_LOADCONST);
    emit_byte(idx);
}

static bool is_negative_number_literal(Expr* expr) {
    if (!expr) return false;
    if (expr->type == EXPR_NUMBER) return expr->data.number < 0;
    if (expr->type == EXPR_UNOP && expr->data.unop.op == UNOP_NEG && expr->data.unop.expr && expr->data.unop.expr->type == EXPR_NUMBER) {
        return expr->data.unop.expr->data.number != 0;
    }
    return false;
}

static void patch_relative_jump(uint16_t jump_pos, uint16_t target_pos) {
    int16_t offset = (int16_t)(target_pos - jump_pos - 2);
    current_func->code[jump_pos] = (uint8_t)(offset & 0xFF);
    current_func->code[jump_pos + 1] = (uint8_t)((offset >> 8) & 0xFF);
}

static void compile_block(Block* block) {
    uint16_t starting_local_count = current_func->local_count;
    Stmt* stmt = block ? block->head : 0;
    while (stmt) {
        compile_stmt(stmt);
        stmt = stmt->next;
    }

    while (current_func->local_count > starting_local_count) {
        emit_op(OP_POP);
        current_func->local_count--;
    }
}

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
        case EXPR_FUNCTION: {
            uint16_t func_idx = compile_function_body("<anon>", 0, expr->data.function.params, expr->data.function.body);
            emit_function_constant(func_idx);
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
                        Constant c; c.type = CONST_STRING; c.data.string = f->data.field.name;
                        uint8_t idx = add_constant(&c);
                        emit_op(OP_LOADCONST);
                        emit_byte(idx);
                        compile_expr(f->data.field.value);
                        emit_op(OP_SETTABLE);
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
            switch (expr->data.binop.op) {
                case BINOP_AND: {
                    uint16_t end_jump_pos;

                    compile_expr(expr->data.binop.left);
                    emit_op(OP_DUP);
                    emit_op(OP_JUMPIFFALSE);
                    end_jump_pos = current_func->code_len;
                    emit_i16(0);
                    emit_op(OP_POP);
                    compile_expr(expr->data.binop.right);
                    patch_relative_jump(end_jump_pos, current_func->code_len);
                    break;
                }
                case BINOP_OR: {
                    uint16_t false_jump_pos;
                    uint16_t end_jump_pos;

                    compile_expr(expr->data.binop.left);
                    emit_op(OP_DUP);
                    emit_op(OP_JUMPIFFALSE);
                    false_jump_pos = current_func->code_len;
                    emit_i16(0);
                    emit_op(OP_JUMP);
                    end_jump_pos = current_func->code_len;
                    emit_i16(0);
                    patch_relative_jump(false_jump_pos, current_func->code_len);
                    emit_op(OP_POP);
                    compile_expr(expr->data.binop.right);
                    patch_relative_jump(end_jump_pos, current_func->code_len);
                    break;
                }
                default:
                    compile_expr(expr->data.binop.left);
                    compile_expr(expr->data.binop.right);
                    break;
            }

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
                case BINOP_AND:
                case BINOP_OR:
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
            emit_op(OP_LOADCONST);
            emit_byte(idx);
            emit_op(OP_GETTABLE);
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
                    emit_op(OP_LOADCONST);
                    emit_byte(idx);
                    emit_op(OP_ROT3);
                    emit_op(OP_SETTABLE);
                }
                lv = lv->next;
            }
        } break;
        case STMT_CALL: {
            compile_expr(stmt->data.call);
            emit_op(OP_POP);
        } break;
        case STMT_IF: {
            uint16_t end_jumps[MAX_LOCALS];
            uint16_t end_jump_count = 0;
            ElseIf* elif = 0;

            compile_expr(stmt->data.if_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            uint16_t false_jump = current_func->code_len;
            emit_i16(0);

            compile_block(stmt->data.if_stmt.then_block);

            if (stmt->data.if_stmt.elseifs || stmt->data.if_stmt.else_block) {
                emit_op(OP_JUMP);
                end_jumps[end_jump_count++] = current_func->code_len;
                emit_i16(0);
            }

            patch_relative_jump(false_jump, current_func->code_len);

            elif = stmt->data.if_stmt.elseifs;
            while (elif) {
                compile_expr(elif->cond);
                emit_op(OP_JUMPIFFALSE);
                false_jump = current_func->code_len;
                emit_i16(0);

                compile_block(elif->block);

                if (elif->next || stmt->data.if_stmt.else_block) {
                    emit_op(OP_JUMP);
                    end_jumps[end_jump_count++] = current_func->code_len;
                    emit_i16(0);
                }

                patch_relative_jump(false_jump, current_func->code_len);
                elif = elif->next;
            }

            if (stmt->data.if_stmt.else_block) {
                compile_block(stmt->data.if_stmt.else_block);
            }

            while (end_jump_count > 0) {
                end_jump_count--;
                patch_relative_jump(end_jumps[end_jump_count], current_func->code_len);
            }
        } break;
        case STMT_WHILE: {
            uint16_t loop_start = current_func->code_len;
            compile_expr(stmt->data.while_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            uint16_t exit_jump = current_func->code_len;
            emit_i16(0);

            compile_block(stmt->data.while_stmt.block);
            
            emit_op(OP_JUMP);
            int16_t offset = (int16_t)(loop_start - current_func->code_len - 2);
            emit_i16(offset);

            patch_relative_jump(exit_jump, current_func->code_len);
        } break;
        case STMT_REPEAT: {
            uint16_t loop_start = current_func->code_len;

            compile_block(stmt->data.repeat_stmt.block);
            compile_expr(stmt->data.repeat_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            emit_i16((int16_t)(loop_start - current_func->code_len - 2));
        } break;
        case STMT_FOR_NUM: {
            static const char for_limit_name[] = "__for_limit";
            static const char for_step_name[] = "__for_step";
            uint16_t base_local = current_func->local_count;
            uint16_t loop_check;
            uint16_t exit_jump;
            uint8_t var_idx;
            uint8_t limit_idx;
            uint8_t step_idx;
            bool descending = is_negative_number_literal(stmt->data.for_num.step);

            compile_expr(stmt->data.for_num.start);
            current_func->locals[current_func->local_count++] = ast_strdup(stmt->data.for_num.var);
            compile_expr(stmt->data.for_num.end);
            current_func->locals[current_func->local_count++] = for_limit_name;
            if (stmt->data.for_num.step) compile_expr(stmt->data.for_num.step);
            else emit_number_constant(1);
            current_func->locals[current_func->local_count++] = for_step_name;

            var_idx = (uint8_t)base_local;
            limit_idx = (uint8_t)(base_local + 1);
            step_idx = (uint8_t)(base_local + 2);

            loop_check = current_func->code_len;

            emit_op(OP_GETLOCAL); emit_byte(var_idx);
            emit_op(OP_GETLOCAL); emit_byte(limit_idx);
            emit_op(descending ? OP_GE : OP_LE);
            emit_op(OP_JUMPIFFALSE);
            exit_jump = current_func->code_len;
            emit_i16(0);
            compile_block(stmt->data.for_num.block);

            emit_op(OP_GETLOCAL); emit_byte(var_idx);
            emit_op(OP_GETLOCAL); emit_byte(step_idx);
            emit_op(OP_ADD);
            emit_op(OP_SETLOCAL); emit_byte(var_idx);
            emit_op(OP_JUMP);
            emit_i16((int16_t)(loop_check - current_func->code_len - 2));

            patch_relative_jump(exit_jump, current_func->code_len);

            emit_op(OP_POP);
            emit_op(OP_POP);
            emit_op(OP_POP);
            current_func->local_count = base_local;
        } break;
        case STMT_FOR_IN: {
            static const char for_table_name[] = "__for_table";
            static const char for_index_name[] = "__for_index";
            IdentList* loop_var = stmt->data.for_in.vars;
            ExprList* iterable = stmt->data.for_in.exprs;
            uint16_t base_local = current_func->local_count;
            uint16_t loop_start;
            uint16_t exit_jump;
            uint8_t table_idx;
            uint8_t index_idx;
            uint8_t value_idx;

            if (!loop_var || !iterable) {
                break;
            }

            compile_expr(iterable->expr);
            current_func->locals[current_func->local_count++] = for_table_name;
            emit_number_constant(1);
            current_func->locals[current_func->local_count++] = for_index_name;
            emit_op(OP_LOADNIL);
            current_func->locals[current_func->local_count++] = ast_strdup(loop_var->ident);

            table_idx = (uint8_t)base_local;
            index_idx = (uint8_t)(base_local + 1);
            value_idx = (uint8_t)(base_local + 2);

            loop_start = current_func->code_len;
            emit_op(OP_GETLOCAL); emit_byte(table_idx);
            emit_op(OP_GETLOCAL); emit_byte(index_idx);
            emit_op(OP_GETTABLE);
            emit_op(OP_DUP);
            emit_op(OP_JUMPIFFALSE);
            exit_jump = current_func->code_len;
            emit_i16(0);
            emit_op(OP_SETLOCAL); emit_byte(value_idx);

            compile_block(stmt->data.for_in.block);

            emit_op(OP_GETLOCAL); emit_byte(index_idx);
            emit_number_constant(1);
            emit_op(OP_ADD);
            emit_op(OP_SETLOCAL); emit_byte(index_idx);
            emit_op(OP_JUMP);
            emit_i16((int16_t)(loop_start - current_func->code_len - 2));

            patch_relative_jump(exit_jump, current_func->code_len);

            emit_op(OP_POP);
            emit_op(OP_POP);
            emit_op(OP_POP);
            current_func->local_count = base_local;
        } break;
        case STMT_RETURN: {
            ExprList* el = stmt->data.return_exprs;
            if (el) compile_expr(el->expr);
            else emit_op(OP_LOADNIL);
            emit_op(OP_RETURN);
            emit_byte(el ? 1 : 0);
        } break;
        case STMT_FUNC_DEF: {
            int local_idx;
            uint8_t global_idx;
            uint16_t func_idx = compile_function_body(stmt->data.func_def.name.base, stmt->data.func_def.name.base, stmt->data.func_def.params, stmt->data.func_def.body);

            emit_function_constant(func_idx);
            local_idx = resolve_local(stmt->data.func_def.name.base);
            if (local_idx != -1) {
                emit_op(OP_SETLOCAL);
                emit_byte((uint8_t)local_idx);
            } else {
                global_idx = add_global(stmt->data.func_def.name.base);
                emit_op(OP_SETGLOBAL);
                emit_byte(global_idx);
            }
        } break;
        case STMT_LOCAL_FUNC: {
            uint16_t func_idx = compile_function_body(stmt->data.local_func.name, stmt->data.local_func.name, stmt->data.local_func.params, stmt->data.local_func.body);

            emit_function_constant(func_idx);
            current_func->locals[current_func->local_count++] = ast_strdup(stmt->data.local_func.name);
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
    init_bytecode_function(current_func, "main");
    
    if (!ast_chunk || !ast_chunk->block) return false;
    
    compile_block(ast_chunk->block);
    
    emit_op(OP_HALT);
    return true;
}
