/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#include "scanner.h"
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
	STATE_COMMENT_1,                         // //
	STATE_COMMENT_2,                         // /*
	STATE_COMMENT_3,                         // /* + another /
	STATE_COMMENT_4,                         // /**
	STATE_DOT,                               // .
	STATE_DOUBLE,                            // double
	STATE_INT_1_9,                           // int 1-9
	STATE_INT_0,                             // int 0
	STATE_DOUBLE_E,                          // double e
	STATE_DOUBLE_E_PLUS_MINUS_0_9,           // double e '+/-' 0-9
	STATE_DOUBLE_E_PLUS_MINUS,               // double e +/-
	STATE_DOUBLE_UNDERSCORE,                 // double_
	STATE_INT_1_9_UNDERSCORE,                // int 1-9_
	STATE_DOUBLE_DOT,                        // double .
	STATE_DOUBLE_D,                          // double D
	STATE_DOUBLE_E_PLUS_MINUS_0_9_UNDERSCORE // double e '+/-' 0-9 _
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
    {"type", TOKEN_KW_TYPE}, {"true", TOKEN_KW_TRUE},     {"false", TOKEN_KW_FALSE}};

#define KEYWORD_COUNT (sizeof(keyword) / sizeof(keyword[0]))

static TokenType check_for_token(const char *identifier) {
	for (size_t i = 0; i < KEYWORD_COUNT; i++) {
		if (strcmp(identifier, keyword[i].str) == 0) {
			return keyword[i].type;
		}
	}
	return TOKEN_ID_IDENT;
}

ErrorCode scanner_init(Scanner *scanner) {
	scanner->column = 1;
	scanner->line = 1;
	scanner->end_of_line = 1;
	return string_init(&scanner->buffer);
}

void scanner_free(Scanner *scanner) {
	scanner->column = 0;
	scanner->line = 0;
	scanner->end_of_line = 0;
	string_free(&scanner->buffer);
}

static ErrorCode send_int(Token *token, Scanner *scanner, int character) {
	ungetc(character, stdin);
	scanner->column--;
	token->type = TOKEN_LIT_INT;
	token->value.int_value = strtol(scanner->buffer.data, NULL, 10);
	return ERR_OK;
}

static ErrorCode send_double(Token *token, Scanner *scanner, int character) {
	ungetc(character, stdin);
	scanner->column--;
	token->type = TOKEN_LIT_DOUBLE;
	token->value.double_value = strtod(scanner->buffer.data, NULL);
	return ERR_OK;
}

ErrorCode get_token(Token *token, Scanner *scanner) {
	ScannerState state = STATE_START;
	int comment_depth = 0; // depth of /* comment
	int new_line_in_comment = 0;

	while (1) {
		int character = getc(stdin);
		scanner->column++;

		switch (state) {
		case STATE_START:

			if (character == ' ' || character == '\t') {
				break;
			}

			else if (character == EOF) {
				if (scanner->end_of_line == 0) {
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				} else {
					token->type = TOKEN_EOF;
					return ERR_OK;
				}
			}

			else if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				scanner->end_of_line = 1;
				token->type = TOKEN_EOL;
				return ERR_OK;
			}

			scanner->end_of_line = 0;

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
				token->type = TOKEN_SE_LPAREN;
				return ERR_OK;
			}

			else if (character == ')') {
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

			else { // unknown character
				return ERR_LEXICAL;
			}

		case STATE_DIV:

			if (character == '/') {
				state = STATE_COMMENT_1;
				break;
			} else if (character == '*') {
				state = STATE_COMMENT_2;
				comment_depth++;
				break;
			} else {
				token->type = TOKEN_OP_DIV;
				ungetc(character, stdin);
				scanner->column--;
				return ERR_OK;
			}

		case STATE_ASSIGN:

			if (character == '=') {
				token->type = TOKEN_COM_EQ;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_ASSIGN;
				ungetc(character, stdin);
				scanner->column--;
				return ERR_OK;
			}
		case STATE_NOT:

			if (character == '=') {
				token->type = TOKEN_COM_NE;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_NOT;
				ungetc(character, stdin);
				scanner->column--;
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
				ungetc(character, stdin);
				scanner->column--;
				return ERR_OK;
			}

		case STATE_GT:

			if (character == '=') {
				token->type = TOKEN_COM_GE;
				return ERR_OK;
			} else {
				token->type = TOKEN_COM_GT;
				ungetc(character, stdin);
				scanner->column--;
				return ERR_OK;
			}

		case STATE_AND:

			if (character == '&') {
				token->type = TOKEN_OP_AND;
				return ERR_OK;
			} else {
				ungetc(character, stdin);
				scanner->column--;
				return ERR_LEXICAL;
			}

		case STATE_OR:

			if (character == '|') {
				token->type = TOKEN_OP_OR;
				return ERR_OK;
			} else {
				ungetc(character, stdin);
				scanner->column--;
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
				ungetc(character, stdin);
				scanner->column--;
				token->type = check_for_token(scanner->buffer.data);

				if (token->type == TOKEN_ID_IDENT) {
					token->value.string_value = &scanner->buffer;
				}
				return ERR_OK;
			}
			break;

		case STATE_COMMENT_1:

			if (character == '\n') {
				scanner->column = 1;
				scanner->line++;
				scanner->end_of_line = 1;
				token->type = TOKEN_EOL;
				return ERR_OK;
			} else if (character == EOF) {
				if (scanner->end_of_line == 0) {
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				} else {
					token->type = TOKEN_EOF;
					return ERR_OK;
				}
			} else {
				state = STATE_COMMENT_1;
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

				if (new_line_in_comment == 1) {
					scanner->column = 1;
					scanner->end_of_line = 1;
					token->type = TOKEN_EOL;
					return ERR_OK;
				} else {
					state = STATE_START;
					break;
				}

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
				ungetc(character, stdin);
				scanner->column--;
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
		}
	}
}