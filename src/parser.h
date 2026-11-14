/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser
 */

#ifndef PARSER_H
#define PARSER_H

#include "error.h"
#include "scanner.h"
#include "token.h"
#include "symtable.h"
#include <stdbool.h>
#include "ast.h"

typedef struct {
	Scanner *scanner;
	SymTable *current_scope;
	Token current_token;
	DataType current_ret_type;
} Parser;

ErrorCode f_list(Parser *parser, AstNode ***functions, int *func_count, AstNode ***extensions, int *ext_count);
ErrorCode f_def(Parser *parser, AstNode **func_node);
ErrorCode anotacia(Parser *parser);
ErrorCode extension(Parser *parser, AstNode **ext_node);
ErrorCode e_body(Parser *parser, AstNode ***methods, int *count, int *capacity);
ErrorCode eb_next(Parser *parser, AstNode ***methods, int *count, int *capacity);
ErrorCode parameter(Parser *parser, AstNode **param_node, AstNode ***params, int *param_count);
ErrorCode param_next(Parser *parser, AstNode ***params, int *param_count);
ErrorCode param_tail(Parser *parser, AstNode ***params, int *param_count);
ErrorCode data_type(Parser *parser, DataType *out_type);
ErrorCode f_body_end(Parser *parser, AstNode **body_node);
ErrorCode f_body(Parser *parser, AstNode **body_node);
ErrorCode new_line(Parser *parser);
ErrorCode ret_val(Parser *parser, DataType expected_ret_type, AstNode **return_node);
ErrorCode val(Parser *parser, AstNode **out_node, DataType *out_type);
ErrorCode dec(Parser *parser, AstNode **expr_node, DataType *out_type);
ErrorCode stmt_next(Parser *parser, AstNode ***statements_out, int *count_out);
ErrorCode stmt(Parser *parser, AstNode **first_stmt);
ErrorCode if_stmt_all(Parser *parser, AstNode **out_node);
ErrorCode if_next(Parser *parser, AstNode **out_node);
ErrorCode else_next(Parser *parser, AstNode **out_node);
ErrorCode if_control(Parser *parser, AstNode **out_node);
ErrorCode if_control_next(Parser *parser, AstNode **out_node);
ErrorCode for_stmt(Parser *parser, AstNode **out_node);
ErrorCode for_content(Parser *parser, AstNode **out_node);
ErrorCode generator(Parser *parser, char **out_var_name, AstNode **out_from, AstNode **out_to, bool *out_is_until, AstNode **out_step);
ErrorCode range_func(Parser *parser, bool *is_until);
ErrorCode opt_step(Parser *parser, AstNode **out_node);
ErrorCode opt_guards(Parser *parser, AstNode ***guards_out, int *count_out);
ErrorCode eol_guards(Parser *parser, AstNode ***guards_out, int *count_out);
ErrorCode for_body(Parser *parser, AstNode **out_node);
ErrorCode exp_cont(Parser *parser, DataType left_type, DataType *out_type);
ErrorCode parse_expr(Parser *parser, AstNode **out_node, DataType *out_type);
ErrorCode type_func(Parser *parser, DataType *out_type);
ErrorCode type(Parser *parser, DataType *out_type);
ErrorCode param_lists(Parser *parser, AstNode ***params, int *param_count);
ErrorCode interp_string(Parser *parser, AstNode **out_node, DataType *out_type);

/**
 * @brief Initializes the parser and prepares it for syntactic analysis.
 *
 * Links the parser with the given scanner and reads the first token
 * so the parser has data to work with from the very beginning.
 *
 * @param parser Pointer to the Parser structure to be initialized.
 * @param scanner Pointer to the already prepared and open scanner to fetch tokens from.
 * @return ErrorCode ERR_OK on success, or an error code
 */
ErrorCode parser_init(Parser *parser, Scanner *scanner, SymTable *global_table);

/**
 * @brief Parses the program prologue and extensions.
 */
ErrorCode prolog(Parser *parser, AstNode **out_program_node);

/**
 * @brief Compares the current token with the expected grammar token type.
 *
 * Checks if the current token in the parser matches the expected type. If so,
 * it automatically advances to the next token using the scanner. If it does not match,
 * it returns a syntax error.
 *
 * @param parser Pointer to the Parser structure holding the current token and scanner.
 * @return ErrorCode ERR_OK if the token matches, ERR_SYNTAX if a syntax error occurs.
 */
ErrorCode compare_token(Parser *parser, TokenType expected);



/**
 * Frees the parts in istring block
 * 
 * @param interp_free_parts structure
 */
void interp_free_parts(AstNode **parts, int count);


/**
 * Frees the parser
 * Frees the string buffer
 *
 * @param parser structure
 */
void parser_free(Parser *parser);

#endif