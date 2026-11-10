/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: AST
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>


typedef struct AstNode AstNode;
typedef struct AstBlock AstBlock;

typedef enum {
    /* program */
    NODE_PROGRAM,
    NODE_FUNC_DEF,
    NODE_EXTENSION,
    NODE_PARAM,
    NODE_BLOCK,

    /* príkazy */
    NODE_VAL_DEF,
    NODE_VAR_DEF,
    NODE_ASSIGN,
    NODE_IF_STMT,
    NODE_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_EXPR_STMT,

    /* výrazy */
    NODE_LIT_INT,
    NODE_LIT_DOUBLE,
    NODE_LIT_STRING,
    NODE_LIT_BOOL,
    NODE_LIT_UNIT,
    NODE_ISTRING,
    NODE_ID,
    NODE_UNARY_OP,      // -a, !a
    NODE_BINARY_OP,     // a + b
    NODE_CALL,          // f(x)
    NODE_MEMBER,        // x.toInt / x.foo()
    NODE_IF_EXPR        // if ako výraz, else povinný
} NodeKind;

typedef enum {
    OP_ADD,   // +
    OP_SUB,   // -
    OP_MUL,   // *
    OP_DIV,   // /
    OP_MOD,   // %
    OP_LT,    // <
    OP_LE,    // <=
    OP_GT,    // >
    OP_GE,    // >=
    OP_EQ,    // ==
    OP_NE,    // !=
    OP_AND,   // &&
    OP_OR,    // ||
    OP_NEG,   // -a
    OP_NOT    // !a
} OpType;

typedef enum {
    TYPE_UNKNOWN = 0, 
    TYPE_INT,         
    TYPE_DOUBLE,      
    TYPE_STRING,      
    TYPE_BOOL,        
    TYPE_UNIT         
} DataType;

struct AstBlock {
    AstNode **statements; 
    int count;            
    int capacity;       
};

struct AstNode {
    NodeKind kind;
    DataType type;       
    int line;
    int column;

    union {

        union {
            long long int_val;
            double double_val;
            char *str_val;
            bool bool_val;
        } literal;          

        struct {
            OpType op;
            AstNode *operand;
        } unary;

        struct {
            OpType op;
            AstNode *left;
            AstNode *right;
        } binary;

        struct {
            char *name;
            DataType declared_type;     
            AstNode *initializer; 
        } var_val;                      

        struct {
            char *name;         
            AstNode *value;      
        } assign;

        struct {
            AstNode *child;     // NODE_EXPR_STMT a NODE_RETURN 
        } expr_stmt;

        struct {
            AstNode *cond;        
            AstNode *then_branch; 
            AstNode *else_branch;   
        } if_node;                  

        struct {
            AstNode *cond; 
            AstNode *body; 
        } while_node;

        struct {
            char *name;         
            AstNode **args;     
            int arg_count;      
            int *list_sizes;    // Počty argumentov v jednotlivých zátvorkách f(a)(b, c)
            int list_count;     // Počet zoznamov argumentov
        } call_node;

        struct {
            AstNode *receiver;  
            char *member_name;  
            AstNode **args;     
            int arg_count;      
            int *list_sizes;    
            int list_count;     
            bool has_parens;    
        } member_node;

        struct {
            AstNode **parts;    
            int part_count;     
        } istring_node;

        struct {
            char *var_name;     
            AstNode *from;      
            AstNode *to;        
            bool is_until;      
            AstNode *by;      
            AstNode **guards;   
            int guard_count;    
            AstNode *body;      
        } for_node;

        struct {
            char *name;         
            DataType type;      
        } param_node;

        struct {
            char *name;         
            AstNode **params;       // všetky parametre naplocho
            int param_count;        // celkový počet parametrov
            int *list_sizes;        // počty parametrov v jednotlivých zátvorkách def f(a)(b, c)
            int list_count;         // počet zoznamov parametrov
            bool has_parens;    
            DataType return_type; 
            AstNode *body;      
            bool is_main;       
        } func_node;

        struct {
            AstBlock block; 
            int count;            
            int capacity;          
        } block_node;

        struct {
            char *receiver_name;   
            DataType receiver_type; 
            AstNode **methods;     
            int method_count;      
        } extension_node;

        struct {
            AstNode **functions;   
            int function_count;    
            AstNode **extensions;  
            int extension_count;   
        } program_node;

        struct {
            char *name;
        } identifier;


    } ast;
};


AstNode *ast_new_literal_int(long long val, int line, int col);
AstNode *ast_new_literal_double(double val, int line, int col);
AstNode *ast_new_literal_string(char *val, int line, int col);
AstNode *ast_new_literal_bool(bool val, int line, int col);
AstNode *ast_new_literal_unit(int line, int col);
AstNode *ast_new_identifier(char *name, int line, int col);

AstNode *ast_new_unary(OpType op, AstNode *operand, int line, int col);
AstNode *ast_new_binary(OpType op, AstNode *left, AstNode *right, int line, int col);
AstNode *ast_new_val_def(char *name, DataType declared_type, AstNode *initializer, int line, int col);
AstNode *ast_new_var_def(char *name, DataType declared_type, AstNode *initializer, int line, int col);
AstNode *ast_new_assign(char *name, AstNode *value, int line, int col);
AstNode *ast_new_expr_stmt(AstNode *expr, int line, int col);
AstNode *ast_new_return(AstNode *expr, int line, int col);

AstNode *ast_new_if(AstNode *cond, AstNode *then_branch, AstNode *else_branch, int line, int col);       // NODE_IF_STMT
AstNode *ast_new_if_expr(AstNode *cond, AstNode *then_branch, AstNode *else_branch, int line, int col);  // NODE_IF_EXPR
AstNode *ast_new_while(AstNode *cond, AstNode *body, int line, int col);
AstNode *ast_new_for(char *var_name, AstNode *from, AstNode *to, bool is_until, AstNode *step, AstNode **guards, int guard_count, AstNode *body, int line, int col);

AstNode *ast_new_call(char *name, AstNode **args, int arg_count, int *list_sizes, int list_count, int line, int col);
AstNode *ast_new_member(AstNode *receiver, char *member_name, AstNode **args, int arg_count, int *list_sizes, int list_count, bool has_parens, int line, int col);
AstNode *ast_new_istring(AstNode **parts, int part_count, int line, int col);

AstNode *ast_new_param(char *name, DataType type, int line, int col);
AstNode *ast_new_func_def(char *name, AstNode **params, int param_count, int *list_sizes, int list_count, bool has_parens, DataType return_type, AstNode *body, bool is_main, int line, int col);
AstNode *ast_new_block(AstNode **statements, int count, int capacity, int line, int col);
AstNode *ast_new_extension(char *receiver_name, DataType receiver_type, AstNode **methods, int method_count, int line, int col);
AstNode *ast_new_program(AstNode **functions, int function_count, AstNode **extensions, int extension_count, int line, int col);


void block_init(AstBlock *block);                  
void block_append(AstBlock *block, AstNode *stmt);
void ast_free(AstNode *node);

#endif 