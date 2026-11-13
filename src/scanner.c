/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#define _POSIX_C_SOURCE 200809L
#include "scanner.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	STATE_START,
	STATE_DIV,    // /
	STATE_ASSIGN, // =
	STATE_NOT,    // !
	STATE_LT,     // <
	STATE_GT,     // >
	STATE_AND,    // &
	STATE_OR,     // |
	STATE_IDENT,
	STATE_COMMENT_1,                          // //
	STATE_COMMENT_2,                          // /*
	STATE_COMMENT_3,                          // /* + another /
	STATE_COMMENT_4,                          // /**
	STATE_DOT,                                // .
	STATE_DOUBLE,                             // double
	STATE_INT_1_9,                            // int 1-9
	STATE_INT_0,                              // int 0
	STATE_DOUBLE_E,                           // double e
	STATE_DOUBLE_E_PLUS_MINUS_0_9,            // double e '+/-' 0-9
	STATE_DOUBLE_E_PLUS_MINUS,                // double e +/-
	STATE_DOUBLE_UNDERSCORE,                  // double_
	STATE_INT_1_9_UNDERSCORE,                 // int 1-9_
	STATE_DOUBLE_DOT,                         // double .
	STATE_DOUBLE_D,                           // double D
	STATE_DOUBLE_E_PLUS_MINUS_0_9_UNDERSCORE, // double e '+/-' 0-9 _
	STATE_STR_SINGLE_START,                   // "
	STATE_STR_SINGLE_ESCAPE,                  /* \ */
	STATE_STR_SINGLE_CONTENT,                 // " text
	STATE_STR_SINGLE_END,                     // " text"
	STATE_STR_EMPTY,                          // ""
	STATE_STR_MULTI_START,                    // """ text
	STATE_STR_MULTI_Q1,                       // """ text "
	STATE_STR_MULTI_Q2,                       // """ text ""
	STATE_STR_MULTI_END,                      // """ text """
	STATE_S,                                  // s
	STATE_S_ONE,                              // s"
	STATE_S_TWO,                              // s""
	STATE_SINGLE_TEXT,                        // s" text
	STATE_SINGLE_ESCAPE,                      // s" /* \ */
	STATE_SINGLE_ID,                          // s" id
	STATE_SINGLE_DOLLAR,                      // s" $
	STATE_SINGLE_EXPRESSION,                  // s" expression
	STATE_MULTI_TEXT,                         // s""" text
	STATE_MULTI_ID,                           // s""" id
	STATE_MULTI_DOLLAR,                       // s""" $
	STATE_MULTI_EXPRESSION,                   // s""" expression
	STATE_MULTI_ONE,                          // s""" "
	STATE_MULTI_TWO,                          // s""" ""

} ScannerState;

typedef struct {
	const char *str;
	TokenType type;
} Keyword;

static const Keyword keyword[] = {
    {"def", TOKEN_KW_DEF},   {"do", TOKEN_KW_DO},         {"Double", TOKEN_KW_DOUBLE},
    {"else", TOKEN_KW_ELSE}, {"if", TOKEN_KW_IF},         {"import", TOKEN_KW_IMPORT},
    {"Int", TOKEN_KW_INT},   {"return", TOKEN_KW_RETURN}, {"String", TOKEN_KW_STRING},
    {"then", TOKEN_KW_THEN}, {"Unit", TOKEN_KW_UNIT},     {"val", TOKEN_KW_VAL},
    {"var", TOKEN_KW_VAR},   {"while", TOKEN_KW_WHILE},   {"Boolean", TOKEN_KW_BOOLEAN},
    {"case", TOKEN_KW_CASE}, {"class", TOKEN_KW_CLASS},   {"extension", TOKEN_KW_EXTENSION},
    {"for", TOKEN_KW_FOR},   {"List", TOKEN_KW_LIST},     {"match", TOKEN_KW_MATCH},
    {"type", TOKEN_KW_TYPE}, {"true", TOKEN_KW_TRUE},     {"false", TOKEN_KW_FALSE},
    {"to", TOKEN_KW_TO},     {"until", TOKEN_KW_UNTIL},   {"by", TOKEN_KW_BY}};

#define KEYWORD_COUNT (sizeof(keyword) / sizeof(keyword[0]))

static TokenType check_for_token(const char *identifier) {
	for (size_t i = 0; i < KEYWORD_COUNT; i++) {
		if (strcmp(identifier, keyword[i].str) == 0) {
			return keyword[i].type;
		}
	}
	return TOKEN_ID_IDENT;
}

static void scanner_ungetc(int character, Scanner *scanner) {
	if (character != EOF) {
		ungetc(character, stdin);
		scanner->column--;
	}
}

void token_free(Token *token) {
	if (token->type == TOKEN_ID_IDENT || token->type == TOKEN_LIT_STRING || token->type == TOKEN_INTERP_TEXT ||
	    token->type == TOKEN_INTERP_ID) {
		free(token->value.string_value);
		token->value.string_value = NULL;
	}
}

ErrorCode scanner_init(Scanner *scanner) {
	scanner->column = 1;
	scanner->line = 1;
	scanner->end_of_line = 1;
	scanner->pending_dedents = 0;
	scanner->paren_depth = 0;
	scanner->interp_state = INTERP_NONE;
	scanner->pending_interp_end = 0;
	ErrorCode err1 = string_init(&scanner->buffer);
	if (err1 != ERR_OK) {
		return err1;
	}
	ErrorCode err2 = indent_stack_init(&scanner->indent_stack);
	if (err2 != ERR_OK) {
		string_free(&scanner->buffer);
		return err2;
	} else {
		return ERR_OK;
	}
}

void scanner_free(Scanner *scanner) {
	string_free(&scanner->buffer);
	indent_stack_free(&scanner->indent_stack);
}

static ErrorCode send_int(Token *token, Scanner *scanner, int character) {
	scanner_ungetc(character, scanner);

	errno = 0;
	char *endptr;
	long val = strtol(scanner->buffer.data, &endptr, 10);

	if (errno == ERANGE || val > INT_MAX || val < INT_MIN || (endptr != NULL && *endptr != '\0')) {
		return ERR_LEXICAL;
	}

	token->type = TOKEN_LIT_INT;
	token->value.int_value = (int)val;
	return ERR_OK;
}

static ErrorCode send_double(Token *token, Scanner *scanner, int character) {
	scanner_ungetc(character, scanner);

	errno = 0;
	char *endptr;
	double val = strtod(scanner->buffer.data, &endptr);

	if (errno == ERANGE || (endptr != NULL && *endptr != '\0')) {
		return ERR_LEXICAL;
	}

	token->type = TOKEN_LIT_DOUBLE;
	token->value.double_value = val;
	return ERR_OK;
}

static ErrorCode send_string(Token *token, Scanner *scanner, TokenType type) {
	token->type = type;
	token->value.string_value = strdup(scanner->buffer.data);
	if (token->value.string_value == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

// returns 1 if \n means EOL, if not returns 0
static int ends_logical_line(const Scanner *scanner) { return scanner->end_of_line == 0 && scanner->paren_depth == 0; }

ErrorCode get_token(Token *token, Scanner *scanner) {
	ScannerState state = STATE_START;
	int comment_depth = 0; // depth of /* comment
	int new_line_in_comment = 0;

	int indent = 0;
	int tab_in_indent = 0;
	int indent_done = 0;

	if (scanner->pending_dedents > 0) { // if there is more than 1 dedent
		scanner->pending_dedents--;
		token->type = TOKEN_DEDENT;
		return ERR_OK;
	}

	if (scanner->pending_interp_end) {
		token->type = TOKEN_INTERP_END;
		return ERR_OK;
	}
	if (scanner->interp_state == INTERP_TEXT_SINGLE) {
		state = STATE_SINGLE_TEXT;
		string_clear(&scanner->buffer);
		token->line = scanner->line;
		token->column = scanner->column;
	} else if (scanner->interp_state == INTERP_EXPR_SINGLE) {
		state = STATE_SINGLE_EXPRESSION;
		token->line = scanner->line;
		token->column = scanner->column;
	} else if (scanner->interp_state == INTERP_TEXT_MULTI) {
		state = STATE_MULTI_TEXT;
		string_clear(&scanner->buffer);
		token->line = scanner->line;
		token->column = scanner->column;
	} else if (scanner->interp_state == INTERP_EXPR_MULTI) {
		state = STATE_MULTI_EXPRESSION;
		token->line = scanner->line;
		token->column = scanner->column;
	}

	while (1) {
		int character = getc(stdin);
		scanner->column++;

		switch (state) {
		case STATE_START:

			token->line = scanner->line;
			token->column = scanner->column - 1;

			if (character == ' ') {
				if (scanner->end_of_line == 1 && scanner->paren_depth == 0 && indent_done == 0) {
					indent++;
				}
				break;
			}

			else if (character == '\t') {
				if (scanner->end_of_line == 1) {
					tab_in_indent = 1;
				}
				break;
			}

			else if (character == EOF) {
				if (scanner->interp_state != INTERP_NONE) {
					return ERR_LEXICAL;
				}
				if (scanner->end_of_line == 0) {
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				}
				if (scanner->indent_stack.size > 1) {
					indent_stack_pop(&scanner->indent_stack);
					token->type = TOKEN_DEDENT;
					return ERR_OK;
				}
				token->type = TOKEN_EOF;
				return ERR_OK;
			}

			else if (character == '\n') {
				if (scanner->interp_state == INTERP_EXPR_SINGLE) {
					return ERR_LEXICAL;
				}
				scanner->column = 1;
				scanner->line++;
				if (ends_logical_line(scanner)) {
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				}
				indent = 0;
				tab_in_indent = 0;
				indent_done = 0;
				new_line_in_comment = 0;
				break;
			}

			if (new_line_in_comment == 1 && character != '/') {
				return ERR_LEXICAL;
			}

			// indent, dedent
			if (scanner->end_of_line == 1 && scanner->paren_depth == 0 && character != '/') {
				if (tab_in_indent == 1) {
					return ERR_LEXICAL;
				}
				scanner->end_of_line = 0;
				int top = indent_stack_top(&scanner->indent_stack);

				if (indent > top) {
					ErrorCode err = indent_stack_push(&scanner->indent_stack, indent);
					if (err != ERR_OK) {
						return err;
					}
					scanner_ungetc(character, scanner);
					token->type = TOKEN_INDENT;
					return ERR_OK;
				} else if (indent < top) {
					int dedents = 0;
					while (indent < top) {
						indent_stack_pop(&scanner->indent_stack);
						top = indent_stack_top(&scanner->indent_stack);
						dedents++;
					}
					if (indent != top) {
						return ERR_LEXICAL;
					}
					scanner->pending_dedents = dedents - 1;
					scanner_ungetc(character, scanner);
					token->type = TOKEN_DEDENT;
					return ERR_OK;
				}
			}

			if (character == '+') {
				token->type = TOKEN_OP_PLUS;
				return ERR_OK;
			}

			else if (character == '-') {
				token->type = TOKEN_OP_MINUS;
				return ERR_OK;
			}

			else if (character == '*') {
				token->type = TOKEN_OP_MUL;
				return ERR_OK;
			}

			else if (character == '/') {
				indent_done = 1;
				state = STATE_DIV;
				break;
			}

			else if (character == '=') {
				state = STATE_ASSIGN;
				break;
			}

			else if (character == '!') {
				state = STATE_NOT;
				break;
			}

			else if (character == '<') {
				state = STATE_LT;
				break;
			}

			else if (character == '>') {
				state = STATE_GT;
				break;
			}

			else if (character == '&') {
				state = STATE_AND;
				break;
			}

			else if (character == '|') {
				state = STATE_OR;
				break;
			}

			else if (character == '"') {
				string_clear(&scanner->buffer);
				state = STATE_STR_SINGLE_START;
				break;
			}

			else if (character == 's') {
				state = STATE_S;
				break;
			}

			else if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			         character == '_') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_IDENT;
				break;
			}

			else if (character == '(') {
				scanner->paren_depth++;
				token->type = TOKEN_SE_LPAREN;
				return ERR_OK;
			}

			else if (character == ')') {
				if (scanner->paren_depth > 0) {
					scanner->paren_depth--;
				}
				token->type = TOKEN_SE_RPAREN;
				return ERR_OK;
			}

			else if (character == ',') {
				token->type = TOKEN_SE_COMMA;
				return ERR_OK;
			}

			else if (character == '.') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOT;
				break;
			}

			else if (character == ':') {
				token->type = TOKEN_SE_COLON;
				return ERR_OK;
			}

			else if (character == ';') {
				token->type = TOKEN_SE_SEMICOLON;
				return ERR_OK;
			}

			else if (character == '@') {
				token->type = TOKEN_SE_AT;
				return ERR_OK;
			}

			else if (character >= '1' && character <= '9') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_INT_1_9;
				break;
			}

			else if (character == '0') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_INT_0;
				break;
			}

			else if (character == '}' &&
			         (scanner->interp_state == INTERP_EXPR_SINGLE || scanner->interp_state == INTERP_EXPR_MULTI)) {
				if (scanner->paren_depth > 0) {
					scanner->paren_depth--;
				}
				if (scanner->interp_state == INTERP_EXPR_SINGLE) {
					scanner->interp_state = INTERP_TEXT_SINGLE;
				} else {
					scanner->interp_state = INTERP_TEXT_MULTI;
				}
				token->type = TOKEN_INTERP_EXPR_END;
				return ERR_OK;
			}

			else { // unknown character
				return ERR_LEXICAL;
			}

		case STATE_DIV:

			if (new_line_in_comment == 1) { // /* */  / 5 is valid,   /* \n */  / 5 is invalid
				return ERR_LEXICAL;
			}

			if (character == '/') {
				state = STATE_COMMENT_1;
				break;
			} else if (character == '*') {
				state = STATE_COMMENT_2;
				comment_depth++;
				break;
			} else {
				scanner->end_of_line = 0;
				token->type = TOKEN_OP_DIV;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}

		case STATE_ASSIGN:

			if (character == '=') {
				token->type = TOKEN_COM_EQ;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_ASSIGN;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}
		case STATE_NOT:

			if (character == '=') {
				token->type = TOKEN_COM_NE;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_NOT;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}

		case STATE_LT:

			if (character == '=') {
				token->type = TOKEN_COM_LE;
				return ERR_OK;
			} else if (character == '-') {
				token->type = TOKEN_SE_LARROW;
				return ERR_OK;
			} else {
				token->type = TOKEN_COM_LT;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}

		case STATE_GT:

			if (character == '=') {
				token->type = TOKEN_COM_GE;
				return ERR_OK;
			} else {
				token->type = TOKEN_COM_GT;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}

		case STATE_AND:

			if (character == '&') {
				token->type = TOKEN_OP_AND;
				return ERR_OK;
			} else {
				scanner_ungetc(character, scanner);
				return ERR_LEXICAL;
			}

		case STATE_OR:

			if (character == '|') {
				token->type = TOKEN_OP_OR;
				return ERR_OK;
			} else {
				scanner_ungetc(character, scanner);
				return ERR_LEXICAL;
			}

		case STATE_IDENT:

			if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			    (character >= '0' && character <= '9') || character == '_') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
			} else {
				scanner_ungetc(character, scanner);
				token->type = check_for_token(scanner->buffer.data);

				if (token->type == TOKEN_ID_IDENT) {
					token->value.string_value = strdup(scanner->buffer.data);
					if (token->value.string_value == NULL) {
						return ERR_INTERNAL_COMPILER;
					}
				}
				return ERR_OK;
			}
			break;

		case STATE_COMMENT_1:

			if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				if (ends_logical_line(scanner)) {
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				}
				indent = 0;
				tab_in_indent = 0;
				indent_done = 0;
				new_line_in_comment = 0;
				state = STATE_START;
				break;
			} else if (character == EOF) {
				state = STATE_START;
				break;
			} else {
				break;
			}

		case STATE_COMMENT_2:

			if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				new_line_in_comment = 1;
				state = STATE_COMMENT_2;
				break;
			} else if (character == '/') {
				state = STATE_COMMENT_3;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '*') {
				state = STATE_COMMENT_4;
				break;
			} else {
				state = STATE_COMMENT_2;
				break;
			}

		case STATE_COMMENT_3:

			if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				new_line_in_comment = 1;
				state = STATE_COMMENT_2;
				break;
			} else if (character == '*') {
				comment_depth++;
				state = STATE_COMMENT_2;
				break;
			} else if (character == '/') {
				state = STATE_COMMENT_3;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else {
				state = STATE_COMMENT_2;
				break;
			}

		case STATE_COMMENT_4:

			if (character == '/' && comment_depth == 1) {
				// end of comment
				comment_depth--;
				state = STATE_START;
				break;
			} else if (character == '/' && comment_depth != 1) {
				comment_depth--;
				state = STATE_COMMENT_2;
				break;
			} else if (character == '*') {
				state = STATE_COMMENT_4;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				new_line_in_comment = 1;
				state = STATE_COMMENT_2;
				break;
			} else {
				state = STATE_COMMENT_2;
				break;
			}

		// numbers
		case STATE_DOT:
			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE;
				break;
			} else {
				token->type = TOKEN_SE_DOT;
				scanner_ungetc(character, scanner);
				return ERR_OK;
			}

		case STATE_DOUBLE:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE;
				break;
			} else if (character == '_') {
				state = STATE_DOUBLE_UNDERSCORE;
				break;
			} else if (character == 'd' || character == 'D') {
				state = STATE_DOUBLE_D;
				break;
			} else if (character == 'e' || character == 'E') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E;
				break;
			} else {
				return send_double(token, scanner, character);
			}

		case STATE_INT_1_9:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_INT_1_9;
				break;
			} else if (character == '_') {
				state = STATE_INT_1_9_UNDERSCORE;
				break;
			} else if (character == 'd' || character == 'D') {
				state = STATE_DOUBLE_D;
				break;
			} else if (character == 'e' || character == 'E') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E;
				break;
			} else if (character == '.') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_DOT;
				break;
			} else {
				return send_int(token, scanner, character);
			}

		case STATE_INT_0:

			if (character == '.') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_DOT;
				break;
			} else if (character == 'd' || character == 'D') {
				state = STATE_DOUBLE_D;
				break;
			} else if (character == 'e' || character == 'E') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E;
				break;
			} else if ((character >= '0' && character <= '9') || character == '_') {
				return ERR_LEXICAL;
			} else {
				return send_int(token, scanner, character);
			}

		case STATE_DOUBLE_E:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E_PLUS_MINUS_0_9;
				break;
			} else if (character == '+' || character == '-') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E_PLUS_MINUS;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_DOUBLE_E_PLUS_MINUS_0_9:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E_PLUS_MINUS_0_9;
				break;
			} else if (character == 'd' || character == 'D') {
				state = STATE_DOUBLE_D;
				break;
			} else if (character == '_') {
				state = STATE_DOUBLE_E_PLUS_MINUS_0_9_UNDERSCORE;
				break;
			} else {
				return send_double(token, scanner, character);
			}

		case STATE_DOUBLE_E_PLUS_MINUS:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E_PLUS_MINUS_0_9;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_DOUBLE_UNDERSCORE:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_INT_1_9_UNDERSCORE:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_INT_1_9;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_DOUBLE_DOT:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_DOUBLE_D:

			return send_double(token, scanner, character);

		case STATE_DOUBLE_E_PLUS_MINUS_0_9_UNDERSCORE:

			if (character >= '0' && character <= '9') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_DOUBLE_E_PLUS_MINUS_0_9;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_STR_SINGLE_START:

			if (character == '"') {
				state = STATE_STR_EMPTY;
				break;
			} else if (character == '\n' || character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '\\') {
				state = STATE_STR_SINGLE_ESCAPE;
				break;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			}

		case STATE_STR_SINGLE_ESCAPE:

			if (character == '\\') {
				ErrorCode err = string_add_char(&scanner->buffer, '\\');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			} else if (character == '"') {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			} else if (character == 'n') {
				ErrorCode err = string_add_char(&scanner->buffer, '\n');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			} else if (character == 't') {
				ErrorCode err = string_add_char(&scanner->buffer, '\t');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			} else if (character == 'r') {
				ErrorCode err = string_add_char(&scanner->buffer, '\r');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_STR_SINGLE_CONTENT:

			if (character == '"') {
				state = STATE_STR_SINGLE_END;
				break;
			} else if (character == '\\') {
				state = STATE_STR_SINGLE_ESCAPE;
				break;
			} else if (character == '\n' || character == EOF) {
				return ERR_LEXICAL;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_SINGLE_CONTENT;
				break;
			}

		case STATE_STR_SINGLE_END:

			scanner_ungetc(character, scanner);
			token->type = TOKEN_LIT_STRING;
			token->value.string_value = strdup(scanner->buffer.data);
			if (token->value.string_value == NULL) {
				return ERR_INTERNAL_COMPILER;
			}
			return ERR_OK;

		case STATE_STR_EMPTY:

			if (character == '"') {
				state = STATE_STR_MULTI_START;
				break;
			} else {
				scanner_ungetc(character, scanner);
				token->type = TOKEN_LIT_STRING;
				token->value.string_value = strdup(scanner->buffer.data);
				if (token->value.string_value == NULL) {
					return ERR_INTERNAL_COMPILER;
				}
				return ERR_OK;
			}

		case STATE_STR_MULTI_START:

			if (character == '"') {
				state = STATE_STR_MULTI_Q1;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '\n') {
				scanner->line++;
				scanner->column = 1;
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_MULTI_START;
				break;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_STR_MULTI_START;
				break;
			}

		case STATE_STR_MULTI_Q1:

			if (character == '"') {
				state = STATE_STR_MULTI_Q2;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				scanner_ungetc(character, scanner);
				state = STATE_STR_MULTI_START;
				break;
			}

		case STATE_STR_MULTI_Q2:

			if (character == '"') {
				state = STATE_STR_MULTI_END;
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				scanner_ungetc(character, scanner);
				state = STATE_STR_MULTI_START;
				break;
			}

		case STATE_STR_MULTI_END:

			scanner_ungetc(character, scanner);
			token->type = TOKEN_LIT_STRING;
			token->value.string_value = strdup(scanner->buffer.data);
			if (token->value.string_value == NULL) {
				return ERR_INTERNAL_COMPILER;
			}
			return ERR_OK;

		case STATE_S:

			if (character == '"') {
				state = STATE_S_ONE;
				break;
			} else {
				string_clear(&scanner->buffer);
				ErrorCode err1 = string_add_char(&scanner->buffer, 's');
				if (err1 != ERR_OK) {
					return err1;
				}
				scanner_ungetc(character, scanner);
				state = STATE_IDENT;
				break;
			}

		case STATE_S_ONE:

			if (scanner->interp_state != INTERP_NONE) {
				return ERR_LEXICAL;
			}

			if (character == '\\') {
				state = STATE_SINGLE_ESCAPE;
				break;
			} else if (character == '$') {
				state = STATE_SINGLE_DOLLAR;
				break;
			} else if (character == '\n' || character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '"') {
				state = STATE_S_TWO;
				break;
			} else {
				scanner_ungetc(character, scanner);
				scanner->interp_state = INTERP_TEXT_SINGLE;
				token->type = TOKEN_INTERP_START;
				return ERR_OK;
			}

		case STATE_S_TWO:

			if (character == '"') {
				scanner->interp_state = INTERP_TEXT_MULTI;
				token->type = TOKEN_INTERP_START;
				return ERR_OK;
			} else {
				scanner_ungetc(character, scanner);
				scanner->pending_interp_end = 1;
				scanner->interp_state = INTERP_NONE;
				token->type = TOKEN_INTERP_START;
				return ERR_OK;
			}

		case STATE_SINGLE_TEXT:

			if (character == '\\') {
				state = STATE_SINGLE_ESCAPE;
				break;
			} else if (character == '\n' || character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '"') {
				if (scanner->buffer.length > 0) {
					scanner_ungetc(character, scanner);
					return send_string(token, scanner, TOKEN_INTERP_TEXT);
				}
				scanner->interp_state = INTERP_NONE;
				token->type = TOKEN_INTERP_END;
				return ERR_OK;
			} else if (character == '$') {
				if (scanner->buffer.length > 0) {
					scanner_ungetc(character, scanner);
					return send_string(token, scanner, TOKEN_INTERP_TEXT);
				}
				state = STATE_SINGLE_DOLLAR;
				break;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			}

		case STATE_SINGLE_ESCAPE:

			if (character == '\\') {
				ErrorCode err = string_add_char(&scanner->buffer, '\\');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else if (character == '"') {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else if (character == '\n') {
				ErrorCode err = string_add_char(&scanner->buffer, '\n');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else if (character == '\r') {
				ErrorCode err = string_add_char(&scanner->buffer, '\r');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else if (character == '\t') {
				ErrorCode err = string_add_char(&scanner->buffer, '\t');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_SINGLE_ID:

			if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			    (character >= '0' && character <= '9') || character == '_') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				break;
			} else {
				scanner_ungetc(character, scanner);
				return send_string(token, scanner, TOKEN_INTERP_ID);
			}

		case STATE_SINGLE_DOLLAR:

			if (character == '$') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, '$');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_TEXT;
				break;
			} else if (character == '{') {
				scanner->interp_state = INTERP_EXPR_SINGLE;
				scanner->paren_depth++;
				token->type = TOKEN_INTERP_EXPR_START;
				return ERR_OK;
			} else if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			           character == '_') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_SINGLE_ID;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_SINGLE_EXPRESSION:

			if (character == '}') {
				if (scanner->paren_depth > 0) {
					scanner->paren_depth--;
				}
				scanner->interp_state = INTERP_TEXT_SINGLE;
				token->type = TOKEN_INTERP_EXPR_END;
				return ERR_OK;
			} else if (character == '\n' || character == EOF) {
				return ERR_LEXICAL;
			} else {
				scanner_ungetc(character, scanner);
				state = STATE_START;
				break;
			}

		case STATE_MULTI_TEXT:

			if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				break;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else if (character == '"') {
				if (scanner->buffer.length > 0) {
					scanner_ungetc(character, scanner);
					return send_string(token, scanner, TOKEN_INTERP_TEXT);
				}
				state = STATE_MULTI_ONE;
				break;
			} else if (character == '$') {
				if (scanner->buffer.length > 0) {
					scanner_ungetc(character, scanner);
					return send_string(token, scanner, TOKEN_INTERP_TEXT);
				}
				state = STATE_MULTI_DOLLAR;
				break;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_MULTI_TEXT;
				break;
			}

		case STATE_MULTI_ID:

			if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			    (character >= '0' && character <= '9') || character == '_') {
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				break;
			} else {
				scanner_ungetc(character, scanner);
				state = STATE_MULTI_TEXT;
				return send_string(token, scanner, TOKEN_INTERP_ID);
			}

		case STATE_MULTI_DOLLAR:

			if (character == '$') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, '$');
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_MULTI_TEXT;
				break;
			} else if (character == '{') {
				scanner->interp_state = INTERP_EXPR_MULTI;
				scanner->paren_depth++;
				token->type = TOKEN_INTERP_EXPR_START;
				return ERR_OK;
			} else if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			           character == '_') {
				string_clear(&scanner->buffer);
				ErrorCode err = string_add_char(&scanner->buffer, character);
				if (err != ERR_OK) {
					return err;
				}
				state = STATE_MULTI_ID;
				break;
			} else {
				return ERR_LEXICAL;
			}

		case STATE_MULTI_EXPRESSION:

			if (character == '}') {
				if (scanner->paren_depth > 0) {
					scanner->paren_depth--;
				}
				scanner->interp_state = INTERP_TEXT_MULTI;
				token->type = TOKEN_INTERP_EXPR_END;
				return ERR_OK;
			} else if (character == EOF) {
				return ERR_LEXICAL;
			} else {
				scanner_ungetc(character, scanner);
				state = STATE_START;
				break;
			}

		case STATE_MULTI_ONE:
			if (character == '"') {
				state = STATE_MULTI_TWO;
				break;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				scanner_ungetc(character, scanner);
				state = STATE_MULTI_TEXT;
				break;
			}

		case STATE_MULTI_TWO:
			if (character == '"') {
				scanner->interp_state = INTERP_NONE;
				token->type = TOKEN_INTERP_END;
				return ERR_OK;
			} else {
				ErrorCode err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				err = string_add_char(&scanner->buffer, '"');
				if (err != ERR_OK) {
					return err;
				}
				scanner_ungetc(character, scanner);
				state = STATE_MULTI_TEXT;
				break;
			}
		}
	}
}