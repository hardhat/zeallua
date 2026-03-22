#include "compiler.h"
#include <string.h>

typedef struct ScopeContext {
    BytecodeFunction* func;
    uint16_t active_local_count;
    struct ScopeContext* enclosing;
} ScopeContext;

static CompiledChunk* current_chunk;
static ScopeContext* current_scope;
static const char function_slot_name[] = "__func";
static const Stmt* current_stmt_node;
static const Expr* current_expr_node;

static void compiler_copy_msg(char* dest, uint16_t capacity, const char* src) {
    uint16_t i = 0;
    if (capacity == 0) return;
    while (src[i] != '\0' && i < (uint16_t)(capacity - 1)) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static bool compiler_fail(uint16_t line, uint16_t column, const char* msg) {
    if (!current_chunk) return false;
    if (!current_chunk->has_error) {
        current_chunk->has_error = true;
        current_chunk->error_line = line;
        current_chunk->error_column = column;
        compiler_copy_msg(current_chunk->error_msg, sizeof(current_chunk->error_msg), msg);
    }
    return false;
}

static bool compiler_fail_at_stmt(const Stmt* stmt, const char* msg);
static bool compiler_fail_at_expr(const Expr* expr, const char* msg);

static BytecodeFunction* current_func(void) {
    return current_scope->func;
}

static void init_bytecode_function(BytecodeFunction* func, const char* name) {
    uint16_t i;

    func->name = name;
    func->code_len = 0;
    func->const_count = 0;
    func->local_count = 0;
    func->param_count = 0;
    func->initial_local_count = 0;
    func->env_local_count = 0;
    func->upvalue_count = 0;

    for (i = 0; i < MAX_LOCALS; i++) {
        func->locals[i] = 0;
        func->local_is_captured[i] = false;
        func->local_env_slot[i] = 0xFF;
    }
}

static void emit_byte(uint8_t b) {
    BytecodeFunction* func = current_func();
    if (current_chunk->has_error) return;
    if (func->code_len < MAX_CODE_SIZE) {
        func->code[func->code_len++] = b;
    } else if (current_expr_node) {
        compiler_fail_at_expr(current_expr_node, "Bytecode too large");
    } else {
        compiler_fail_at_stmt(current_stmt_node, "Bytecode too large");
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
    BytecodeFunction* func = current_func();
    uint16_t i;

    if (current_chunk->has_error) return 0;

    for (i = 0; i < func->const_count; i++) {
        if (func->constants[i].type == c->type) {
            if (c->type == CONST_NUMBER && func->constants[i].data.number == c->data.number) return (uint8_t)i;
            if (c->type == CONST_STRING && strcmp(func->constants[i].data.string, c->data.string) == 0) return (uint8_t)i;
            if (c->type == CONST_FUNCTION && func->constants[i].data.func_idx == c->data.func_idx) return (uint8_t)i;
        }
    }

    if (func->const_count >= MAX_CONSTANTS) {
        if (current_expr_node) {
            compiler_fail_at_expr(current_expr_node, "Too many constants");
        } else {
            compiler_fail_at_stmt(current_stmt_node, "Too many constants");
        }
        return 0;
    }

    func->constants[func->const_count] = *c;
    return (uint8_t)func->const_count++;
}

static uint8_t add_global(const char* name) {
    uint16_t i;

    if (current_chunk->has_error) return 0;

    for (i = 0; i < current_chunk->global_count; i++) {
        if (strcmp(current_chunk->globals[i], name) == 0) return (uint8_t)i;
    }

    if (current_chunk->global_count >= MAX_GLOBALS) {
        if (current_expr_node) {
            compiler_fail_at_expr(current_expr_node, "Too many globals");
        } else {
            compiler_fail_at_stmt(current_stmt_node, "Too many globals");
        }
        return 0;
    }

    current_chunk->globals[current_chunk->global_count] = ast_strdup(name);
    return (uint8_t)current_chunk->global_count++;
}

static int resolve_local_scope(ScopeContext* scope, const char* name) {
    int i;

    for (i = (int)scope->active_local_count - 1; i >= 0; i--) {
        if (scope->func->locals[i] && strcmp(scope->func->locals[i], name) == 0) return i;
    }

    return -1;
}

static int add_upvalue(BytecodeFunction* func, bool from_local, uint8_t index) {
    uint8_t i;

    for (i = 0; i < func->upvalue_count; i++) {
        if (func->upvalues[i].from_local == from_local && func->upvalues[i].index == index) return i;
    }

    func->upvalues[func->upvalue_count].from_local = from_local;
    func->upvalues[func->upvalue_count].index = index;
    return func->upvalue_count++;
}

static int resolve_upvalue_scope(ScopeContext* scope, const char* name) {
    int local_idx;
    int upvalue_idx;

    if (!scope->enclosing) return -1;

    local_idx = resolve_local_scope(scope->enclosing, name);
    if (local_idx != -1) {
        scope->enclosing->func->local_is_captured[local_idx] = true;
        if (scope->enclosing->func->local_env_slot[local_idx] != 0xFF) {
            return add_upvalue(scope->func, true, scope->enclosing->func->local_env_slot[local_idx]);
        }
        return add_upvalue(scope->func, true, (uint8_t)local_idx);
    }

    upvalue_idx = resolve_upvalue_scope(scope->enclosing, name);
    if (upvalue_idx != -1) {
        return add_upvalue(scope->func, false, (uint8_t)upvalue_idx);
    }

    return -1;
}

static void scope_add_local(ScopeContext* scope, const char* name) {
    uint16_t slot = scope->active_local_count;
    scope->func->locals[slot] = name;
    scope->active_local_count++;
    if (scope->active_local_count > scope->func->local_count) {
        scope->func->local_count = scope->active_local_count;
    }
}

static void analyze_block(ScopeContext* scope, Block* block);
static void analyze_expr(ScopeContext* scope, Expr* expr);
static void analyze_stmt(ScopeContext* scope, Stmt* stmt);

static void analyze_function_body(BytecodeFunction* func, ScopeContext* enclosing_scope, const char* self_name, bool include_function_slot, IdentList* params, Block* body) {
    ScopeContext scope;
    uint16_t i;

    scope.func = func;
    scope.active_local_count = 0;
    scope.enclosing = enclosing_scope;

    if (include_function_slot) {
        scope_add_local(&scope, self_name ? ast_strdup(self_name) : function_slot_name);
    }
    while (params) {
        scope_add_local(&scope, ast_strdup(params->ident));
        func->param_count++;
        params = params->next;
    }

    func->initial_local_count = (uint8_t)scope.active_local_count;

    analyze_block(&scope, body);

    func->env_local_count = 0;
    for (i = 0; i < func->local_count; i++) {
        if (func->local_is_captured[i]) {
            func->local_env_slot[i] = func->env_local_count++;
        }
    }
}

static void analyze_block(ScopeContext* scope, Block* block) {
    uint16_t starting_local_count = scope->active_local_count;
    Stmt* stmt = block ? block->head : 0;

    while (stmt) {
        analyze_stmt(scope, stmt);
        stmt = stmt->next;
    }

    scope->active_local_count = starting_local_count;
}

static void analyze_expr(ScopeContext* scope, Expr* expr) {
    ExprList* arg;
    TableField* field;

    if (!expr) return;

    switch (expr->type) {
        case EXPR_VAR:
            if (resolve_local_scope(scope, expr->data.var_name) == -1) {
                resolve_upvalue_scope(scope, expr->data.var_name);
            }
            break;
        case EXPR_FUNCTION: {
            BytecodeFunction nested;
            init_bytecode_function(&nested, "<analysis>");
            analyze_function_body(&nested, scope, 0, true, expr->data.function.params, expr->data.function.body);
        } break;
        case EXPR_BINOP:
            analyze_expr(scope, expr->data.binop.left);
            analyze_expr(scope, expr->data.binop.right);
            break;
        case EXPR_UNOP:
            analyze_expr(scope, expr->data.unop.expr);
            break;
        case EXPR_CALL:
            analyze_expr(scope, expr->data.call.func);
            arg = expr->data.call.args;
            while (arg) {
                analyze_expr(scope, arg->expr);
                arg = arg->next;
            }
            break;
        case EXPR_METHOD_CALL:
            analyze_expr(scope, expr->data.method_call.obj);
            arg = expr->data.method_call.args;
            while (arg) {
                analyze_expr(scope, arg->expr);
                arg = arg->next;
            }
            break;
        case EXPR_INDEX:
            analyze_expr(scope, expr->data.index.base);
            analyze_expr(scope, expr->data.index.key);
            break;
        case EXPR_FIELD:
            analyze_expr(scope, expr->data.field.base);
            break;
        case EXPR_TABLE:
            field = expr->data.table_fields;
            while (field) {
                if (field->type == TF_INDEX) {
                    analyze_expr(scope, field->data.index.key);
                    analyze_expr(scope, field->data.index.value);
                } else if (field->type == TF_FIELD) {
                    analyze_expr(scope, field->data.field.value);
                } else {
                    analyze_expr(scope, field->data.array_expr);
                }
                field = field->next;
            }
            break;
        default:
            break;
    }
}

static void analyze_lvalue_base(ScopeContext* scope, LValue* lval) {
    if (lval->type == LVAL_INDEX) {
        analyze_expr(scope, lval->data.index.base);
        analyze_expr(scope, lval->data.index.key);
    } else if (lval->type == LVAL_FIELD) {
        analyze_expr(scope, lval->data.field.base);
    }
}

static void analyze_stmt(ScopeContext* scope, Stmt* stmt) {
    ExprList* exprs;
    IdentList* names;
    LValueList* lvals;
    ElseIf* elif;

    if (!stmt) return;

    switch (stmt->type) {
        case STMT_ASSIGN:
            exprs = stmt->data.assign.exprs;
            while (exprs) {
                analyze_expr(scope, exprs->expr);
                exprs = exprs->next;
            }
            lvals = stmt->data.assign.lvars;
            while (lvals) {
                if (lvals->lval.type == LVAL_VAR) {
                    if (resolve_local_scope(scope, lvals->lval.data.var_name) == -1) {
                        resolve_upvalue_scope(scope, lvals->lval.data.var_name);
                    }
                } else {
                    analyze_lvalue_base(scope, &lvals->lval);
                }
                lvals = lvals->next;
            }
            break;
        case STMT_LOCAL:
            exprs = stmt->data.local.exprs;
            while (exprs) {
                analyze_expr(scope, exprs->expr);
                exprs = exprs->next;
            }
            names = stmt->data.local.names;
            while (names) {
                scope_add_local(scope, ast_strdup(names->ident));
                names = names->next;
            }
            break;
        case STMT_CALL:
            analyze_expr(scope, stmt->data.call);
            break;
        case STMT_IF:
            analyze_expr(scope, stmt->data.if_stmt.cond);
            analyze_block(scope, stmt->data.if_stmt.then_block);
            elif = stmt->data.if_stmt.elseifs;
            while (elif) {
                analyze_expr(scope, elif->cond);
                analyze_block(scope, elif->block);
                elif = elif->next;
            }
            analyze_block(scope, stmt->data.if_stmt.else_block);
            break;
        case STMT_WHILE:
            analyze_expr(scope, stmt->data.while_stmt.cond);
            analyze_block(scope, stmt->data.while_stmt.block);
            break;
        case STMT_REPEAT:
            analyze_block(scope, stmt->data.repeat_stmt.block);
            analyze_expr(scope, stmt->data.repeat_stmt.cond);
            break;
        case STMT_FOR_NUM:
            analyze_expr(scope, stmt->data.for_num.start);
            analyze_expr(scope, stmt->data.for_num.end);
            if (stmt->data.for_num.step) analyze_expr(scope, stmt->data.for_num.step);
            scope_add_local(scope, ast_strdup(stmt->data.for_num.var));
            scope_add_local(scope, "__for_limit");
            scope_add_local(scope, "__for_step");
            analyze_block(scope, stmt->data.for_num.block);
            scope->active_local_count -= 3;
            break;
        case STMT_FOR_IN:
            exprs = stmt->data.for_in.exprs;
            while (exprs) {
                analyze_expr(scope, exprs->expr);
                exprs = exprs->next;
            }
            scope_add_local(scope, "__for_table");
            scope_add_local(scope, "__for_index");
            if (stmt->data.for_in.vars) {
                scope_add_local(scope, ast_strdup(stmt->data.for_in.vars->ident));
            }
            analyze_block(scope, stmt->data.for_in.block);
            scope->active_local_count -= 3;
            break;
        case STMT_FUNC_DEF: {
            BytecodeFunction nested;
            init_bytecode_function(&nested, stmt->data.func_def.name.base);
            analyze_function_body(&nested, scope, stmt->data.func_def.name.base, true, stmt->data.func_def.params, stmt->data.func_def.body);
            if (resolve_local_scope(scope, stmt->data.func_def.name.base) == -1) {
                resolve_upvalue_scope(scope, stmt->data.func_def.name.base);
            }
        } break;
        case STMT_LOCAL_FUNC: {
            BytecodeFunction nested;
            init_bytecode_function(&nested, stmt->data.local_func.name);
            analyze_function_body(&nested, scope, stmt->data.local_func.name, true, stmt->data.local_func.params, stmt->data.local_func.body);
            scope_add_local(scope, ast_strdup(stmt->data.local_func.name));
        } break;
        case STMT_RETURN:
            exprs = stmt->data.return_exprs;
            while (exprs) {
                analyze_expr(scope, exprs->expr);
                exprs = exprs->next;
            }
            break;
        case STMT_DO:
            analyze_block(scope, stmt->data.do_block);
            break;
        default:
            break;
    }
}

static void patch_relative_jump(uint16_t jump_pos, uint16_t target_pos) {
    int16_t offset = (int16_t)(target_pos - jump_pos - 2);
    current_func()->code[jump_pos] = (uint8_t)(offset & 0xFF);
    current_func()->code[jump_pos + 1] = (uint8_t)((offset >> 8) & 0xFF);
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

static void emit_function_value(uint16_t func_idx) {
    Constant c;
    uint8_t idx;

    c.type = CONST_FUNCTION;
    c.data.func_idx = func_idx;
    idx = add_constant(&c);
    if (current_chunk->functions[func_idx].upvalue_count == 0) {
        emit_op(OP_LOADCONST);
        emit_byte(idx);
    } else {
        emit_op(OP_CLOSURE);
        emit_byte(idx);
    }
}

static void emit_capture_prologue(BytecodeFunction* func) {
    uint8_t i;

    for (i = 0; i < func->initial_local_count; i++) {
        if (func->local_is_captured[i]) {
            emit_op(OP_GETLOCAL);
            emit_byte(i);
            emit_op(OP_SETCAPTURED);
            emit_byte(func->local_env_slot[i]);
        }
    }
}

static bool is_negative_number_literal(Expr* expr) {
    if (!expr) return false;
    if (expr->type == EXPR_NUMBER) return expr->data.number < 0;
    if (expr->type == EXPR_UNOP && expr->data.unop.op == UNOP_NEG && expr->data.unop.expr && expr->data.unop.expr->type == EXPR_NUMBER) {
        return expr->data.unop.expr->data.number != 0;
    }
    return false;
}

static void compile_block(Block* block);
static void compile_expr(Expr* expr);
static void compile_stmt(Stmt* stmt);

static uint16_t compile_function_body(const char* name, const char* self_name, IdentList* params, Block* body) {
    if (current_chunk->func_count >= MAX_FUNCTIONS) {
        if (current_expr_node) {
            compiler_fail_at_expr(current_expr_node, "Too many functions");
        } else {
            compiler_fail_at_stmt(current_stmt_node, "Too many functions");
        }
        return 0;
    }

    uint16_t func_idx = current_chunk->func_count++;
    BytecodeFunction* func = &current_chunk->functions[func_idx];
    ScopeContext scope;
    ScopeContext* saved_scope = current_scope;

    init_bytecode_function(func, name ? name : "<function>");
    analyze_function_body(func, current_scope, self_name, true, params, body);

    scope.func = func;
    scope.active_local_count = func->initial_local_count;
    scope.enclosing = current_scope;
    current_scope = &scope;

    emit_capture_prologue(func);
    compile_block(body);
    emit_op(OP_LOADNIL);
    emit_op(OP_RETURN);
    emit_byte(0);

    current_scope = saved_scope;
    return func_idx;
}

static void emit_load_variable(const char* name) {
    int local_idx = resolve_local_scope(current_scope, name);
    int upvalue_idx;

    if (local_idx != -1) {
        if (current_func()->local_is_captured[local_idx]) {
            emit_op(OP_GETCAPTURED);
            emit_byte(current_func()->local_env_slot[local_idx]);
        } else {
            emit_op(OP_GETLOCAL);
            emit_byte((uint8_t)local_idx);
        }
        return;
    }

    upvalue_idx = resolve_upvalue_scope(current_scope, name);
    if (upvalue_idx != -1) {
        emit_op(OP_GETUPVAL);
        emit_byte((uint8_t)upvalue_idx);
        return;
    }

    emit_op(OP_GETGLOBAL);
    emit_byte(add_global(name));
}

static void emit_store_variable(const char* name) {
    int local_idx = resolve_local_scope(current_scope, name);
    int upvalue_idx;

    if (local_idx != -1) {
        if (current_func()->local_is_captured[local_idx]) {
            emit_op(OP_SETCAPTURED);
            emit_byte(current_func()->local_env_slot[local_idx]);
        } else {
            emit_op(OP_SETLOCAL);
            emit_byte((uint8_t)local_idx);
        }
        return;
    }

    upvalue_idx = resolve_upvalue_scope(current_scope, name);
    if (upvalue_idx != -1) {
        emit_op(OP_SETUPVAL);
        emit_byte((uint8_t)upvalue_idx);
        return;
    }

    emit_op(OP_SETGLOBAL);
    emit_byte(add_global(name));
}

static void compile_block(Block* block) {
    uint16_t starting_local_count = current_scope->active_local_count;
    Stmt* stmt = block ? block->head : 0;

    while (stmt) {
        compile_stmt(stmt);
        if (current_chunk->has_error) {
            return;
        }
        stmt = stmt->next;
    }

    while (current_scope->active_local_count > starting_local_count) {
        emit_op(OP_POP);
        current_scope->active_local_count--;
    }
}

static int count_exprs(ExprList* exprs) {
    int count = 0;
    while (exprs) {
        count++;
        exprs = exprs->next;
    }
    return count;
}

static bool compile_builtin_call(Expr* expr) {
    const char* name;
    int arg_count;

    if (!expr || expr->type != EXPR_CALL || !expr->data.call.func || expr->data.call.func->type != EXPR_VAR) {
        return false;
    }

    name = expr->data.call.func->data.var_name;
    arg_count = count_exprs(expr->data.call.args);

    if (strcmp(name, "print") == 0) {
        ExprList* arg = expr->data.call.args;
        while (arg) {
            compile_expr(arg->expr);
            arg = arg->next;
        }
        emit_op(OP_PRINT);
        emit_byte((uint8_t)arg_count);
        emit_op(OP_LOADNIL);
        return true;
    }

    if (strcmp(name, "type") == 0 || strcmp(name, "tostring") == 0 || strcmp(name, "tonumber") == 0) {
        if (arg_count > 1) {
            compiler_fail_at_expr(expr, "Builtin expects at most 1 argument");
            return true;
        }

        if (expr->data.call.args) compile_expr(expr->data.call.args->expr);
        else emit_op(OP_LOADNIL);

        if (strcmp(name, "type") == 0) emit_op(OP_TYPE);
        else if (strcmp(name, "tostring") == 0) emit_op(OP_TOSTRING);
        else emit_op(OP_TONUMBER);
        return true;
    }

    if (strcmp(name, "input") == 0) {
        if (arg_count > 1) {
            compiler_fail_at_expr(expr, "input expects at most 1 argument");
            return true;
        }

        if (expr->data.call.args) {
            compile_expr(expr->data.call.args->expr);
        }
        emit_op(OP_INPUT);
        emit_byte((uint8_t)arg_count);
        return true;
    }

    return false;
}

static void compile_expr(Expr* expr) {
    ExprList* arg;
    TableField* f;
    const Expr* saved_expr = current_expr_node;

    if (!expr) return;
    if (current_chunk->has_error) return;

    current_expr_node = expr;

    switch (expr->type) {
        case EXPR_NIL:
            emit_op(OP_LOADNIL);
            break;
        case EXPR_BOOL:
            emit_op(expr->data.boolean ? OP_LOADTRUE : OP_LOADFALSE);
            break;
        case EXPR_NUMBER: {
            Constant c;
            uint8_t idx;
            c.type = CONST_NUMBER;
            c.data.number = expr->data.number;
            idx = add_constant(&c);
            emit_op(OP_LOADCONST);
            emit_byte(idx);
        } break;
        case EXPR_STRING: {
            Constant c;
            uint8_t idx;
            c.type = CONST_STRING;
            c.data.string = expr->data.string_val;
            idx = add_constant(&c);
            emit_op(OP_LOADCONST);
            emit_byte(idx);
        } break;
        case EXPR_FUNCTION: {
            uint16_t func_idx = compile_function_body("<anon>", 0, expr->data.function.params, expr->data.function.body);
            if (current_chunk->has_error) {
                current_expr_node = saved_expr;
                return;
            }
            emit_function_value(func_idx);
        } break;
        case EXPR_VAR:
            emit_load_variable(expr->data.var_name);
            break;
        case EXPR_TABLE:
            emit_op(OP_NEWTABLE);
            f = expr->data.table_fields;
            {
                int array_index = 1;
                while (f) {
                    emit_op(OP_DUP);
                    switch (f->type) {
                        case TF_INDEX:
                            compile_expr(f->data.index.key);
                            compile_expr(f->data.index.value);
                            emit_op(OP_SETTABLE);
                            break;
                        case TF_FIELD: {
                            Constant c;
                            uint8_t idx;
                            c.type = CONST_STRING;
                            c.data.string = f->data.field.name;
                            idx = add_constant(&c);
                            emit_op(OP_LOADCONST);
                            emit_byte(idx);
                            compile_expr(f->data.field.value);
                            emit_op(OP_SETTABLE);
                        } break;
                        case TF_ARRAY: {
                            Constant c;
                            uint8_t idx;
                            c.type = CONST_NUMBER;
                            c.data.number = array_index++;
                            idx = add_constant(&c);
                            emit_op(OP_LOADCONST);
                            emit_byte(idx);
                            compile_expr(f->data.array_expr);
                            emit_op(OP_SETTABLE);
                        } break;
                    }
                    f = f->next;
                }
            }
            break;
        case EXPR_BINOP:
            switch (expr->data.binop.op) {
                case BINOP_AND: {
                    uint16_t end_jump_pos;
                    compile_expr(expr->data.binop.left);
                    emit_op(OP_DUP);
                    emit_op(OP_JUMPIFFALSE);
                    end_jump_pos = current_func()->code_len;
                    emit_i16(0);
                    emit_op(OP_POP);
                    compile_expr(expr->data.binop.right);
                    patch_relative_jump(end_jump_pos, current_func()->code_len);
                } break;
                case BINOP_OR: {
                    uint16_t false_jump_pos;
                    uint16_t end_jump_pos;
                    compile_expr(expr->data.binop.left);
                    emit_op(OP_DUP);
                    emit_op(OP_JUMPIFFALSE);
                    false_jump_pos = current_func()->code_len;
                    emit_i16(0);
                    emit_op(OP_JUMP);
                    end_jump_pos = current_func()->code_len;
                    emit_i16(0);
                    patch_relative_jump(false_jump_pos, current_func()->code_len);
                    emit_op(OP_POP);
                    compile_expr(expr->data.binop.right);
                    patch_relative_jump(end_jump_pos, current_func()->code_len);
                } break;
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
                case BINOP_POW: emit_op(OP_POW); break;
                case BINOP_EQ: emit_op(OP_EQ); break;
                case BINOP_NE: emit_op(OP_NE); break;
                case BINOP_LT: emit_op(OP_LT); break;
                case BINOP_LE: emit_op(OP_LE); break;
                case BINOP_GT: emit_op(OP_GT); break;
                case BINOP_GE: emit_op(OP_GE); break;
                case BINOP_CONCAT: emit_op(OP_CONCAT); break;
                default: break;
            }
            break;
        case EXPR_UNOP:
            compile_expr(expr->data.unop.expr);
            switch (expr->data.unop.op) {
                case UNOP_NEG: emit_op(OP_NEG); break;
                case UNOP_NOT: emit_op(OP_NOT); break;
                case UNOP_LEN: emit_op(OP_LEN); break;
            }
            break;
        case EXPR_CALL:
            if (compile_builtin_call(expr)) {
                return;
            }
            compile_expr(expr->data.call.func);
            arg = expr->data.call.args;
            {
                uint8_t arg_count = 0;
                while (arg) {
                    compile_expr(arg->expr);
                    arg_count++;
                    arg = arg->next;
                }
                emit_op(OP_CALL);
                emit_byte(arg_count);
            }
            break;
        case EXPR_INDEX:
            compile_expr(expr->data.index.base);
            compile_expr(expr->data.index.key);
            emit_op(OP_GETTABLE);
            break;
        case EXPR_FIELD: {
            Constant c;
            uint8_t idx;
            compile_expr(expr->data.field.base);
            c.type = CONST_STRING;
            c.data.string = expr->data.field.field;
            idx = add_constant(&c);
            emit_op(OP_LOADCONST);
            emit_byte(idx);
            emit_op(OP_GETTABLE);
        } break;
        default:
            break;
    }

    current_expr_node = saved_expr;
}

static int collect_lvalues(LValueList* list, LValueList** items, int max_items) {
    int count = 0;
    while (list && count < max_items) {
        items[count++] = list;
        list = list->next;
    }
    return count;
}

static void compile_stmt(Stmt* stmt) {
    const Stmt* saved_stmt = current_stmt_node;

    if (!stmt) return;
    if (current_chunk->has_error) return;

    current_stmt_node = stmt;

    switch (stmt->type) {
        case STMT_ASSIGN: {
            ExprList* el = stmt->data.assign.exprs;
            LValueList* lvalue_items[MAX_LOCALS];
            int expr_count;
            int lvalue_count;

            while (el) {
                compile_expr(el->expr);
                el = el->next;
            }

            expr_count = count_exprs(stmt->data.assign.exprs);
            lvalue_count = collect_lvalues(stmt->data.assign.lvars, lvalue_items, MAX_LOCALS);

            while (expr_count > lvalue_count) {
                emit_op(OP_POP);
                expr_count--;
            }
            while (expr_count < lvalue_count) {
                emit_op(OP_LOADNIL);
                expr_count++;
            }

            while (lvalue_count > 0) {
                LValueList* lv;
                lvalue_count--;
                lv = lvalue_items[lvalue_count];
                if (lv->lval.type == LVAL_VAR) {
                    emit_store_variable(lv->lval.data.var_name);
                } else if (lv->lval.type == LVAL_INDEX) {
                    compile_expr(lv->lval.data.index.base);
                    compile_expr(lv->lval.data.index.key);
                    emit_op(OP_ROT3);
                    emit_op(OP_SETTABLE);
                } else if (lv->lval.type == LVAL_FIELD) {
                    Constant c;
                    uint8_t idx;
                    compile_expr(lv->lval.data.field.base);
                    c.type = CONST_STRING;
                    c.data.string = lv->lval.data.field.field;
                    idx = add_constant(&c);
                    emit_op(OP_LOADCONST);
                    emit_byte(idx);
                    emit_op(OP_ROT3);
                    emit_op(OP_SETTABLE);
                }
            }
        } break;
        case STMT_CALL:
            compile_expr(stmt->data.call);
            emit_op(OP_POP);
            break;
        case STMT_IF: {
            uint16_t end_jumps[MAX_LOCALS];
            uint16_t end_jump_count = 0;
            ElseIf* elif = 0;

            compile_expr(stmt->data.if_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            uint16_t false_jump = current_func()->code_len;
            emit_i16(0);

            compile_block(stmt->data.if_stmt.then_block);

            if (stmt->data.if_stmt.elseifs || stmt->data.if_stmt.else_block) {
                emit_op(OP_JUMP);
                end_jumps[end_jump_count++] = current_func()->code_len;
                emit_i16(0);
            }

            patch_relative_jump(false_jump, current_func()->code_len);

            elif = stmt->data.if_stmt.elseifs;
            while (elif) {
                compile_expr(elif->cond);
                emit_op(OP_JUMPIFFALSE);
                false_jump = current_func()->code_len;
                emit_i16(0);
                compile_block(elif->block);
                if (elif->next || stmt->data.if_stmt.else_block) {
                    emit_op(OP_JUMP);
                    end_jumps[end_jump_count++] = current_func()->code_len;
                    emit_i16(0);
                }
                patch_relative_jump(false_jump, current_func()->code_len);
                elif = elif->next;
            }

            if (stmt->data.if_stmt.else_block) compile_block(stmt->data.if_stmt.else_block);

            while (end_jump_count > 0) {
                end_jump_count--;
                patch_relative_jump(end_jumps[end_jump_count], current_func()->code_len);
            }
        } break;
        case STMT_WHILE: {
            uint16_t loop_start = current_func()->code_len;
            uint16_t exit_jump;
            compile_expr(stmt->data.while_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            exit_jump = current_func()->code_len;
            emit_i16(0);
            compile_block(stmt->data.while_stmt.block);
            emit_op(OP_JUMP);
            emit_i16((int16_t)(loop_start - current_func()->code_len - 2));
            patch_relative_jump(exit_jump, current_func()->code_len);
        } break;
        case STMT_REPEAT: {
            uint16_t loop_start = current_func()->code_len;
            compile_block(stmt->data.repeat_stmt.block);
            compile_expr(stmt->data.repeat_stmt.cond);
            emit_op(OP_JUMPIFFALSE);
            emit_i16((int16_t)(loop_start - current_func()->code_len - 2));
        } break;
        case STMT_FOR_NUM: {
            uint16_t base_local = current_scope->active_local_count;
            uint16_t loop_check;
            uint16_t exit_jump;
            uint8_t var_idx;
            uint8_t limit_idx;
            uint8_t step_idx;
            bool descending = is_negative_number_literal(stmt->data.for_num.step);

            compile_expr(stmt->data.for_num.start);
            if (current_func()->local_is_captured[base_local]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local]);
            }
            current_scope->active_local_count++;
            compile_expr(stmt->data.for_num.end);
            if (current_func()->local_is_captured[base_local + 1]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local + 1]);
            }
            current_scope->active_local_count++;
            if (stmt->data.for_num.step) compile_expr(stmt->data.for_num.step);
            else emit_number_constant(1);
            if (current_func()->local_is_captured[base_local + 2]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local + 2]);
            }
            current_scope->active_local_count++;

            var_idx = (uint8_t)base_local;
            limit_idx = (uint8_t)(base_local + 1);
            step_idx = (uint8_t)(base_local + 2);

            loop_check = current_func()->code_len;
            if (current_func()->local_is_captured[var_idx]) {
                emit_op(OP_GETCAPTURED); emit_byte(current_func()->local_env_slot[var_idx]);
            } else {
                emit_op(OP_GETLOCAL); emit_byte(var_idx);
            }
            if (current_func()->local_is_captured[limit_idx]) {
                emit_op(OP_GETCAPTURED); emit_byte(current_func()->local_env_slot[limit_idx]);
            } else {
                emit_op(OP_GETLOCAL); emit_byte(limit_idx);
            }
            emit_op(descending ? OP_GE : OP_LE);
            emit_op(OP_JUMPIFFALSE);
            exit_jump = current_func()->code_len;
            emit_i16(0);
            compile_block(stmt->data.for_num.block);

            if (current_func()->local_is_captured[var_idx]) emit_op(OP_GETCAPTURED), emit_byte(current_func()->local_env_slot[var_idx]);
            else emit_op(OP_GETLOCAL), emit_byte(var_idx);
            if (current_func()->local_is_captured[step_idx]) emit_op(OP_GETCAPTURED), emit_byte(current_func()->local_env_slot[step_idx]);
            else emit_op(OP_GETLOCAL), emit_byte(step_idx);
            emit_op(OP_ADD);
            if (current_func()->local_is_captured[var_idx]) {
                emit_op(OP_SETCAPTURED); emit_byte(current_func()->local_env_slot[var_idx]);
            } else {
                emit_op(OP_SETLOCAL); emit_byte(var_idx);
            }
            emit_op(OP_JUMP);
            emit_i16((int16_t)(loop_check - current_func()->code_len - 2));

            patch_relative_jump(exit_jump, current_func()->code_len);
            emit_op(OP_POP);
            emit_op(OP_POP);
            emit_op(OP_POP);
            current_scope->active_local_count = base_local;
        } break;
        case STMT_FOR_IN: {
            uint16_t base_local = current_scope->active_local_count;
            uint16_t loop_start;
            uint16_t exit_jump;
            uint8_t table_idx;
            uint8_t index_idx;
            uint8_t value_idx;

            if (!stmt->data.for_in.vars || !stmt->data.for_in.exprs) break;

            compile_expr(stmt->data.for_in.exprs->expr);
            if (current_func()->local_is_captured[base_local]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local]);
            }
            current_scope->active_local_count++;
            emit_number_constant(1);
            if (current_func()->local_is_captured[base_local + 1]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local + 1]);
            }
            current_scope->active_local_count++;
            emit_op(OP_LOADNIL);
            if (current_func()->local_is_captured[base_local + 2]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[base_local + 2]);
            }
            current_scope->active_local_count++;

            table_idx = (uint8_t)base_local;
            index_idx = (uint8_t)(base_local + 1);
            value_idx = (uint8_t)(base_local + 2);

            loop_start = current_func()->code_len;
            if (current_func()->local_is_captured[table_idx]) emit_op(OP_GETCAPTURED), emit_byte(current_func()->local_env_slot[table_idx]);
            else emit_op(OP_GETLOCAL), emit_byte(table_idx);
            if (current_func()->local_is_captured[index_idx]) emit_op(OP_GETCAPTURED), emit_byte(current_func()->local_env_slot[index_idx]);
            else emit_op(OP_GETLOCAL), emit_byte(index_idx);
            emit_op(OP_GETTABLE);
            emit_op(OP_DUP);
            emit_op(OP_JUMPIFFALSE);
            exit_jump = current_func()->code_len;
            emit_i16(0);
            if (current_func()->local_is_captured[value_idx]) {
                emit_op(OP_SETCAPTURED); emit_byte(current_func()->local_env_slot[value_idx]);
            } else {
                emit_op(OP_SETLOCAL); emit_byte(value_idx);
            }

            compile_block(stmt->data.for_in.block);

            if (current_func()->local_is_captured[index_idx]) emit_op(OP_GETCAPTURED), emit_byte(current_func()->local_env_slot[index_idx]);
            else emit_op(OP_GETLOCAL), emit_byte(index_idx);
            emit_number_constant(1);
            emit_op(OP_ADD);
            if (current_func()->local_is_captured[index_idx]) {
                emit_op(OP_SETCAPTURED); emit_byte(current_func()->local_env_slot[index_idx]);
            } else {
                emit_op(OP_SETLOCAL); emit_byte(index_idx);
            }
            emit_op(OP_JUMP);
            emit_i16((int16_t)(loop_start - current_func()->code_len - 2));

            patch_relative_jump(exit_jump, current_func()->code_len);
            emit_op(OP_POP);
            emit_op(OP_POP);
            emit_op(OP_POP);
            current_scope->active_local_count = base_local;
        } break;
        case STMT_RETURN: {
            ExprList* el = stmt->data.return_exprs;
            if (el) compile_expr(el->expr);
            else emit_op(OP_LOADNIL);
            emit_op(OP_RETURN);
            emit_byte(el ? 1 : 0);
        } break;
        case STMT_FUNC_DEF: {
            uint16_t func_idx = compile_function_body(stmt->data.func_def.name.base, stmt->data.func_def.name.base, stmt->data.func_def.params, stmt->data.func_def.body);
            emit_function_value(func_idx);
            emit_store_variable(stmt->data.func_def.name.base);
        } break;
        case STMT_LOCAL_FUNC: {
            uint16_t slot = current_scope->active_local_count;
            uint16_t func_idx = compile_function_body(stmt->data.local_func.name, stmt->data.local_func.name, stmt->data.local_func.params, stmt->data.local_func.body);
            emit_function_value(func_idx);
            if (current_func()->local_is_captured[slot]) {
                emit_op(OP_DUP);
                emit_op(OP_SETCAPTURED);
                emit_byte(current_func()->local_env_slot[slot]);
            }
            current_scope->active_local_count++;
        } break;
        case STMT_LOCAL: {
            IdentList* name = stmt->data.local.names;
            ExprList* val = stmt->data.local.exprs;
            while (name) {
                uint16_t slot = current_scope->active_local_count;
                if (val) {
                    compile_expr(val->expr);
                    val = val->next;
                } else {
                    emit_op(OP_LOADNIL);
                }
                if (current_func()->local_is_captured[slot]) {
                    emit_op(OP_DUP);
                    emit_op(OP_SETCAPTURED);
                    emit_byte(current_func()->local_env_slot[slot]);
                }
                current_scope->active_local_count++;
                name = name->next;
            }
        } break;
        default:
            break;
    }

    current_stmt_node = saved_stmt;
}

void compiler_init(void) {
}

bool compiler_compile(Chunk* ast_chunk, CompiledChunk* out_chunk) {
    ScopeContext main_scope;

    if (!out_chunk) return false;

    current_chunk = out_chunk;
    current_chunk->func_count = 0;
    current_chunk->global_count = 0;
    current_chunk->has_error = false;
    current_chunk->error_line = 0;
    current_chunk->error_column = 0;
    current_chunk->error_msg[0] = '\0';
    current_stmt_node = 0;
    current_expr_node = 0;

    init_bytecode_function(&current_chunk->main, "main");
    analyze_function_body(&current_chunk->main, 0, 0, false, 0, ast_chunk ? ast_chunk->block : 0);

    main_scope.func = &current_chunk->main;
    main_scope.active_local_count = current_chunk->main.initial_local_count;
    main_scope.enclosing = 0;
    current_scope = &main_scope;

    if (!ast_chunk || !ast_chunk->block) {
        return compiler_fail(0, 0, "Invalid AST input");
    }

    compile_block(ast_chunk->block);
    if (current_chunk->has_error) {
        return false;
    }
    emit_op(OP_HALT);
    return !current_chunk->has_error;
}

static bool compiler_fail_at_stmt(const Stmt* stmt, const char* msg) {
    if (stmt) {
        return compiler_fail(stmt->line, stmt->column, msg);
    }
    return compiler_fail(0, 0, msg);
}

static bool compiler_fail_at_expr(const Expr* expr, const char* msg) {
    if (expr) {
        return compiler_fail(expr->line, expr->column, msg);
    }
    return compiler_fail(0, 0, msg);
}