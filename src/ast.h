/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: AST
 */

#ifndef AST_H
#define AST_H

typedef enum {
	NODE_LIT_INT,
	NODE_LIT_DOUBLE,
	NODE_LIT_STRING,
	NODE_IDENTIFIER,
	NODE_BINARY_OP, // a + b
	NODE_VAR_DEF,   // val x = 5
	NODE_ASSIGN,    // x = 10
	NODE_IF,
	NODE_WHILE,
	NODE_RETURN
} NodeKind;

typedef enum {
	OP_ADD, // +
	OP_SUB, // -
	OP_MUL, // *
	OP_DIV  // /
} OpType;

typedef struct AstNode AstNode;
typedef struct AstBlock AstBlock;

// Block of orders if (something)
//                    [something]
struct AstBlock {
	AstNode **statements; // array of pointers to commands
	int count;            // number of commands
	int capacity;         // allocated capacity
};

struct AstNode {
	NodeKind kind;
	int line;
	int column;

	union {

		struct {
			int int_val;
			double double_val;
			char *string_val;
		} literal;

		char *name; // identifier name

		// binary operations
		struct {
			OpType op; // (+,-,*,/)
			AstNode *left;
			AstNode *right;
		} binary;

		struct {
			char *name;
			AstNode *init_expr; // pointer to NODE_LIT_INT/DOUBLE/STRING
		} variable_definiton;

		struct {
			AstNode *condition;   // pointer to node
			AstBlock *then_block; // pointer to block
			AstBlock *else_block; // NULL if !else
		} if_statement;

		struct {
			AstNode *condition; // loop condition
			AstBlock *body;     // loop body
		} while_statement;

		struct {
			AstNode *expr; // returned expression (can be NULL)
		} return_statement;
	};
};

#endif