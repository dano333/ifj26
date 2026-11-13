/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: tokens description
 */

#ifndef TOKEN_H
#define TOKEN_H

#include "string_buffer.h"

typedef enum {
	/*keywords*/
	TOKEN_KW_DEF,       // def
	TOKEN_KW_DO,        // do
	TOKEN_KW_DOUBLE,    // Double
	TOKEN_KW_ELSE,      // else
	TOKEN_KW_IF,        // if
	TOKEN_KW_IMPORT,    // import
	TOKEN_KW_INT,       // Int
	TOKEN_KW_RETURN,    // return
	TOKEN_KW_STRING,    // String
	TOKEN_KW_THEN,      // then
	TOKEN_KW_UNIT,      // Unit
	TOKEN_KW_VAL,       // val
	TOKEN_KW_VAR,       // var
	TOKEN_KW_WHILE,     // while
	TOKEN_KW_BOOLEAN,   // Boolean
	TOKEN_KW_CASE,      // case
	TOKEN_KW_CLASS,     // class
	TOKEN_KW_EXTENSION, // extension
	TOKEN_KW_FOR,       // for
	TOKEN_KW_LIST,      // List
	TOKEN_KW_MATCH,     // match
	TOKEN_KW_TYPE,      // type
	TOKEN_KW_TRUE,      // true
	TOKEN_KW_FALSE,     // false
	TOKEN_KW_TO,        // to
	TOKEN_KW_UNTIL,     // until
	TOKEN_KW_BY,        // by

	/*operators*/
	TOKEN_OP_ASSIGN, // =
	TOKEN_OP_PLUS,   // +
	TOKEN_OP_MINUS,  // -
	TOKEN_OP_MUL,    // *
	TOKEN_OP_DIV,    // /
	TOKEN_OP_NOT,    // !
	TOKEN_OP_AND,    // &&
	TOKEN_OP_OR,     // ||

	/*comparing*/
	TOKEN_COM_NE, // !=
	TOKEN_COM_EQ, // ==
	TOKEN_COM_LT, // <
	TOKEN_COM_LE, // <=
	TOKEN_COM_GT, // >
	TOKEN_COM_GE, // >=

	/*end of*/
	TOKEN_EOL,    // EOL
	TOKEN_EOF,    // EOF
	TOKEN_INDENT, // tabulator section
	TOKEN_DEDENT, // end of tabulator section

	/*separators*/
	TOKEN_SE_LPAREN,    // (
	TOKEN_SE_RPAREN,    // )
	TOKEN_SE_COMMA,     // ,
	TOKEN_SE_DOT,       // .
	TOKEN_SE_COLON,     // :
	TOKEN_SE_SEMICOLON, // ;
	TOKEN_SE_AT,        // @
	TOKEN_SE_LARROW,    // <-

	/*identifiers*/
	TOKEN_ID_IDENT, // identifier

	/*literals*/
	TOKEN_LIT_INT,    // Int literal
	TOKEN_LIT_DOUBLE, // Double literal
	TOKEN_LIT_STRING, // String literal

	/*interpolateds*/
	TOKEN_INTERP_START,
	TOKEN_INTERP_TEXT,
	TOKEN_INTERP_ID,
	TOKEN_INTERP_EXPR_START,
	TOKEN_INTERP_EXPR_END,
	TOKEN_INTERP_END,

} TokenType;

typedef struct {
	TokenType type;
	union {
		int int_value;
		double double_value;
		char *string_value;
	} value;
	// position for errors
	int line;
	int column;
} Token;
/**
 * Frees heap-allocated resources inside a token (like identifier or string strings).
 */
void token_free(Token *token);
#endif