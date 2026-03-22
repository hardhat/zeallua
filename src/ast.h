#ifndef AST_H
#define AST_H

#include "token.h"
#include <stdint.h>
#include <stdbool.h>

// Max pool sizes for compiler memory allocation
#define AST_POOL_SIZE 8192
#define STR_POOL_SIZE 2048

void ast_reset(void);
void* ast_alloc(uint16_t size);
const char* ast_strdup(const char* str);

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Block Block;

typedef enum {
    BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
    BINOP_POW, BINOP_EQ, BINOP_NE, BINOP_LT, BINOP_LE,
    BINOP_GT, BINOP_GE, BINOP_AND, BINOP_OR, BINOP_CONCAT
} BinOp;

typedef enum {
    UNOP_NEG, UNOP_NOT, UNOP_LEN
} UnOp;

typedef enum {
    EXPR_NIL, EXPR_BOOL, EXPR_NUMBER, EXPR_STRING,
    EXPR_VAR, EXPR_TABLE, EXPR_BINOP, EXPR_UNOP,
    EXPR_CALL, EXPR_METHOD_CALL, EXPR_INDEX, EXPR_FIELD,
    EXPR_FUNCTION
} ExprType;

typedef struct ExprList {
    Expr* expr;
    struct ExprList* next;
} ExprList;

typedef struct IdentList {
    const char* ident;
    struct IdentList* next;
} IdentList;

typedef struct TableField {
    enum { TF_INDEX, TF_FIELD, TF_ARRAY } type;
    union {
        struct { Expr* key; Expr* value; } index;
        struct { const char* name; Expr* value; } field;
        Expr* array_expr;
    } data;
    struct TableField* next;
} TableField;

struct Expr {
    ExprType type;
    uint16_t line;
    uint16_t column;
    union {
        bool boolean;
        int16_t number;
        const char* string_val;
        const char* var_name;
        
        TableField* table_fields;
        
        struct { Expr* left; BinOp op; Expr* right; } binop;
        struct { UnOp op; Expr* expr; } unop;
        
        struct { Expr* func; ExprList* args; } call;
        struct { Expr* obj; const char* method; ExprList* args; } method_call;
        
        struct { Expr* base; Expr* key; } index;
        struct { Expr* base; const char* field; } field;
        
        struct { IdentList* params; Block* body; } function;
    } data;
};

typedef enum {
    LVAL_VAR,
    LVAL_INDEX,
    LVAL_FIELD
} LValueType;

typedef struct LValue {
    LValueType type;
    union {
        const char* var_name;
        struct { Expr* base; Expr* key; } index;
        struct { Expr* base; const char* field; } field;
    } data;
} LValue;

typedef struct LValueList {
    LValue lval;
    struct LValueList* next;
} LValueList;

typedef struct ElseIf {
    Expr* cond;
    Block* block;
    struct ElseIf* next;
} ElseIf;

typedef struct FuncName {
    const char* base;
    IdentList* fields;
    const char* method;
} FuncName;

typedef enum {
    STMT_ASSIGN, STMT_LOCAL, STMT_CALL, STMT_IF,
    STMT_WHILE, STMT_REPEAT, STMT_FOR_NUM, STMT_FOR_IN,
    STMT_FUNC_DEF, STMT_LOCAL_FUNC, STMT_RETURN, STMT_BREAK,
    STMT_DO
} StmtType;

struct Stmt {
    StmtType type;
    uint16_t line;
    uint16_t column;
    union {
        struct { LValueList* lvars; ExprList* exprs; } assign;
        struct { IdentList* names; ExprList* exprs; } local;
        Expr* call;
        struct { Expr* cond; Block* then_block; ElseIf* elseifs; Block* else_block; } if_stmt;
        struct { Expr* cond; Block* block; } while_stmt;
        struct { Expr* cond; Block* block; } repeat_stmt;
        struct { const char* var; Expr* start; Expr* end; Expr* step; Block* block; } for_num;
        struct { IdentList* vars; ExprList* exprs; Block* block; } for_in;
        struct { FuncName name; IdentList* params; Block* body; } func_def;
        struct { const char* name; IdentList* params; Block* body; } local_func;
        ExprList* return_exprs;
        Block* do_block;
    } data;
    struct Stmt* next;
};

struct Block {
    Stmt* head;
    Stmt* tail;
};

typedef struct Chunk {
    Block* block;
} Chunk;

#endif
