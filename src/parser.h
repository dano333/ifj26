/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser (statements, functions, blocks). Expressions live in expr.c
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "error.h"
#include "scanner.h"
#include "symtable.h"
#include "token.h"
#include <stdbool.h>

typedef struct {
	Scanner *scanner;
	SymTable *current_scope;
	Token current_token;
	DataType current_ret_type;
	DataType ext_recv_type; /* receiver type while parsing an extension body, TYPE_UNKNOWN otherwise */
} Parser;

ErrorCode f_list(Parser *parser, AstNode ***functions, int *func_count, AstNode ***extensions, int *ext_count);
ErrorCode f_def(Parser *parser, AstNode **func_node);
ErrorCode anotacia(Parser *parser, bool *is_main);
ErrorCode extension(Parser *parser, AstNode **ext_node);
ErrorCode e_body(Parser *parser, AstNode ***methods, int *count, int *capacity);
ErrorCode eb_next(Parser *parser, AstNode ***methods, int *count, int *capacity);
ErrorCode parameter(Parser *parser, AstNode ***params, int *param_count);
ErrorCode param_next(Parser *parser, AstNode ***params, int *param_count);
ErrorCode param_tail(Parser *parser, AstNode ***params, int *param_count);
ErrorCode param_lists(Parser *parser, AstNode ***params, int *param_count, int **list_sizes, int *list_count);
ErrorCode data_type(Parser *parser, DataType *out_type);
ErrorCode f_body_end(Parser *parser, AstNode **body_node);
ErrorCode f_body(Parser *parser, AstNode **body_node);
ErrorCode new_line(Parser *parser);
ErrorCode ret_val(Parser *parser, DataType expected_ret_type, AstNode **return_node);
ErrorCode stmt_next(Parser *parser, AstNode ***statements_out, int *count_out);
ErrorCode stmt(Parser *parser, AstNode **first_stmt);
ErrorCode if_stmt_all(Parser *parser, AstNode **out_node);
ErrorCode if_next(Parser *parser, AstNode **out_node);
ErrorCode else_next(Parser *parser, AstNode **out_node);
ErrorCode if_control(Parser *parser, AstNode **out_node); /* if as an expression, else is mandatory */
ErrorCode if_control_next(Parser *parser, AstNode **out_node);
ErrorCode for_stmt(Parser *parser, AstNode **out_node);
ErrorCode for_content(Parser *parser, AstNode **out_node);
ErrorCode generator(Parser *parser, char **out_var_name, AstNode **out_from, AstNode **out_to, bool *out_is_until,
                    AstNode **out_step);
ErrorCode range_func(Parser *parser, bool *is_until);
ErrorCode opt_step(Parser *parser, AstNode **out_node);
ErrorCode opt_guards(Parser *parser, AstNode ***guards_out, int *count_out);
ErrorCode eol_guards(Parser *parser, AstNode ***guards_out, int *count_out);
ErrorCode for_body(Parser *parser, AstNode **out_node);
ErrorCode type_func(Parser *parser, DataType *out_type);
ErrorCode type(Parser *parser, DataType *out_type);

/**
 * @brief Initializes the parser and reads the first token.
 */
ErrorCode parser_init(Parser *parser, Scanner *scanner, SymTable *global_table);

/**
 * @brief Parses the whole program (prologue, functions, extensions) into an AST.
 */
ErrorCode prolog(Parser *parser, AstNode **out_program_node);

/**
 * @brief Checks the current token against `expected` and advances; ERR_PARSING otherwise.
 */
ErrorCode compare_token(Parser *parser, TokenType expected);

/** Frees the parser (current token). */
void parser_free(Parser *parser);

#endif