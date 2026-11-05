/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser
 */

#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "token.h"
#include "error.h"

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

#endif  