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
	StringBuffer buffer;      // Temporary buffer for strings, numbers, and identifiers
	int line;                 // Current line number in the source file
	int column;               // Current column number in the source file
	int end_of_line;          // Flag indicating position at the end of a line (for EOL tokens)
	IndentStack indent_stack; // Stack tracking block indentations
	int pending_dedents;      // Number of pending DEDENT tokens
	int paren_depth;          // Current parenthesis depth
	InterpType interp_state;  // Current interpolation mode
	int pending_interp_end;   // Flag for postponed TOKEN_INTERP_END
} Scanner;

/**
 * @brief Reads the next lexical token from the input.
 *
 * Uses a state machine to analyze characters, handle identifiers, keywords,
 * numeric/string literals, operators, comments, and indentation (INDENT/DEDENT).
 *
 * @param token Pointer to the Token structure where the scanned token data is stored.
 * @param scanner Pointer to the Scanner context structure.
 * @return ErrorCode ERR_OK on success, ERR_LEXICAL on lexical error or either error codes form helper functions.
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
 * Frees the scanner
 * Frees the string buffer
 *
 * @param scanner structure
 */
void scanner_free(Scanner *scanner);
#endif