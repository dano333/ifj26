/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#include "scanner.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	StringBuffer buffer;
	int line;
	int column
} Scanner;

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

ErrorCode get_token(Token *token) {
	ScannerState state = STATE_START;
	StringBuffer string_buffer;

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
				string_init(&string_buffer);
				string_add_char(&string_buffer, character);
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
				token->type = TOKEN_SE_DOT;
				return ERR_OK;
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

		case STATE_DIV: // DOROBIT KOMETARE SEM!!!!

			token->type = TOKEN_OP_DIV;
			return ERR_OK;

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
				string_add_char(&string_buffer, character);
			} else {
				ungetc(character, stdin);
				token->type = check_for_token(string_buffer.data);

				if (token->type == TOKEN_ID_IDENT) {
					token->value.string_value = &string_buffer.data;
				}
				return ERR_OK;
			}
		}
	}
}