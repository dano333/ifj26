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

typedef struct {
	Scanner *scanner;
	Token current_token;
} Parser;

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
ErrorCode parser_init(Parser *parser, Scanner *scanner);

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

/* =========================================================================
 * LL gramma/table
 * ========================================================================= */

/**
 * @brief Parses the program prologue and extensions.
 */
ErrorCode prolog(Parser *parser);
ErrorCode extension(Parser *parser);
ErrorCode e_body(Parser *parser);
ErrorCode eb_next(Parser *parser);

/**
 * @brief Parses function parameters and types.
 */
ErrorCode parameter(Parser *parser);
ErrorCode param_next(Parser *parser);
ErrorCode param_tail(Parser *parser);
ErrorCode type_func(Parser *parser);
ErrorCode data_type(Parser *parser);

/**
 * @brief Parses function body and statements.
 */
ErrorCode f_body(Parser *parser);
ErrorCode stmt(Parser *parser);
ErrorCode stmt_next(Parser *parser);

/**
 * @brief Parses expressions, values, return values and assignments.
 */
ErrorCode ret_val(Parser *parser);
ErrorCode dec(Parser *parser);
ErrorCode val(Parser *parser);
ErrorCode parse_expression(Parser *parser);
ErrorCode exp_cont(Parser *parser);

/**
 * @brief Parses conditional statements (if-else blocks and inline controls).
 */
ErrorCode if_stmt_all(Parser *parser);
ErrorCode if_next(Parser *parser);
ErrorCode else_next(Parser *parser);
ErrorCode if_control(Parser *parser);
ErrorCode if_control_next(Parser *parser);

/**
 * @brief Parses for loops, generators, ranges, steps, and guards.
 */
ErrorCode for_stmt(Parser *parser);
ErrorCode for_content(Parser *parser);
ErrorCode generator(Parser *parser);
ErrorCode range_func(Parser *parser);
ErrorCode opt_step(Parser *parser);
ErrorCode opt_guards(Parser *parser);
ErrorCode eol_guards(Parser *parser);
ErrorCode for_body(Parser *parser);

/**
 * Frees the parser
 * Frees the string buffer
 *
 * @param parser structure
 */
void parser_free(Parser *parser);

#endif