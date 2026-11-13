/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "error.h"
#include "indent_stack.h"
#include "string_buffer.h"
#include "token.h"

typedef enum { INTERP_NONE, INTERP_TEXT_SINGLE, INTERP_TEXT_MULTI, INTERP_EXPR_SINGLE, INTERP_EXPR_MULTI } InterpType;

typedef struct {
	StringBuffer buffer;
	int line;
	int column;
	int end_of_line;
	IndentStack indent_stack;
	int pending_dedents;
	int paren_depth;
	InterpType interp_state;
	int pending_interp_end;
} Scanner;

/**
 * @brief Reads the next lexical token from the input.
 *
 * Uses a state machine to analyze characters, handle identifiers, keywords,
 * numeric/string literals, operators, comments, and indentation (INDENT/DEDENT).
 *
 * @param token Pointer to the Token structure where the scanned token data is stored.
 * @param scanner Pointer to the Scanner context structure.
 * @return ErrorCode ERR_OK on success, ERR_LEXICAL on lexical error or ither error codes form helper functions.
 */
ErrorCode get_token(Token *token, Scanner *scanner); // token only valid until next call

/**
 * Initializes the scanner: sets the position to line 1, column 1 and
 * prepares the string buffer. Must be called once before get_token.
 *
 * @param scanner structure
 * @return ERR_OK if successful, ERR_INTERNAL_COMPILER if malloc fails
 */
ErrorCode scanner_init(Scanner *scanner);

/**
 * Frees the scaner
 * Frees the string buffer
 *
 * @param scanner structure
 */
void scanner_free(Scanner *scanner);
#endif