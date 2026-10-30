/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#include "scanner.h"
#include <stdio.h>
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
	STATE_COMMENT_1,   // //
	STATE_COMMENT_2,   // /*
	STATE_COMMENT_3,   // /* + another /
	STATE_COMMENT_4,   // /**
	STATE_DOT,         // .
	STATE_INT_0,       // int 0
	STATE_INT_VAL,     // int 1-9 then 0-9
	STATE_INT_DOT,     // int + .
	STATE_DOUBLE,      // double
	STATE_DOUBLE_E,    // double e
	STATE_DOUBLE_SIGN, // double e +/-
	STATE_DOUBLE_EXP,  // double e 0-9
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
	return string_init(&scanner->buffer);
}

void scanner_free(Scanner *scanner) {
	scanner->column = 0;
	scanner->line = 0;
	string_free(&scanner->buffer);
}

ErrorCode get_token(Token *token, Scanner *scanner) {
	ScannerState state = STATE_START;
	int comment_depth = 0; // depth of /* comment
	int new_line_in_comment = 0;

	while (1) {
		int character = getc(stdin);

		switch (state) {
		case STATE_START:

			if (character == ' ' || character == '\t') {
				break;
			}

			else if (character == EOF) {
				token->type = TOKEN_EOF;
				return ERR_OK;
			}

			else if (character == '\n') {
				token->type = TOKEN_EOL;
				return ERR_OK;
			}

			else if (character == '+') {
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
					return ERR_INTERNAL_COMPILER;
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
				return ERR_OK;
			}

		case STATE_ASSIGN:

			if (character == '=') {
				token->type = TOKEN_COM_EQ;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_ASSIGN;
				ungetc(character, stdin);
				return ERR_OK;
			}
		case STATE_NOT:

			if (character == '=') {
				token->type = TOKEN_COM_NE;
				return ERR_OK;
			} else {
				token->type = TOKEN_OP_NOT;
				ungetc(character, stdin);
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
				return ERR_OK;
			}

		case STATE_GT:

			if (character == '=') {
				token->type = TOKEN_COM_GE;
				return ERR_OK;
			} else {
				token->type = TOKEN_COM_GT;
				ungetc(character, stdin);
				return ERR_OK;
			}

		case STATE_AND:

			if (character == '&') {
				token->type = TOKEN_OP_AND;
				return ERR_OK;
			} else {
				ungetc(character, stdin);
				return ERR_LEXICAL;
			}

		case STATE_OR:

			if (character == '|') {
				token->type = TOKEN_OP_OR;
				return ERR_OK;
			} else {
				ungetc(character, stdin);
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
				token->type = check_for_token(scanner->buffer.data);

				if (token->type == TOKEN_ID_IDENT) {
					token->value.string_value = &scanner->buffer;
				}
				return ERR_OK;
			}
			break;

		case STATE_COMMENT_1:

			if (character == '\n') {
				token->type = TOKEN_EOL;
				return ERR_OK;
			} else if (character == EOF) {
				token->type = TOKEN_EOF;
				return ERR_OK;
			} else {
				state = STATE_COMMENT_1;
				break;
			}

		case STATE_COMMENT_2:

			if (character == '\n') {
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
				new_line_in_comment = 1;
				state = STATE_COMMENT_2;
				break;
			} else {
				state = STATE_COMMENT_2;
				break;
			}

		case STATE_DOT: // NEDOROBENE!!!
			if (character >= '0' && character <= '9') {
				state = STATE_DOUBLE;
				break;
			} else {
				ungetc(character, stdin);
				return ERR_LEXICAL;
			}
		}
	}
}