/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "error.h"
#include "string_buffer.h"
#include "token.h"

typedef struct {
	StringBuffer buffer;
	int line;
	int column;
	int end_of_line;

} Scanner;

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