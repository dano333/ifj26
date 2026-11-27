/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: AST
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

static AstNode *ast_alloc(NodeKind kind, int line, int col) {
	AstNode *node = calloc(1, sizeof(AstNode));
	if (!node)
		return NULL;

	node->kind = kind;
	node->type = TYPE_UNKNOWN;
	node->line = line;
	node->column = col;

	return node;
}

void block_init(AstBlock *block) {
	if (!block)
		return;
	block->statements = NULL;
	block->count = 0;
	block->capacity = 0;
}

bool block_append(AstBlock *block, AstNode *stmt) {
	if (!block || !stmt)
		return false;

	if (block->count >= block->capacity) {
		int new_cap = (block->capacity == 0) ? 4 : block->capacity * 2;
		AstNode **new_stmts = realloc(block->statements, new_cap * sizeof(AstNode *));
		if (!new_stmts) {
			return false;
		}

		block->statements = new_stmts;
		block->capacity = new_cap;
	}

	block->statements[block->count++] = stmt;
	return true;
}

AstNode *ast_new_literal_int(long long val, int line, int col) {
	AstNode *node = ast_alloc(NODE_LIT_INT, line, col);
	if (!node)
		return NULL;
	node->type = TYPE_INT;
	node->ast.literal.int_val = val;
	return node;
}

AstNode *ast_new_literal_double(double val, int line, int col) {
	AstNode *node = ast_alloc(NODE_LIT_DOUBLE, line, col);
	if (!node)
		return NULL;
	node->type = TYPE_DOUBLE;
	node->ast.literal.double_val = val;
	return node;
}

AstNode *ast_new_literal_string(char *val, int line, int col) {
	AstNode *node = ast_alloc(NODE_LIT_STRING, line, col);
	if (!node)
		return NULL;
	node->type = TYPE_STRING;
	node->ast.literal.str_val = val;
	return node;
}

AstNode *ast_new_literal_bool(bool val, int line, int col) {
	AstNode *node = ast_alloc(NODE_LIT_BOOL, line, col);
	if (!node)
		return NULL;
	node->type = TYPE_BOOLEAN;
	node->ast.literal.bool_val = val;
	return node;
}

AstNode *ast_new_literal_unit(int line, int col) {
	AstNode *node = ast_alloc(NODE_LIT_UNIT, line, col);
	if (!node)
		return NULL;
	node->type = TYPE_UNIT;
	return node;
}

AstNode *ast_new_identifier(char *name, int line, int col) {
	AstNode *node = ast_alloc(NODE_ID, line, col);
	if (!node)
		return NULL;
	node->ast.identifier.name = name;
	return node;
}

AstNode *ast_new_unary(OpType op, AstNode *operand, int line, int col) {
	AstNode *node = ast_alloc(NODE_UNARY_OP, line, col);
	if (!node)
		return NULL;
	node->ast.unary.op = op;
	node->ast.unary.operand = operand;
	return node;
}

AstNode *ast_new_binary(OpType op, AstNode *left, AstNode *right, int line, int col) {
	AstNode *node = ast_alloc(NODE_BINARY_OP, line, col);
	if (!node)
		return NULL;
	node->ast.binary.op = op;
	node->ast.binary.left = left;
	node->ast.binary.right = right;
	return node;
}

AstNode *ast_new_val_def(char *name, DataType declared_type, AstNode *initializer, int line, int col) {
	AstNode *node = ast_alloc(NODE_VAL_DEF, line, col);
	if (!node)
		return NULL;
	node->type = declared_type;
	node->ast.var_val.name = name;
	node->ast.var_val.declared_type = declared_type;
	node->ast.var_val.initializer = initializer;
	return node;
}

AstNode *ast_new_var_def(char *name, DataType declared_type, AstNode *initializer, int line, int col) {
	AstNode *node = ast_alloc(NODE_VAR_DEF, line, col);
	if (!node)
		return NULL;
	node->type = declared_type;
	node->ast.var_val.name = name;
	node->ast.var_val.declared_type = declared_type;
	node->ast.var_val.initializer = initializer;
	return node;
}

AstNode *ast_new_assign(char *name, AstNode *value, int line, int col) {
	AstNode *node = ast_alloc(NODE_ASSIGN, line, col);
	if (!node)
		return NULL;
	node->ast.assign.name = name;
	node->ast.assign.value = value;
	return node;
}

AstNode *ast_new_expr_stmt(AstNode *expr, int line, int col) {
	AstNode *node = ast_alloc(NODE_EXPR_STMT, line, col);
	if (!node)
		return NULL;
	node->ast.expr_stmt.child = expr;
	return node;
}

AstNode *ast_new_return(AstNode *expr, int line, int col) {
	AstNode *node = ast_alloc(NODE_RETURN, line, col);
	if (!node)
		return NULL;
	node->ast.expr_stmt.child = expr;
	return node;
}

AstNode *ast_new_if(AstNode *cond, AstNode *then_branch, AstNode *else_branch, int line, int col) {
	AstNode *node = ast_alloc(NODE_IF_STMT, line, col);
	if (!node)
		return NULL;
	node->ast.if_node.cond = cond;
	node->ast.if_node.then_branch = then_branch;
	node->ast.if_node.else_branch = else_branch;
	return node;
}

AstNode *ast_new_if_expr(AstNode *cond, AstNode *then_branch, AstNode *else_branch, int line, int col) {
	AstNode *node = ast_alloc(NODE_IF_EXPR, line, col);
	if (!node)
		return NULL;
	node->ast.if_node.cond = cond;
	node->ast.if_node.then_branch = then_branch;
	node->ast.if_node.else_branch = else_branch;
	return node;
}

AstNode *ast_new_while(AstNode *cond, AstNode *body, int line, int col) {
	AstNode *node = ast_alloc(NODE_WHILE, line, col);
	if (!node)
		return NULL;
	node->ast.while_node.cond = cond;
	node->ast.while_node.body = body;
	return node;
}

AstNode *ast_new_for(char *var_name, AstNode *from, AstNode *to, bool is_until, AstNode *step, AstNode **guards,
                     int guard_count, AstNode *body, int line, int col) {
	AstNode *node = ast_alloc(NODE_FOR, line, col);
	if (!node)
		return NULL;
	node->ast.for_node.var_name = var_name;
	node->ast.for_node.from = from;
	node->ast.for_node.to = to;
	node->ast.for_node.is_until = is_until;
	node->ast.for_node.by = step;
	node->ast.for_node.guards = guards;
	node->ast.for_node.guard_count = guard_count;
	node->ast.for_node.body = body;
	return node;
}

AstNode *ast_new_call(char *name, AstNode **args, int arg_count, int *list_sizes, int list_count, int line, int col) {
	AstNode *node = ast_alloc(NODE_CALL, line, col);
	if (!node)
		return NULL;
	node->ast.call_node.name = name;
	node->ast.call_node.args = args;
	node->ast.call_node.arg_count = arg_count;
	node->ast.call_node.list_sizes = list_sizes;
	node->ast.call_node.list_count = list_count;
	return node;
}

AstNode *ast_new_member(AstNode *receiver, char *member_name, AstNode **args, int arg_count, int *list_sizes,
                        int list_count, bool has_parens, int line, int col) {
	AstNode *node = ast_alloc(NODE_MEMBER, line, col);
	if (!node)
		return NULL;
	node->ast.member_node.receiver = receiver;
	node->ast.member_node.member_name = member_name;
	node->ast.member_node.args = args;
	node->ast.member_node.arg_count = arg_count;
	node->ast.member_node.list_sizes = list_sizes;
	node->ast.member_node.list_count = list_count;
	node->ast.member_node.has_parens = has_parens;
	return node;
}

AstNode *ast_new_istring(AstNode **parts, int part_count, int line, int col) {
	AstNode *node = ast_alloc(NODE_ISTRING, line, col);
	if (!node)
		return NULL;
	node->ast.istring_node.parts = parts;
	node->ast.istring_node.part_count = part_count;
	return node;
}

AstNode *ast_new_param(char *name, DataType type, int line, int col) {
	AstNode *node = ast_alloc(NODE_PARAM, line, col);
	if (!node)
		return NULL;
	node->ast.param_node.name = name;
	node->ast.param_node.type = type;
	return node;
}

AstNode *ast_new_func_def(char *name, AstNode **params, int param_count, int *list_sizes, int list_count,
                          bool has_parens, DataType return_type, AstNode *body, bool is_main, int line, int col) {
	AstNode *node = ast_alloc(NODE_FUNC_DEF, line, col);
	if (!node)
		return NULL;
	node->ast.func_node.name = name;
	node->ast.func_node.params = params;
	node->ast.func_node.param_count = param_count;
	node->ast.func_node.list_sizes = list_sizes;
	node->ast.func_node.list_count = list_count;
	node->ast.func_node.has_parens = has_parens;
	node->ast.func_node.return_type = return_type;
	node->ast.func_node.body = body;
	node->ast.func_node.is_main = is_main;
	return node;
}

AstNode *ast_new_block(AstNode **statements, int count, int capacity, int line, int col) {
	AstNode *node = ast_alloc(NODE_BLOCK, line, col);
	if (!node)
		return NULL;
	node->ast.block_node.block.statements = statements;
	node->ast.block_node.block.count = count;
	node->ast.block_node.block.capacity = capacity;
	return node;
}

AstNode *ast_new_extension(char *receiver_name, DataType receiver_type, AstNode **methods, int method_count, int line,
                           int col) {
	AstNode *node = ast_alloc(NODE_EXTENSION, line, col);
	if (!node)
		return NULL;
	node->ast.extension_node.receiver_name = receiver_name;
	node->ast.extension_node.receiver_type = receiver_type;
	node->ast.extension_node.methods = methods;
	node->ast.extension_node.method_count = method_count;
	return node;
}

AstNode *ast_new_program(AstNode **functions, int function_count, AstNode **extensions, int extension_count, int line,
                         int col) {
	AstNode *node = ast_alloc(NODE_PROGRAM, line, col);
	if (!node)
		return NULL;
	node->ast.program_node.functions = functions;
	node->ast.program_node.function_count = function_count;
	node->ast.program_node.extensions = extensions;
	node->ast.program_node.extension_count = extension_count;
	return node;
}

void ast_free(AstNode *node) {
	if (!node)
		return;

	switch (node->kind) {
	case NODE_LIT_STRING:
		free(node->ast.literal.str_val);
		break;

	case NODE_ID:
		free(node->ast.identifier.name);
		break;

	case NODE_VAL_DEF:
	case NODE_VAR_DEF:
		free(node->ast.var_val.name);
		ast_free(node->ast.var_val.initializer);
		break;

	case NODE_ASSIGN:
		free(node->ast.assign.name);
		ast_free(node->ast.assign.value);
		break;

	case NODE_EXPR_STMT:
	case NODE_RETURN:
		ast_free(node->ast.expr_stmt.child);
		break;

	case NODE_UNARY_OP:
		ast_free(node->ast.unary.operand);
		break;

	case NODE_BINARY_OP:
		ast_free(node->ast.binary.left);
		ast_free(node->ast.binary.right);
		break;

	case NODE_IF_STMT:
	case NODE_IF_EXPR:
		ast_free(node->ast.if_node.cond);
		ast_free(node->ast.if_node.then_branch);
		ast_free(node->ast.if_node.else_branch);
		break;

	case NODE_WHILE:
		ast_free(node->ast.while_node.cond);
		ast_free(node->ast.while_node.body);
		break;

	case NODE_FOR:
		free(node->ast.for_node.var_name);
		ast_free(node->ast.for_node.from);
		ast_free(node->ast.for_node.to);
		ast_free(node->ast.for_node.by);
		ast_free(node->ast.for_node.body);
		if (node->ast.for_node.guards) {
			for (int i = 0; i < node->ast.for_node.guard_count; i++) {
				ast_free(node->ast.for_node.guards[i]);
			}
			free(node->ast.for_node.guards);
		}
		break;

	case NODE_CALL:
		free(node->ast.call_node.name);
		if (node->ast.call_node.args) {
			for (int i = 0; i < node->ast.call_node.arg_count; i++) {
				ast_free(node->ast.call_node.args[i]);
			}
			free(node->ast.call_node.args);
		}
		free(node->ast.call_node.list_sizes);
		break;

	case NODE_MEMBER:
		free(node->ast.member_node.member_name);
		ast_free(node->ast.member_node.receiver);
		if (node->ast.member_node.args) {
			for (int i = 0; i < node->ast.member_node.arg_count; i++) {
				ast_free(node->ast.member_node.args[i]);
			}
			free(node->ast.member_node.args);
		}
		free(node->ast.member_node.list_sizes);
		break;

	case NODE_ISTRING:
		if (node->ast.istring_node.parts) {
			for (int i = 0; i < node->ast.istring_node.part_count; i++) {
				ast_free(node->ast.istring_node.parts[i]);
			}
			free(node->ast.istring_node.parts);
		}
		break;

	case NODE_PARAM:
		free(node->ast.param_node.name);
		break;

	case NODE_FUNC_DEF:
		free(node->ast.func_node.name);
		if (node->ast.func_node.params) {
			for (int i = 0; i < node->ast.func_node.param_count; i++) {
				ast_free(node->ast.func_node.params[i]);
			}
			free(node->ast.func_node.params);
		}
		free(node->ast.func_node.list_sizes);
		ast_free(node->ast.func_node.body);
		break;

	case NODE_BLOCK:
		if (node->ast.block_node.block.statements) {
			for (int i = 0; i < node->ast.block_node.block.count; i++) {
				ast_free(node->ast.block_node.block.statements[i]);
			}
			free(node->ast.block_node.block.statements);
		}
		break;

	case NODE_EXTENSION:
		free(node->ast.extension_node.receiver_name);
		if (node->ast.extension_node.methods) {
			for (int i = 0; i < node->ast.extension_node.method_count; i++) {
				ast_free(node->ast.extension_node.methods[i]);
			}
			free(node->ast.extension_node.methods);
		}
		break;

	case NODE_PROGRAM:
		if (node->ast.program_node.functions) {
			for (int i = 0; i < node->ast.program_node.function_count; i++) {
				ast_free(node->ast.program_node.functions[i]);
			}
			free(node->ast.program_node.functions);
		}
		if (node->ast.program_node.extensions) {
			for (int i = 0; i < node->ast.program_node.extension_count; i++) {
				ast_free(node->ast.program_node.extensions[i]);
			}
			free(node->ast.program_node.extensions);
		}
		break;

	case NODE_LIT_INT:
	case NODE_LIT_DOUBLE:
	case NODE_LIT_BOOL:
	case NODE_LIT_UNIT:
		break;
	}

	free(node);
}