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

/* One function / extension method found by the pre-pass over the token buffer. */
typedef struct {
	int start;       /* index of the first token (the '@' of @main or the 'def') */
	int end;         /* index of the token following the definition, valid once `node` is set */
	Symbol *sym;     /* symbol in the global table */
	AstNode *node;   /* parsed definition (set when it was parsed ahead of its position), NULL otherwise */
	bool is_method;  /* extension method */
	char *recv_name; /* extension receiver name / type (methods only) */
	DataType recv_type;
} FuncDefEntry;

typedef struct {
	Scanner *scanner;
	SymTable *current_scope;
	Token current_token;
	DataType current_ret_type;
	DataType ext_recv_type; /* receiver type while parsing an extension body, TYPE_UNKNOWN otherwise */
	bool ret_explicit;      /* current function declared its return type */
	DataType inferred_ret;  /* return type found by f_body_end for functions without a declared one */
	TokenType prev_type;    /* type of the token consumed just before current_token */
	bool single_method;     /* parse-ahead of one extension method: do not continue with its siblings */
	/* the whole program is tokenized up front so that calls can refer to functions defined later */
	Token *toks;
	int tok_count, tok_cap, tok_pos; /* tok_pos = index of the next token to be delivered */
	ErrorCode lex_err;               /* lexical error found at index tok_count (the buffer ends there) */
	FuncDefEntry *defs;
	int def_count, def_cap;
} Parser;

/* makes sure a function called before its definition has a known return type (parses it ahead if needed) */
ErrorCode resolve_function(Parser *parser, Symbol *sym);

ErrorCode f_list(Parser *parser, AstNode ***functions, int *func_count, AstNode ***extensions, int *ext_count);
ErrorCode f_def(Parser *parser, AstNode **func_node);
ErrorCode annotation(Parser *parser, bool *is_main);
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
