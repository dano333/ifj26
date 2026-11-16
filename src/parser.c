/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00), Jakub Krčmárik (xkrcmaj00)
 * Description: parser (statements, functions, blocks). Expressions live in expr.c
 */

#include "parser.h"
#include "error.h"
#include "expr.h"
#include "semantics.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- DEBUG MACRO FOR PARSING ERRORS --- */
#define PARSER_ERR(func_name)                                                                                          \
	do {                                                                                                               \
		fprintf(stderr, "\n[PARSER ERROR] %s() failed at line %d, col %d (Token Type: %d)\n", func_name,               \
		        parser->current_token.line, parser->current_token.column, parser->current_token.type);                 \
		return ERR_PARSING;                                                                                            \
	} while (0)

#define TOKEN_TYPE(p) ((p)->current_token.type)

/* ------------------------------------------------------------------------- */
/*  Helpers                                                                  */
/* ------------------------------------------------------------------------- */

static ErrorCode parser_next_token(Parser *parser) {
	token_free(&parser->current_token);
	return get_token(&parser->current_token, parser->scanner);
}

ErrorCode parser_init(Parser *parser, Scanner *scanner, SymTable *global_table) {
	parser->scanner = scanner;
	parser->current_scope = global_table;
	parser->current_ret_type = TYPE_UNKNOWN;
	parser->ext_recv_type = TYPE_UNKNOWN;
	memset(&parser->current_token, 0, sizeof(Token));
	return parser_next_token(parser);
}

ErrorCode compare_token(Parser *parser, TokenType expected) {
	if (parser->current_token.type == expected) {
		return parser_next_token(parser);
	}
	PARSER_ERR("compare_token");
}

/* frees an array of nodes and the array itself */
static void free_nodes(AstNode **arr, int n) {
	if (arr == NULL) {
		return;
	}
	for (int i = 0; i < n; i++) {
		ast_free(arr[i]);
	}
	free(arr);
}

/* appends a node; on failure the node is freed, the array stays valid */
static ErrorCode push_node(AstNode ***arr, int *count, AstNode *node) {
	AstNode **tmp = realloc(*arr, (size_t)(*count + 1) * sizeof(AstNode *));
	if (tmp == NULL) {
		ast_free(node);
		return ERR_INTERNAL_COMPILER;
	}
	*arr = tmp;
	tmp[(*count)++] = node;
	return ERR_OK;
}

static void scope_enter(Parser *parser, SymTable *scope) {
	symtable_init(scope, parser->current_scope);
	parser->current_scope = scope;
}

static void scope_leave(Parser *parser, SymTable *scope) {
	parser->current_scope = scope->parent;
	symtable_free(scope);
}

static bool is_f_def(TokenType type) { return (type == TOKEN_KW_DEF || type == TOKEN_SE_AT); }

static bool bool_like(DataType t) { return t == TYPE_BOOLEAN || t == TYPE_UNKNOWN; }

/* tokens that may directly follow a finished statement without a separator (e.g. after a DEDENT) */
static bool is_stmt_keyword_start(TokenType t) {
	return t == TOKEN_ID_IDENT || t == TOKEN_KW_VAL || t == TOKEN_KW_VAR || t == TOKEN_KW_RETURN || t == TOKEN_KW_IF ||
	       t == TOKEN_KW_WHILE || t == TOKEN_KW_FOR;
}

/* INDENT <F_BODY> DEDENT in a fresh scope */
static ErrorCode indented_block(Parser *parser, AstNode **out) {
	ErrorCode err;
	SymTable scope;
	*out = NULL;

	if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
		return err;
	}
	scope_enter(parser, &scope);
	err = f_body(parser, out);
	scope_leave(parser, &scope);
	if (err != ERR_OK) {
		return err;
	}
	if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
		ast_free(*out);
		*out = NULL;
		return err;
	}
	return ERR_OK;
}

/* wraps a single statement into a NODE_BLOCK so that every branch / loop body is a block */
static ErrorCode wrap_in_block(AstNode *st, AstNode **out) {
	if (st == NULL || st->kind == NODE_BLOCK) {
		*out = st;
		return ERR_OK;
	}
	AstNode **arr = malloc(sizeof(AstNode *));
	if (arr == NULL) {
		ast_free(st);
		return ERR_INTERNAL_COMPILER;
	}
	arr[0] = st;
	*out = ast_new_block(arr, 1, 1, st->line, st->column);
	if (*out == NULL) {
		free_nodes(arr, 1);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

/* a single statement in its own scope, returned as a block */
static ErrorCode scoped_stmt(Parser *parser, AstNode **out) {
	SymTable scope;
	AstNode *st = NULL;
	scope_enter(parser, &scope);
	ErrorCode err = stmt(parser, &st);
	scope_leave(parser, &scope);
	if (err != ERR_OK) {
		*out = NULL;
		return err;
	}
	return wrap_in_block(st, out);
}

/* =============================================================================================================== */
/*  PROLOG (rule 1)                                                                                                */
/* =============================================================================================================== */
ErrorCode prolog(Parser *parser, AstNode **out_program_node) {
	ErrorCode err = ERR_OK;
	*out_program_node = NULL;

	while (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
	}

	if ((err = compare_token(parser, TOKEN_KW_IMPORT)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("prolog");
	}
	if (parser->current_token.value.string_value == NULL ||
	    strcmp(parser->current_token.value.string_value, "ifj26") != 0) {
		PARSER_ERR("prolog");
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
		return err;
	}
	if ((err = compare_token(parser, TOKEN_SE_DOT)) != ERR_OK) {
		return err;
	}
	if ((err = compare_token(parser, TOKEN_OP_MUL)) != ERR_OK) {
		return err;
	}
	if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
		return err;
	}

	AstNode **functions = NULL;
	int func_count = 0;
	AstNode **extensions = NULL;
	int ext_count = 0;

	if ((err = f_list(parser, &functions, &func_count, &extensions, &ext_count)) != ERR_OK) {
		goto fail;
	}
	if ((err = compare_token(parser, TOKEN_EOF)) != ERR_OK) {
		goto fail;
	}

	/* a function annotated with @main is required */
	bool has_main = false;
	for (int i = 0; i < func_count; i++) {
		if (functions[i]->ast.func_node.is_main) {
			has_main = true;
		}
	}
	if (!has_main) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] missing function annotated with @main\n");
		err = ERR_SEMANTIC_UNDEFINED;
		goto fail;
	}

	*out_program_node = ast_new_program(functions, func_count, extensions, ext_count, 1, 1);
	if (*out_program_node == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}
	return ERR_OK;

fail:
	free_nodes(functions, func_count);
	free_nodes(extensions, ext_count);
	return err;
}

/* =============================================================================================================== */
/*  F_LIST (rules 2, 3, 4)                                                                                         */
/* =============================================================================================================== */
ErrorCode f_list(Parser *parser, AstNode ***functions, int *func_count, AstNode ***extensions, int *ext_count) {
	ErrorCode err;

	for (;;) {
		TokenType t = parser->current_token.type;
		AstNode *node = NULL;

		if (is_f_def(t)) {
			if ((err = f_def(parser, &node)) != ERR_OK) {
				return err;
			}
			if ((err = push_node(functions, func_count, node)) != ERR_OK) {
				return err;
			}
		} else if (t == TOKEN_KW_EXTENSION) {
			if ((err = compare_token(parser, TOKEN_KW_EXTENSION)) != ERR_OK) {
				return err;
			}
			if ((err = extension(parser, &node)) != ERR_OK) {
				return err;
			}
			if ((err = push_node(extensions, ext_count, node)) != ERR_OK) {
				return err;
			}
		} else {
			return ERR_OK; /* rule 4: epsilon */
		}
	}
}

/* =============================================================================================================== */
/*  F_DEF (rule 5) and shared function tail                                                                        */
/* =============================================================================================================== */

/*
 * Parses  <PARAM_LISTS> <D_TYPE> = <F_BODY_END>  in a new scope and builds the NODE_FUNC_DEF.
 * Takes ownership of `name` (freed on failure, stored in the node on success).
 * If reg_scope is given, the function symbol in it gets its parameter info before the body is parsed
 * (so recursion type-checks).
 */
static ErrorCode func_rest(Parser *parser, char *name, bool is_main, SymTable *reg_scope, const char *sym_name,
                           bool insert_sym, int line, int col, AstNode **out) {
	ErrorCode err;
	SymTable scope;
	AstNode **params = NULL;
	int param_count = 0;
	int *list_sizes = NULL;
	int list_count = 0;
	AstNode *body = NULL;
	AstNode *fn = NULL;
	DataType ret_type = TYPE_UNKNOWN;
	DataType saved_ret = parser->current_ret_type;

	scope_enter(parser, &scope);

	if ((err = param_lists(parser, &params, &param_count, &list_sizes, &list_count)) != ERR_OK) {
		goto fail;
	}
	if (is_main && param_count > 0) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] @main function must not have parameters\n");
		err = ERR_SEMANTIC_FUNCTION_CALL;
		goto fail;
	}
	if ((err = data_type(parser, &ret_type)) != ERR_OK) {
		goto fail;
	}

	if (reg_scope != NULL) {
		const char *key = sym_name != NULL ? sym_name : name;
		if (insert_sym && (err = symtable_insert(reg_scope, key, SYMBOL_FUNC, ret_type, 0)) != ERR_OK) {
			goto fail;
		}
		Symbol *fs = symtable_lookup_local(reg_scope, key);
		if (fs != NULL) {
			fs->type = ret_type;
			fs->param_count = param_count;
			if (param_count > 0) {
				fs->param_types = malloc((size_t)param_count * sizeof(DataType));
				if (fs->param_types == NULL) {
					err = ERR_INTERNAL_COMPILER;
					goto fail;
				}
				for (int i = 0; i < param_count; i++) {
					fs->param_types[i] = params[i]->ast.param_node.type;
				}
			}
		}
	}

	if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
		goto fail;
	}

	parser->current_ret_type = ret_type;
	if ((err = f_body_end(parser, &body)) != ERR_OK) {
		goto fail;
	}

	fn = ast_new_func_def(name, params, param_count, list_sizes, list_count, list_count > 0, ret_type, body, is_main,
	                      line, col);
	if (fn == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}

	parser->current_ret_type = saved_ret;
	scope_leave(parser, &scope);
	*out = fn;
	return ERR_OK;

fail:
	parser->current_ret_type = saved_ret;
	scope_leave(parser, &scope);
	free_nodes(params, param_count);
	free(list_sizes);
	ast_free(body);
	free(name);
	return err;
}

ErrorCode f_def(Parser *parser, AstNode **func_node) {
	ErrorCode err;
	bool is_main = false;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	if ((err = anotacia(parser, &is_main)) != ERR_OK) {
		return err;
	}
	if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("f_def");
	}

	char *func_name = strdup(parser->current_token.value.string_value);
	if (func_name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if (symtable_lookup_local(parser->current_scope, func_name) != NULL) {
		free(func_name);
		return ERR_SEMANTIC_REDEFINITION;
	}
	if ((err = symtable_insert(parser->current_scope, func_name, SYMBOL_FUNC, TYPE_UNKNOWN, 0)) != ERR_OK) {
		free(func_name);
		return err;
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
		free(func_name);
		return err;
	}

	return func_rest(parser, func_name, is_main, parser->current_scope, NULL, false, line, col, func_node);
}

/* =============================================================================================================== */
/*  ANOTACIA (rules 10, 11)                                                                                        */
/* =============================================================================================================== */
ErrorCode anotacia(Parser *parser, bool *is_main) {
	ErrorCode err;
	*is_main = false;

	if (parser->current_token.type != TOKEN_SE_AT) {
		return ERR_OK; /* rule 11: epsilon */
	}
	if ((err = parser_next_token(parser)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT || strcmp(parser->current_token.value.string_value, "main") != 0) {
		PARSER_ERR("anotacia");
	}
	if ((err = parser_next_token(parser)) != ERR_OK) {
		return err;
	}
	if ((err = new_line(parser)) != ERR_OK) {
		return err;
	}
	while (parser->current_token.type == TOKEN_EOL) {
		if ((err = parser_next_token(parser)) != ERR_OK) {
			return err;
		}
	}
	*is_main = true;
	return ERR_OK;
}

/* =============================================================================================================== */
/*  EXTENSION (rule 14)                                                                                            */
/* =============================================================================================================== */
ErrorCode extension(Parser *parser, AstNode **ext_node) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	DataType recv_type = TYPE_UNKNOWN;
	char *ext_name = NULL;
	AstNode **methods = NULL;
	int method_count = 0, method_capacity = 0;
	SymTable scope;
	bool in_scope = false;

	if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("extension");
	}
	ext_name = strdup(parser->current_token.value.string_value);
	if (ext_name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK || (err = type(parser, &recv_type)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
		goto fail;
	}

	scope_enter(parser, &scope);
	in_scope = true;
	/* the receiver is visible in the methods like a read-only parameter */
	if ((err = symtable_insert(parser->current_scope, ext_name, SYMBOL_VAR, recv_type, 0)) != ERR_OK) {
		goto fail;
	}

	parser->ext_recv_type = recv_type;
	err = compare_token(parser, TOKEN_INDENT);
	if (err == ERR_OK) {
		err = e_body(parser, &methods, &method_count, &method_capacity);
	}
	parser->ext_recv_type = TYPE_UNKNOWN;
	if (err != ERR_OK || (err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
		goto fail;
	}

	*ext_node = ast_new_extension(ext_name, recv_type, methods, method_count, line, col);
	if (*ext_node == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}
	scope_leave(parser, &scope);
	return ERR_OK;

fail:
	if (in_scope) {
		scope_leave(parser, &scope);
	}
	free_nodes(methods, method_count);
	free(ext_name);
	return err;
}

/* =============================================================================================================== */
/*  E_BODY / EB_NEXT (rules 15, 16, 17)                                                                            */
/* =============================================================================================================== */
ErrorCode e_body(Parser *parser, AstNode ***methods, int *count, int *capacity) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("e_body");
	}
	char *name = strdup(parser->current_token.value.string_value);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
		free(name);
		return err;
	}

	/* the method is registered in the global scope under "<ReceiverType>.<name>" */
	char key[256];
	mangle_method(key, sizeof key, parser->ext_recv_type, name);
	SymTable *global = parser->current_scope;
	while (global->parent != NULL) {
		global = global->parent;
	}

	AstNode *fn = NULL;
	if ((err = func_rest(parser, name, false, global, key, true, line, col, &fn)) != ERR_OK) {
		return err;
	}
	if ((err = push_node(methods, count, fn)) != ERR_OK) {
		return err;
	}
	*capacity = *count;

	return eb_next(parser, methods, count, capacity);
}

ErrorCode eb_next(Parser *parser, AstNode ***methods, int *count, int *capacity) {
	if (parser->current_token.type == TOKEN_KW_DEF) {
		return e_body(parser, methods, count, capacity); /* rule 16 */
	}
	return ERR_OK; /* rule 17 */
}

/* =============================================================================================================== */
/*  TYPE (rules 20-24), DATA_TYPE (rules 18, 19)                                                                   */
/* =============================================================================================================== */
ErrorCode type(Parser *parser, DataType *out_type) {
	switch (parser->current_token.type) {
	case TOKEN_KW_INT:
		*out_type = TYPE_INT;
		break;
	case TOKEN_KW_DOUBLE:
		*out_type = TYPE_DOUBLE;
		break;
	case TOKEN_KW_STRING:
		*out_type = TYPE_STRING;
		break;
	case TOKEN_KW_UNIT:
		*out_type = TYPE_UNIT;
		break;
	case TOKEN_KW_BOOLEAN:
		*out_type = TYPE_BOOLEAN;
		break;
	default:
		PARSER_ERR("type");
	}
	return parser_next_token(parser);
}

ErrorCode type_func(Parser *parser, DataType *out_type) { return type(parser, out_type); }

ErrorCode data_type(Parser *parser, DataType *out_type) {
	ErrorCode err;
	*out_type = TYPE_UNKNOWN;

	if (parser->current_token.type == TOKEN_SE_COLON) {
		if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
			return err;
		}
		return type(parser, out_type);
	}
	return ERR_OK; /* rule 19 */
}

/* =============================================================================================================== */
/*  PARAMETERS (rules 6, 7, 25 - 30)                                                                               */
/* =============================================================================================================== */

/* id : <TYPE>  -> adds the parameter to the scope and to the array (keeps source order) */
static ErrorCode param_decl(Parser *parser, AstNode ***params, int *param_count) {
	ErrorCode err;
	DataType p_type;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	char *name = strdup(parser->current_token.value.string_value);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK || (err = type(parser, &p_type)) != ERR_OK ||
	    (err = symtable_insert(parser->current_scope, name, SYMBOL_VAR, p_type, 0)) != ERR_OK) {
		free(name);
		return err;
	}

	AstNode *node = ast_new_param(name, p_type, line, col); /* takes the name */
	if (node == NULL) {
		free(name);
		return ERR_INTERNAL_COMPILER;
	}
	return push_node(params, param_count, node);
}

ErrorCode parameter(Parser *parser, AstNode ***params, int *param_count) {
	ErrorCode err;

	if (parser->current_token.type == TOKEN_ID_IDENT) { /* rule 25 */
		if ((err = param_decl(parser, params, param_count)) != ERR_OK) {
			return err;
		}
		return param_next(parser, params, param_count);
	}
	return ERR_OK; /* rule 26 */
}

ErrorCode param_next(Parser *parser, AstNode ***params, int *param_count) {
	ErrorCode err;

	if (parser->current_token.type == TOKEN_SE_COMMA) { /* rule 27 */
		if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK) {
			return err;
		}
		return param_tail(parser, params, param_count);
	}
	return ERR_OK; /* rule 28 */
}

ErrorCode param_tail(Parser *parser, AstNode ***params, int *param_count) {
	ErrorCode err;

	if (parser->current_token.type == TOKEN_ID_IDENT) { /* rule 29 */
		if ((err = param_decl(parser, params, param_count)) != ERR_OK) {
			return err;
		}
		return param_next(parser, params, param_count);
	}
	return ERR_OK; /* rule 30 */
}

ErrorCode param_lists(Parser *parser, AstNode ***params, int *param_count, int **list_sizes, int *list_count) {
	ErrorCode err;

	while (parser->current_token.type == TOKEN_SE_LPAREN) { /* rule 6 */
		if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK) {
			return err;
		}
		int before = *param_count;
		if ((err = parameter(parser, params, param_count)) != ERR_OK) {
			return err;
		}
		if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK) {
			return err;
		}
		int *sizes = realloc(*list_sizes, (size_t)(*list_count + 1) * sizeof(int));
		if (sizes == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		*list_sizes = sizes;
		sizes[(*list_count)++] = *param_count - before;
	}
	return ERR_OK; /* rule 7 */
}

/* =============================================================================================================== */
/*  F_BODY_END (rules 8, 9), F_BODY (rule 31), NEW_LINE (rules 12, 13)                                             */
/* =============================================================================================================== */
/*
 * A function with a non-Unit return type must end with a value on every path:
 * the last statement is a `return`, an expression (it becomes an implicit NODE_RETURN
 * after its type is checked against the return type) or an if/else whose branches do so.
 */
static ErrorCode finalize_value(AstNode *blk, DataType ret) {
	if (ret == TYPE_UNIT || ret == TYPE_UNKNOWN || blk == NULL || blk->kind != NODE_BLOCK) {
		return ERR_OK;
	}
	int cnt = blk->ast.block_node.block.count;
	AstNode *last = cnt > 0 ? blk->ast.block_node.block.statements[cnt - 1] : NULL;
	if (last == NULL) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] function body without a value\n");
		return ERR_SEMANTIC_COMPATIBILITY;
	}

	switch (last->kind) {
	case NODE_RETURN:
		return ERR_OK;
	case NODE_EXPR_STMT: {
		AstNode *e = last->ast.expr_stmt.child;
		if (!is_type_compatible(ret, e->type)) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] Return type mismatch: expected=%d, got=%d\n", ret, e->type);
			return ERR_SEMANTIC_COMPATIBILITY;
		}
		last->kind = NODE_RETURN; /* same payload (child expression) */
		last->type = e->type;
		return ERR_OK;
	}
	case NODE_IF_STMT: {
		ErrorCode err;
		if (last->ast.if_node.else_branch == NULL) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] if without else at the end of a function returning a value\n");
			return ERR_SEMANTIC_COMPATIBILITY;
		}
		if ((err = finalize_value(last->ast.if_node.then_branch, ret)) != ERR_OK) {
			return err;
		}
		return finalize_value(last->ast.if_node.else_branch, ret);
	}
	default:
		fprintf(stderr, "\n[SEMANTIC DEBUG] function returning a value must end with a value\n");
		return ERR_SEMANTIC_COMPATIBILITY;
	}
}

ErrorCode f_body_end(Parser *parser, AstNode **body_node) {
	ErrorCode err;
	*body_node = NULL;

	while (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
	}

	if (parser->current_token.type == TOKEN_INDENT) {
		/* rule 8: INDENT <F_BODY> DEDENT (statements share the function scope with the parameters) */
		if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
			return err;
		}
		if ((err = f_body(parser, body_node)) != ERR_OK) {
			return err;
		}
		if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK ||
		    (err = finalize_value(*body_node, parser->current_ret_type)) != ERR_OK) {
			ast_free(*body_node);
			*body_node = NULL;
			return err;
		}
		return ERR_OK;
	}

	/* rule 9: single expression body, its value is the return value */
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	AstNode *expr = NULL;
	DataType expr_type = TYPE_UNKNOWN;

	if ((err = parse_expr(parser, &expr, &expr_type)) != ERR_OK) {
		return err;
	}
	if (!is_type_compatible(parser->current_ret_type, expr_type)) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] Return type mismatch: expected=%d, got=%d\n", parser->current_ret_type,
		        expr_type);
		ast_free(expr);
		return ERR_SEMANTIC_COMPATIBILITY;
	}

	AstNode *ret = ast_new_return(expr, line, col);
	if (ret == NULL) {
		ast_free(expr);
		return ERR_INTERNAL_COMPILER;
	}
	AstNode **stmts = malloc(sizeof(AstNode *));
	if (stmts == NULL) {
		ast_free(ret);
		return ERR_INTERNAL_COMPILER;
	}
	stmts[0] = ret;
	*body_node = ast_new_block(stmts, 1, 1, line, col);
	if (*body_node == NULL) {
		free_nodes(stmts, 1);
		return ERR_INTERNAL_COMPILER;
	}

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			ast_free(*body_node);
			*body_node = NULL;
			return err;
		}
	}
	return ERR_OK;
}

ErrorCode f_body(Parser *parser, AstNode **body_node) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	AstNode **statements = NULL;
	int count = 0;
	AstNode *first = NULL;

	if ((err = stmt(parser, &first)) != ERR_OK) {
		return err;
	}
	if (first != NULL && (err = push_node(&statements, &count, first)) != ERR_OK) {
		return err;
	}
	if ((err = stmt_next(parser, &statements, &count)) != ERR_OK) {
		free_nodes(statements, count);
		return err;
	}

	*body_node = ast_new_block(statements, count, count, line, col);
	if (*body_node == NULL) {
		free_nodes(statements, count);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

ErrorCode new_line(Parser *parser) {
	if (parser->current_token.type == TOKEN_EOL) {
		return compare_token(parser, TOKEN_EOL); /* rule 12 */
	}
	return ERR_OK; /* rule 13 */
}

/* =============================================================================================================== */
/*  RET_VAL (rules 41, 42)                                                                                         */
/* =============================================================================================================== */
ErrorCode ret_val(Parser *parser, DataType expected_ret_type, AstNode **return_node) {
	ErrorCode err;
	DataType expr_type = TYPE_UNIT;
	AstNode *expr_node = NULL;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	if (is_expr_start(parser->current_token.type)) {
		/* rule 41 */
		if ((err = parse_expr(parser, &expr_node, &expr_type)) != ERR_OK) {
			return err;
		}
		if (!is_type_compatible(expected_ret_type, expr_type)) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] Return type mismatch: expected=%d, got=%d\n", expected_ret_type,
			        expr_type);
			ast_free(expr_node);
			return ERR_SEMANTIC_COMPATIBILITY;
		}
	} else if (expected_ret_type != TYPE_UNIT && expected_ret_type != TYPE_UNKNOWN) {
		return ERR_SEMANTIC_COMPATIBILITY;
	}
	/* rule 42: epsilon */

	*return_node = ast_new_return(expr_node, line, col);
	if (*return_node == NULL) {
		ast_free(expr_node);
		return ERR_INTERNAL_COMPILER;
	}
	(*return_node)->type = expr_type;
	return ERR_OK;
}

/* =============================================================================================================== */
/*  STMT_NEXT (rules 32, 33, 34)                                                                                   */
/* =============================================================================================================== */
ErrorCode stmt_next(Parser *parser, AstNode ***statements_out, int *count_out) {
	ErrorCode err;

	for (;;) {
		TokenType t = parser->current_token.type;

		if (t == TOKEN_EOL || t == TOKEN_SE_SEMICOLON) { /* rules 32, 33 */
			if ((err = compare_token(parser, t)) != ERR_OK) {
				return err;
			}
			/* a separator may end the block (EOL before DEDENT / EOF) */
			if (!is_stmt_keyword_start(parser->current_token.type) && !is_expr_start(parser->current_token.type)) {
				return ERR_OK;
			}
		} else if (!is_stmt_keyword_start(t)) {
			return ERR_OK; /* rule 34 */
		}
		/* else: statement directly after a block (no separator needed) */

		AstNode *next = NULL;
		if ((err = stmt(parser, &next)) != ERR_OK) {
			return err;
		}
		if (next != NULL && (err = push_node(statements_out, count_out, next)) != ERR_OK) {
			return err;
		}
	}
}

/* =============================================================================================================== */
/*  STMT (rules 35 - 40, 48, 57)                                                                                   */
/* =============================================================================================================== */

/* val / var id [: T] = expr */
static ErrorCode stmt_def(Parser *parser, AstNode **out) {
	ErrorCode err;
	bool is_var = (parser->current_token.type == TOKEN_KW_VAR);
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	DataType declared = TYPE_UNKNOWN;
	DataType expr_type = TYPE_UNKNOWN;
	AstNode *init = NULL;

	if ((err = parser_next_token(parser)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("stmt");
	}

	char *name = strdup(parser->current_token.value.string_value);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if (symtable_lookup_local(parser->current_scope, name) != NULL) {
		free(name);
		return ERR_SEMANTIC_REDEFINITION;
	}
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK || (err = data_type(parser, &declared)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK ||
	    (err = parse_expr(parser, &init, &expr_type)) != ERR_OK) {
		free(name);
		return err;
	}

	if (declared != TYPE_UNKNOWN && !is_type_compatible(declared, expr_type)) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] Type mismatch in declaration: declared=%d, expr=%d\n", declared, expr_type);
		free(name);
		ast_free(init);
		return ERR_SEMANTIC_COMPATIBILITY;
	}

	DataType final_type = (declared != TYPE_UNKNOWN) ? declared : expr_type;
	/* the variable becomes visible only after its initializer */
	if ((err = symtable_insert(parser->current_scope, name, is_var ? SYMBOL_VAR : SYMBOL_VAL, final_type,
	                           is_var ? 1 : 0)) != ERR_OK) {
		free(name);
		ast_free(init);
		return err;
	}

	AstNode *node =
	    is_var ? ast_new_var_def(name, declared, init, line, col) : ast_new_val_def(name, declared, init, line, col);
	if (node == NULL) {
		free(name);
		ast_free(init);
		return ERR_INTERNAL_COMPILER;
	}
	node->type = final_type;
	*out = node;
	return ERR_OK;
}

/* while <EXP> do EOL INDENT <F_BODY> DEDENT */
static ErrorCode stmt_while(Parser *parser, AstNode **out) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	DataType cond_type;
	AstNode *cond = NULL;
	AstNode *body = NULL;

	if ((err = compare_token(parser, TOKEN_KW_WHILE)) != ERR_OK) {
		return err;
	}
	if ((err = parse_expr(parser, &cond, &cond_type)) != ERR_OK) {
		return err;
	}
	if (!bool_like(cond_type)) {
		ast_free(cond);
		return ERR_SEMANTIC_TYPE;
	}
	if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK || (err = compare_token(parser, TOKEN_EOL)) != ERR_OK ||
	    (err = indented_block(parser, &body)) != ERR_OK) {
		ast_free(cond);
		return err;
	}

	*out = ast_new_while(cond, body, line, col);
	if (*out == NULL) {
		ast_free(cond);
		ast_free(body);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

/* expression statement or assignment  id = expr */
static ErrorCode stmt_expr_or_assign(Parser *parser, AstNode **out) {
	ErrorCode err;
	AstNode *lhs = NULL;
	DataType lhs_type = TYPE_UNKNOWN;

	if ((err = parse_expr(parser, &lhs, &lhs_type)) != ERR_OK) {
		return err;
	}

	if (parser->current_token.type != TOKEN_OP_ASSIGN) {
		AstNode *n = ast_new_expr_stmt(lhs, lhs->line, lhs->column);
		if (n == NULL) {
			ast_free(lhs);
			return ERR_INTERNAL_COMPILER;
		}
		*out = n;
		return ERR_OK;
	}

	/* assignment: left side must be a plain variable */
	if (lhs->kind != NODE_ID) {
		ast_free(lhs);
		PARSER_ERR("stmt");
	}
	int line = lhs->line;
	int col = lhs->column;
	char *name = strdup(lhs->ast.identifier.name);
	ast_free(lhs);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}

	AstNode *rhs = NULL;
	DataType rhs_type = TYPE_UNKNOWN;
	if ((err = check_variable_assignment(parser->current_scope, name)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK ||
	    (err = parse_expr(parser, &rhs, &rhs_type)) != ERR_OK) {
		free(name);
		return err;
	}
	if (!is_type_compatible(lhs_type, rhs_type)) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] Type mismatch in assignment: left=%d, rhs=%d\n", lhs_type, rhs_type);
		free(name);
		ast_free(rhs);
		return ERR_SEMANTIC_COMPATIBILITY;
	}

	*out = ast_new_assign(name, rhs, line, col); /* takes the name */
	if (*out == NULL) {
		free(name);
		ast_free(rhs);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

ErrorCode stmt(Parser *parser, AstNode **first_stmt) {
	ErrorCode err;
	TokenType t = parser->current_token.type;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	*first_stmt = NULL;

	if (t == TOKEN_KW_VAL || t == TOKEN_KW_VAR) {
		err = stmt_def(parser, first_stmt); /* rules 35, 36 */
	} else if (t == TOKEN_KW_WHILE) {
		err = stmt_while(parser, first_stmt); /* rule 37 */
	} else if (t == TOKEN_KW_RETURN) {
		if ((err = compare_token(parser, TOKEN_KW_RETURN)) == ERR_OK) { /* rule 38 */
			err = ret_val(parser, parser->current_ret_type, first_stmt);
		}
	} else if (t == TOKEN_KW_IF) {
		err = if_stmt_all(parser, first_stmt); /* rule 48 */
	} else if (t == TOKEN_KW_FOR) {
		err = for_stmt(parser, first_stmt); /* rule 57 */
	} else {
		err = stmt_expr_or_assign(parser, first_stmt); /* rules 39, 40 */
	}

	/* statements carry the position of their first token */
	if (err == ERR_OK && *first_stmt != NULL) {
		(*first_stmt)->line = line;
		(*first_stmt)->column = col;
	}
	return err;
}

/* =============================================================================================================== */
/*  IF STATEMENT (rules 49 - 53)                                                                                   */
/* =============================================================================================================== */
ErrorCode if_stmt_all(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	DataType cond_type;
	AstNode *cond = NULL;
	AstNode *then_body = NULL;
	AstNode *else_body = NULL;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
			return err;
		}
	}

	if ((err = parse_expr(parser, &cond, &cond_type)) != ERR_OK) {
		return err;
	}
	if (!bool_like(cond_type)) {
		ast_free(cond);
		return ERR_SEMANTIC_TYPE;
	}

	if (parser->current_token.type == TOKEN_KW_THEN) {
		if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
			ast_free(cond);
			return err;
		}
	}

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			ast_free(cond);
			return err;
		}
		err = (parser->current_token.type == TOKEN_INDENT) ? indented_block(parser, &then_body)
		                                                   : scoped_stmt(parser, &then_body);
	} else {
		err = scoped_stmt(parser, &then_body);
	}
	if (err != ERR_OK) {
		ast_free(cond);
		return err;
	}

	if ((err = if_next(parser, &else_body)) != ERR_OK) {
		ast_free(cond);
		ast_free(then_body);
		return err;
	}

	*out_node = ast_new_if(cond, then_body, else_body, line, col);
	if (*out_node == NULL) {
		ast_free(cond);
		ast_free(then_body);
		ast_free(else_body);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

ErrorCode if_next(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_KW_ELSE) { /* rule 50 */
		if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK) {
			return err;
		}
		return else_next(parser, out_node);
	}
	return ERR_OK; /* rule 51 */
}

ErrorCode else_next(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_KW_IF) { /* rule 52 */
		AstNode *chained = NULL;
		if ((err = if_stmt_all(parser, &chained)) != ERR_OK) {
			return err;
		}
		return wrap_in_block(chained, out_node);
	}
	if (parser->current_token.type == TOKEN_EOL) { /* rule 53 */
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
		if (parser->current_token.type == TOKEN_INDENT) {
			return indented_block(parser, out_node);
		}
	}
	return scoped_stmt(parser, out_node);
}

/* =============================================================================================================== */
/*  IF EXPRESSION (rules 54 - 56) - else is mandatory                                                              */
/* =============================================================================================================== */
ErrorCode if_control(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	DataType cond_type;
	AstNode *cond = NULL;
	AstNode *then_body = NULL;
	AstNode *else_body = NULL;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
			return err;
		}
	}

	if ((err = parse_expr(parser, &cond, &cond_type)) != ERR_OK) {
		return err;
	}
	if (!bool_like(cond_type)) {
		ast_free(cond);
		return ERR_SEMANTIC_TYPE;
	}
	if (parser->current_token.type == TOKEN_KW_THEN) {
		if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
			ast_free(cond);
			return err;
		}
	}
	if ((err = if_control_next(parser, &then_body)) != ERR_OK) {
		ast_free(cond);
		return err;
	}
	if (parser->current_token.type != TOKEN_KW_ELSE) {
		ast_free(cond);
		ast_free(then_body);
		PARSER_ERR("if_control");
	}
	if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK ||
	    (err = if_control_next(parser, &else_body)) != ERR_OK) {
		ast_free(cond);
		ast_free(then_body);
		return err;
	}

	*out_node = ast_new_if_expr(cond, then_body, else_body, line, col);
	if (*out_node == NULL) {
		ast_free(cond);
		ast_free(then_body);
		ast_free(else_body);
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

ErrorCode if_control_next(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	DataType dummy;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_EOL) { /* rule 56 */
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
		if (parser->current_token.type != TOKEN_INDENT) {
			PARSER_ERR("if_control_next");
		}
		return indented_block(parser, out_node);
	}
	return parse_expr(parser, out_node, &dummy); /* rule 55 */
}

/* =============================================================================================================== */
/*  FOR (rules 58 - 71)                                                                                            */
/* =============================================================================================================== */
ErrorCode for_stmt(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	SymTable scope;
	*out_node = NULL;

	if ((err = compare_token(parser, TOKEN_KW_FOR)) != ERR_OK) {
		return err;
	}
	scope_enter(parser, &scope); /* the loop variable lives in its own scope */
	err = for_content(parser, out_node);
	scope_leave(parser, &scope);
	return err;
}

ErrorCode for_content(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	char *var_name = NULL;
	AstNode *from_node = NULL;
	AstNode *to_node = NULL;
	AstNode *step_node = NULL;
	bool is_until = false;
	AstNode **guards = NULL;
	int guard_count = 0;
	AstNode *body_node = NULL;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_EOL) {
		/* rule 60: EOL INDENT <GENERATOR> <EOL_GUARDS> DEDENT do <FOR_BODY> */
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK ||
		    (err = compare_token(parser, TOKEN_INDENT)) != ERR_OK ||
		    (err = generator(parser, &var_name, &from_node, &to_node, &is_until, &step_node)) != ERR_OK) {
			goto fail;
		}
		if ((err = eol_guards(parser, &guards, &guard_count)) != ERR_OK ||
		    (err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
			goto fail;
		}
	} else {
		/* rule 59: <GENERATOR> <OPT_GUARDS> do <FOR_BODY> */
		if ((err = generator(parser, &var_name, &from_node, &to_node, &is_until, &step_node)) != ERR_OK ||
		    (err = opt_guards(parser, &guards, &guard_count)) != ERR_OK) {
			goto fail;
		}
	}

	if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK || (err = for_body(parser, &body_node)) != ERR_OK) {
		goto fail;
	}

	/* the node takes over var_name and guards */
	*out_node =
	    ast_new_for(var_name, from_node, to_node, is_until, step_node, guards, guard_count, body_node, line, col);
	if (*out_node == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}
	return ERR_OK;

fail:
	free(var_name);
	ast_free(from_node);
	ast_free(to_node);
	ast_free(step_node);
	free_nodes(guards, guard_count);
	ast_free(body_node);
	return err;
}

/* rule 61: id <- <EXP> <RANGE> <EXP> <OPT_STEP> */
ErrorCode generator(Parser *parser, char **out_var_name, AstNode **out_from, AstNode **out_to, bool *out_is_until,
                    AstNode **out_step) {
	ErrorCode err;
	DataType t;
	char *name = NULL;
	AstNode *from = NULL;
	AstNode *to = NULL;
	AstNode *step = NULL;
	bool is_until = false;
	*out_var_name = NULL;
	*out_from = NULL;
	*out_to = NULL;
	*out_step = NULL;
	*out_is_until = false;

	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("generator");
	}
	name = strdup(parser->current_token.value.string_value);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}

	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK ||
	    (err = compare_token(parser, TOKEN_SE_LARROW)) != ERR_OK || (err = parse_expr(parser, &from, &t)) != ERR_OK) {
		goto fail;
	}
	if (t != TYPE_INT && t != TYPE_UNKNOWN) {
		err = ERR_SEMANTIC_TYPE;
		goto fail;
	}
	if ((err = range_func(parser, &is_until)) != ERR_OK || (err = parse_expr(parser, &to, &t)) != ERR_OK) {
		goto fail;
	}
	if (t != TYPE_INT && t != TYPE_UNKNOWN) {
		err = ERR_SEMANTIC_TYPE;
		goto fail;
	}
	if ((err = opt_step(parser, &step)) != ERR_OK) {
		goto fail;
	}
	/* the loop variable is visible in guards and body only, not in its own bounds */
	if ((err = symtable_insert(parser->current_scope, name, SYMBOL_VAL, TYPE_INT, 0)) != ERR_OK) {
		goto fail;
	}

	*out_var_name = name;
	*out_from = from;
	*out_to = to;
	*out_step = step;
	*out_is_until = is_until;
	return ERR_OK;

fail:
	free(name);
	ast_free(from);
	ast_free(to);
	ast_free(step);
	return err;
}

ErrorCode range_func(Parser *parser, bool *is_until) {
	if (parser->current_token.type == TOKEN_KW_TO) { /* rule 62 */
		*is_until = false;
		return compare_token(parser, TOKEN_KW_TO);
	}
	if (parser->current_token.type == TOKEN_KW_UNTIL) { /* rule 63 */
		*is_until = true;
		return compare_token(parser, TOKEN_KW_UNTIL);
	}
	PARSER_ERR("range_func");
}

ErrorCode opt_step(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	DataType step_type;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_KW_BY) { /* rule 64 */
		if ((err = compare_token(parser, TOKEN_KW_BY)) != ERR_OK) {
			return err;
		}
		if ((err = parse_expr(parser, out_node, &step_type)) != ERR_OK) {
			return err;
		}
		if (step_type != TYPE_INT && step_type != TYPE_UNKNOWN) {
			ast_free(*out_node);
			*out_node = NULL;
			return ERR_SEMANTIC_TYPE;
		}
	}
	return ERR_OK; /* rule 65 */
}

/* parses `<EXP>` of a guard (the `if` was already consumed) and appends it */
static ErrorCode parse_guard(Parser *parser, AstNode ***guards, int *count) {
	ErrorCode err;
	DataType t;
	AstNode *cond = NULL;

	if ((err = parse_expr(parser, &cond, &t)) != ERR_OK) {
		return err;
	}
	if (!bool_like(t)) {
		ast_free(cond);
		return ERR_SEMANTIC_TYPE;
	}
	return push_node(guards, count, cond);
}

ErrorCode opt_guards(Parser *parser, AstNode ***guards_out, int *count_out) {
	ErrorCode err = ERR_OK;
	*guards_out = NULL;
	*count_out = 0;

	while (parser->current_token.type == TOKEN_KW_IF) { /* rules 66, 67 */
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK ||
		    (err = parse_guard(parser, guards_out, count_out)) != ERR_OK) {
			free_nodes(*guards_out, *count_out);
			*guards_out = NULL;
			*count_out = 0;
			return err;
		}
	}
	return ERR_OK;
}

ErrorCode eol_guards(Parser *parser, AstNode ***guards_out, int *count_out) {
	ErrorCode err = ERR_OK;
	*guards_out = NULL;
	*count_out = 0;

	while (parser->current_token.type == TOKEN_EOL) { /* rules 68, 69 */
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			goto fail;
		}
		if (parser->current_token.type != TOKEN_KW_IF) {
			break;
		}
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK ||
		    (err = parse_guard(parser, guards_out, count_out)) != ERR_OK) {
			goto fail;
		}
	}
	return ERR_OK;

fail:
	free_nodes(*guards_out, *count_out);
	*guards_out = NULL;
	*count_out = 0;
	return err;
}

ErrorCode for_body(Parser *parser, AstNode **out_node) {
	ErrorCode err;
	*out_node = NULL;

	if (parser->current_token.type == TOKEN_EOL) { /* rule 71 */
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
		if (parser->current_token.type != TOKEN_INDENT) {
			PARSER_ERR("for_body");
		}
		return indented_block(parser, out_node);
	}
	return scoped_stmt(parser, out_node); /* rule 70 */
}

/* =============================================================================================================== */
/*  FREE                                                                                                           */
/* =============================================================================================================== */
void parser_free(Parser *parser) {
	if (parser != NULL) {
		token_free(&parser->current_token);
	}
}