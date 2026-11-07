/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser
 */

#include "parser.h"
#include "error.h"
#include "semantics.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- DEBUG MACRO FOR PARSING ERRORS --- */
#define PARSER_ERR(func_name)                                                                                          \
	do {                                                                                                               \
		fprintf(stderr, "\n[PARSER ERROR] %s() failed at line %d, col %d (Token Type: %d)\n", func_name,               \
		        parser->current_token.line, parser->current_token.column, parser->current_token.type);                 \
		return ERR_PARSING;                                                                                            \
	} while (0)

static ErrorCode parser_next_token(Parser *parser) {
	token_free(&parser->current_token);
	return get_token(&parser->current_token, parser->scanner);
}

ErrorCode parser_init(Parser *parser, Scanner *scanner, SymTable *global_table) {
	parser->scanner = scanner;
	parser->current_scope = global_table;
	parser->current_ret_type = TYPE_UNKNOWN;
	memset(&parser->current_token, 0, sizeof(Token));
	return parser_next_token(parser);
}

ErrorCode compare_token(Parser *parser, TokenType expected) {
	if (parser->current_token.type == expected) {
		return parser_next_token(parser);
	} else {
		PARSER_ERR("compare_token");
	}
}

static bool is_f_def(TokenType type) { return (type == TOKEN_KW_DEF || type == TOKEN_SE_AT); }

ErrorCode prolog(Parser *parser) {
	ErrorCode err = ERR_OK;

	while (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
	}

	if ((err = compare_token(parser, TOKEN_KW_IMPORT)) != ERR_OK) {
		return err;
	}

	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("prolog");
	}

	if (parser->current_token.value.string_value == NULL ||
	    strcmp(parser->current_token.value.string_value, "ifj26") != 0) {
		PARSER_ERR("prolog");
	}

	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
		return err;
	}

	if ((err = compare_token(parser, TOKEN_SE_DOT)) != ERR_OK) {
		return err;
	}

	if ((err = compare_token(parser, TOKEN_OP_MUL)) != ERR_OK) {
		return err;
	}

	if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
		return err;
	}

	if ((err = f_list(parser)) != ERR_OK) {
		return err;
	}

	if ((err = compare_token(parser, TOKEN_EOF)) != ERR_OK) {
		return err;
	}

	return ERR_OK;
}

ErrorCode f_list(Parser *parser) {
	ErrorCode err = ERR_OK;
	TokenType current_type = parser->current_token.type;

	if (is_f_def(current_type)) {
		if ((err = f_def(parser)) != ERR_OK) {
			return err;
		}
		return f_list(parser);
	} else if (current_type == TOKEN_KW_EXTENSION) {
		if ((err = compare_token(parser, TOKEN_KW_EXTENSION)) != ERR_OK) {
			return err;
		}
		if ((err = extension(parser)) != ERR_OK) {
			return err;
		}
		return f_list(parser);
	} else {
		return ERR_OK;
	}
}

ErrorCode f_def(Parser *parser) {
	ErrorCode err = ERR_OK;

	if ((err = anotacia(parser)) != ERR_OK) {
		return err;
	}

	if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK) {
		return err;
	}

	if (parser->current_token.type != TOKEN_ID_IDENT) {
		PARSER_ERR("f_def");
	}

	char *func_name = strdup(parser->current_token.value.string_value);
	if (symtable_lookup_local(parser->current_scope, func_name) != NULL) {
		free(func_name);
		return ERR_SEMANTIC_REDEFINITION;
	}
	symtable_insert(parser->current_scope, func_name, SYMBOL_FUNC, TYPE_UNKNOWN, 0);

	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
		free(func_name);
		return err;
	}

	SymTable func_scope;
	symtable_init(&func_scope, parser->current_scope);
	parser->current_scope = &func_scope;

	if ((err = param_lists(parser)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		free(func_name);
		return err;
	}

	DataType ret_type;
	if ((err = data_type(parser, &ret_type)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		free(func_name);
		return err;
	}

	parser->current_ret_type = ret_type;

	Symbol *func_sym = symtable_lookup(func_scope.parent, func_name);
	if (func_sym != NULL) {
		func_sym->type = ret_type;
	}
	free(func_name);

	if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		return err;
	}

	if ((err = f_body_end(parser)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		return err;
	}

	parser->current_scope = func_scope.parent;
	symtable_free(&func_scope);

	return ERR_OK;
}

ErrorCode anotacia(Parser *parser) {
	ErrorCode err = ERR_OK;
	if (parser->current_token.type == TOKEN_SE_AT) {
		if ((err = parser_next_token(parser)) != ERR_OK) {
			return err;
		}

		if (parser->current_token.type == TOKEN_ID_IDENT &&
		    strcmp(parser->current_token.value.string_value, "main") == 0) {

			if ((err = parser_next_token(parser)) != ERR_OK) {
				return err;
			}

			if ((err = new_line(parser)) != ERR_OK) {
				return err;
			}
		} else {
			PARSER_ERR("anotacia");
		}

		if (parser->current_token.type == TOKEN_EOL) {
			if ((err = parser_next_token(parser)) != ERR_OK) {
				return err;
			}
		}
	}

	return ERR_OK;
}

ErrorCode extension(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType dummy_type;

	if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK)
		return err;
	if ((err = type(parser, &dummy_type)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
		return err;
	if ((err = e_body(parser)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode e_body(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType dummy_type;

	if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK)
		return err;
	if ((err = param_lists(parser)) != ERR_OK)
		return err;
	if ((err = data_type(parser, &dummy_type)) != ERR_OK)
		return err;
	if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK)
		return err;
	if ((err = f_body_end(parser)) != ERR_OK)
		return err;
	if ((err = eb_next(parser)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode eb_next(Parser *parser) {
	ErrorCode err = ERR_OK;
	if (parser->current_token.type == TOKEN_KW_DEF) {
		if ((err = e_body(parser)) != ERR_OK) {
			return err;
		}
	}
	return ERR_OK;
}

ErrorCode type(Parser *parser, DataType *out_type) {
	TokenType current_token = parser->current_token.type;

	if (current_token == TOKEN_KW_INT) {
		*out_type = TYPE_INT;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_DOUBLE) {
		*out_type = TYPE_DOUBLE;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_STRING) {
		*out_type = TYPE_STRING;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_UNIT) {
		*out_type = TYPE_UNIT;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_BOOLEAN) {
		*out_type = TYPE_BOOLEAN;
		return parser_next_token(parser);
	} else {
		PARSER_ERR("type");
	}
}

ErrorCode param_lists(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_SE_LPAREN) {
		if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
			return err;
		if ((err = parameter(parser)) != ERR_OK)
			return err;
		if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK)
			return err;
		if ((err = param_lists(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode data_type(Parser *parser, DataType *out_type) {
	ErrorCode err = ERR_OK;
	*out_type = TYPE_UNKNOWN;

	if (parser->current_token.type == TOKEN_SE_COLON) {
		if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK)
			return err;
		if ((err = type(parser, out_type)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode f_body_end(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType dummy_type;

	while (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
	}

	if (parser->current_token.type == TOKEN_INDENT) {
		if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
			return err;
		if ((err = f_body(parser)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_DEDENT) {
			if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("f_body_end");
		}
	} else {
		if ((err = parse_expr(parser, &dummy_type)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_EOL) {
			if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
				return err;
		}
	}

	return ERR_OK;
}

ErrorCode f_body(Parser *parser) {
	ErrorCode err = ERR_OK;
	if ((err = stmt(parser)) != ERR_OK)
		return err;
	if ((err = stmt_next(parser)) != ERR_OK)
		return err;
	return ERR_OK;
}

ErrorCode new_line(Parser *parser) {
	ErrorCode err = ERR_OK;
	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
	}
	return ERR_OK;
}

ErrorCode parameter(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType p_type;

	if (parser->current_token.type == TOKEN_ID_IDENT) {
		char *param_name = strdup(parser->current_token.value.string_value);

		if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
			free(param_name);
			return err;
		}
		if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
			free(param_name);
			return err;
		}
		if ((err = type_func(parser, &p_type)) != ERR_OK) {
			free(param_name);
			return err;
		}

		symtable_insert(parser->current_scope, param_name, SYMBOL_VAR, p_type, false);
		free(param_name);

		if ((err = param_next(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode param_next(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_SE_COMMA) {
		if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK)
			return err;
		if ((err = param_tail(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode param_tail(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType p_type;

	if (parser->current_token.type == TOKEN_ID_IDENT) {
		char *param_name = strdup(parser->current_token.value.string_value);

		if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
			free(param_name);
			return err;
		}
		if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
			free(param_name);
			return err;
		}
		if ((err = type(parser, &p_type)) != ERR_OK) {
			free(param_name);
			return err;
		}

		symtable_insert(parser->current_scope, param_name, SYMBOL_VAR, p_type, false);
		free(param_name);

		if ((err = param_next(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode ret_val(Parser *parser, DataType expected_ret_type) {
	ErrorCode err = ERR_OK;
	DataType expr_type;
	TokenType t = parser->current_token.type;

	if (t == TOKEN_ID_IDENT || t == TOKEN_LIT_INT || t == TOKEN_LIT_DOUBLE || t == TOKEN_LIT_STRING ||
	    t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE || t == TOKEN_SE_LPAREN || t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT ||
	    t == TOKEN_KW_IF) {
		if ((err = parse_expr(parser, &expr_type)) != ERR_OK) {
			return err;
		}
		if (!is_type_compatible(expected_ret_type, expr_type)) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] Return type mismatch: expected=%d, got=%d\n", expected_ret_type,
			        expr_type);
			return ERR_SEMANTIC_COMPATIBILITY;
		}
	} else if (expected_ret_type != TYPE_UNIT && expected_ret_type != TYPE_UNKNOWN) {
		return ERR_SEMANTIC_COMPATIBILITY;
	}

	return ERR_OK;
}

ErrorCode val(Parser *parser, DataType *out_type) {
	ErrorCode err = ERR_OK;
	TokenType t = parser->current_token.type;

	if (t == TOKEN_KW_IF) {
		if ((err = if_control(parser)) != ERR_OK) {
			return err;
		}
		*out_type = TYPE_UNKNOWN;
	} else if (t == TOKEN_SE_LPAREN) {
		if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
			return err;
		if (parser->current_token.type != TOKEN_SE_RPAREN) {
			if ((err = parse_expr(parser, out_type)) != ERR_OK)
				return err;
		} else {
			*out_type = TYPE_UNIT;
		}
		if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK)
			return err;
	} else if (t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT) {
		TokenType op = t;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;
		if ((err = val(parser, out_type)) != ERR_OK)
			return err;
		if (op == TOKEN_OP_MINUS && *out_type != TYPE_INT && *out_type != TYPE_DOUBLE) {
			return ERR_SEMANTIC_TYPE;
		}
		if (op == TOKEN_OP_NOT && *out_type != TYPE_BOOLEAN) {
			return ERR_SEMANTIC_TYPE;
		}
	} else if (t == TOKEN_LIT_INT) {
		*out_type = TYPE_INT;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;
	} else if (t == TOKEN_LIT_DOUBLE) {
		*out_type = TYPE_DOUBLE;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;
	} else if (t == TOKEN_LIT_STRING) {
		*out_type = TYPE_STRING;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;
	} else if (t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE) {
		*out_type = TYPE_BOOLEAN;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;
	} else if (t == TOKEN_ID_IDENT) {
		char *id_name = strdup(parser->current_token.value.string_value);

		Symbol *sym = symtable_lookup(parser->current_scope, id_name);
		if (sym == NULL) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] Undefined identifier or function: '%s' at line %d\n", id_name,
			        parser->current_token.line);
			free(id_name);
			return ERR_SEMANTIC_UNDEFINED;
		}
		*out_type = sym->type;

		if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
			free(id_name);
			return err;
		}

		if (parser->current_token.type == TOKEN_SE_LPAREN) {
			if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK) {
				free(id_name);
				return err;
			}

			int arg_count = 0;
			DataType arg_types[10];

			if (parser->current_token.type != TOKEN_SE_RPAREN) {
				DataType a_type;
				if ((err = parse_expr(parser, &a_type)) != ERR_OK) {
					free(id_name);
					return err;
				}
				arg_types[arg_count++] = a_type;

				while (parser->current_token.type == TOKEN_SE_COMMA) {
					if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK) {
						free(id_name);
						return err;
					}
					if ((err = parse_expr(parser, &a_type)) != ERR_OK) {
						free(id_name);
						return err;
					}
					if (arg_count < 10)
						arg_types[arg_count++] = a_type;
				}
			}
			if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK) {
				free(id_name);
				return err;
			}

			DataType ret_type;
			if ((err = validate_function_call(parser->current_scope, id_name, arg_types, arg_count, &ret_type)) != ERR_OK) {
				free(id_name);
				return err;
			}
			*out_type = ret_type;
		}
		free(id_name);
	} else {
		PARSER_ERR("val");
	}

	return ERR_OK;
}

ErrorCode dec(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType dummy_type;
	TokenType t = parser->current_token.type;

	if (t == TOKEN_OP_ASSIGN) {
		if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK)
			return err;
		if ((err = parse_expr(parser, &dummy_type)) != ERR_OK)
			return err;
	} else if (t == TOKEN_OP_PLUS || t == TOKEN_OP_MINUS || t == TOKEN_OP_MUL || t == TOKEN_OP_DIV ||
	           t == TOKEN_COM_EQ || t == TOKEN_COM_NE || t == TOKEN_COM_LT || t == TOKEN_COM_LE || t == TOKEN_COM_GT ||
	           t == TOKEN_COM_GE || t == TOKEN_OP_AND || t == TOKEN_OP_OR || t == TOKEN_SE_DOT ||
	           t == TOKEN_SE_LPAREN) {
		DataType left_dummy = TYPE_UNKNOWN;
		if ((err = exp_cont(parser, left_dummy, &dummy_type)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode stmt_next(Parser *parser) {
	ErrorCode err = ERR_OK;
	TokenType t = parser->current_token.type;

	if (t == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_DEDENT) {
			return ERR_OK;
		}
		if ((err = stmt(parser)) != ERR_OK)
			return err;
		if ((err = stmt_next(parser)) != ERR_OK)
			return err;
	} else if (t == TOKEN_SE_SEMICOLON) {
		if ((err = compare_token(parser, TOKEN_SE_SEMICOLON)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_DEDENT) {
			return ERR_OK;
		}
		if ((err = stmt(parser)) != ERR_OK)
			return err;
		if ((err = stmt_next(parser)) != ERR_OK)
			return err;
	} else if (t == TOKEN_ID_IDENT || t == TOKEN_KW_VAL || t == TOKEN_KW_VAR || t == TOKEN_KW_RETURN ||
	           t == TOKEN_KW_IF || t == TOKEN_KW_WHILE || t == TOKEN_KW_FOR) {
		if ((err = stmt(parser)) != ERR_OK)
			return err;
		if ((err = stmt_next(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode stmt(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_VAL || parser->current_token.type == TOKEN_KW_VAR) {
		int is_mutable = (parser->current_token.type == TOKEN_KW_VAR);

		if ((err = compare_token(parser, parser->current_token.type)) != ERR_OK)
			return err;

		if (parser->current_token.type != TOKEN_ID_IDENT) {
			PARSER_ERR("stmt");
		}

		char *var_name = strdup(parser->current_token.value.string_value);

		if (symtable_lookup_local(parser->current_scope, var_name) != NULL) {
			free(var_name);
			return ERR_SEMANTIC_REDEFINITION;
		}

		if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
			free(var_name);
			return err;
		}

		DataType declared_type;
		if ((err = data_type(parser, &declared_type)) != ERR_OK) {
			free(var_name);
			return err;
		}

		if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
			free(var_name);
			return err;
		}

		DataType expr_type;
		if ((err = parse_expr(parser, &expr_type)) != ERR_OK) {
			free(var_name);
			return err;
		}

		if (declared_type != TYPE_UNKNOWN && !is_type_compatible(declared_type, expr_type)) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] Type mismatch in declaration: declared=%d, expr=%d\n", declared_type,
			        expr_type);
			free(var_name);
			return ERR_SEMANTIC_COMPATIBILITY;
		}

		DataType final_type = (declared_type != TYPE_UNKNOWN) ? declared_type : expr_type;
		symtable_insert(parser->current_scope, var_name, is_mutable ? SYMBOL_VAR : SYMBOL_VAL, final_type, is_mutable);
		free(var_name);

	} else if (parser->current_token.type == TOKEN_KW_WHILE) {
		DataType cond_type;
		if ((err = compare_token(parser, TOKEN_KW_WHILE)) != ERR_OK)
			return err;
		if ((err = parse_expr(parser, &cond_type)) != ERR_OK)
			return err;
		if (cond_type != TYPE_BOOLEAN)
			return ERR_SEMANTIC_TYPE;
		if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK)
			return err;
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
		if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
			return err;
		if ((err = f_body(parser)) != ERR_OK)
			return err;
		if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
			return err;
	} else if (parser->current_token.type == TOKEN_KW_RETURN) {
		if ((err = compare_token(parser, TOKEN_KW_RETURN)) != ERR_OK)
			return err;
		if ((err = ret_val(parser, parser->current_ret_type)) != ERR_OK)
			return err;
	} else if (parser->current_token.type == TOKEN_ID_IDENT) {
		DataType expr_type;
		if ((err = parse_expr(parser, &expr_type)) != ERR_OK) {
			return err;
		}
		if (parser->current_token.type == TOKEN_OP_ASSIGN) {
			if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK)
				return err;
			DataType rhs_type;
			if ((err = parse_expr(parser, &rhs_type)) != ERR_OK)
				return err;
			if (!is_type_compatible(expr_type, rhs_type)) {
				fprintf(stderr, "\n[SEMANTIC DEBUG] Type mismatch in assignment: left=%d, rhs=%d\n", expr_type,
				        rhs_type);
				return ERR_SEMANTIC_COMPATIBILITY;
			}
		}
	} else if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = if_stmt_all(parser)) != ERR_OK)
			return err;
	} else if (parser->current_token.type == TOKEN_KW_FOR) {
		if ((err = for_stmt(parser)) != ERR_OK)
			return err;
	} else {
		DataType dummy_type;
		if ((err = parse_expr(parser, &dummy_type)) != ERR_OK) {
			return err;
		}
	}

	return ERR_OK;
}

ErrorCode if_stmt_all(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType cond_type;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("if_stmt_all");
	}

	if ((err = parse_expr(parser, &cond_type)) != ERR_OK)
		return err;
	if (cond_type != TYPE_BOOLEAN)
		return ERR_SEMANTIC_TYPE;

	if (parser->current_token.type == TOKEN_KW_THEN) {
		if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("if_stmt_all");
	}

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_INDENT) {
			if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
				return err;
			if ((err = f_body(parser)) != ERR_OK)
				return err;
			if (parser->current_token.type == TOKEN_DEDENT) {
				if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
					return err;
			} else {
				PARSER_ERR("if_stmt_all");
			}
		} else {
			if ((err = stmt(parser)) != ERR_OK)
				return err;
		}
	} else {
		if ((err = stmt(parser)) != ERR_OK)
			return err;
	}

	if ((err = if_next(parser)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode if_next(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_ELSE) {
		if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK)
			return err;
		if ((err = else_next(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode else_next(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = if_stmt_all(parser)) != ERR_OK)
			return err;
	} else if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_INDENT) {
			if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
				return err;
			if ((err = f_body(parser)) != ERR_OK)
				return err;
			if (parser->current_token.type == TOKEN_DEDENT) {
				if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
					return err;
			} else {
				PARSER_ERR("else_next");
			}
		} else {
			if ((err = stmt(parser)) != ERR_OK)
				return err;
		}
	} else {
		if ((err = stmt(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode if_control(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType cond_type;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("if_control");
	}

	if ((err = parse_expr(parser, &cond_type)) != ERR_OK)
		return err;
	if (cond_type != TYPE_BOOLEAN)
		return ERR_SEMANTIC_TYPE;

	if (parser->current_token.type == TOKEN_KW_THEN) {
		if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("if_control");
	}

	if ((err = if_control_next(parser)) != ERR_OK)
		return err;

	if (parser->current_token.type == TOKEN_KW_ELSE) {
		if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("if_control");
	}

	if ((err = if_control_next(parser)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode if_control_next(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType dummy_type;

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_INDENT) {
			if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("if_control_next");
		}
		if ((err = f_body(parser)) != ERR_OK)
			return err;
		if (parser->current_token.type == TOKEN_DEDENT) {
			if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("if_control_next");
		}
	} else {
		if ((err = parse_expr(parser, &dummy_type)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode for_stmt(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_FOR) {
		if ((err = compare_token(parser, TOKEN_KW_FOR)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("for_stmt");
	}

	if ((err = for_content(parser)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode for_content(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_INDENT) {
			if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_content");
		}

		if ((err = generator(parser)) != ERR_OK)
			return err;
		if ((err = eol_guards(parser)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_DEDENT) {
			if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_content");
		}

		if (parser->current_token.type == TOKEN_KW_DO) {
			if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_content");
		}

		if ((err = for_body(parser)) != ERR_OK)
			return err;
	} else {
		if ((err = generator(parser)) != ERR_OK)
			return err;
		if ((err = opt_guards(parser)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_KW_DO) {
			if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_content");
		}

		if ((err = for_body(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode generator(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType start_type, end_type;

	if (parser->current_token.type == TOKEN_ID_IDENT) {
		char *loop_var = strdup(parser->current_token.value.string_value);
		symtable_insert(parser->current_scope, loop_var, SYMBOL_VAR, TYPE_INT, true);
		free(loop_var);
		if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("generator");
	}

	if (parser->current_token.type == TOKEN_SE_LARROW) {
		if ((err = compare_token(parser, TOKEN_SE_LARROW)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("generator");
	}

	if ((err = parse_expr(parser, &start_type)) != ERR_OK)
		return err;
	if (start_type != TYPE_INT)
		return ERR_SEMANTIC_TYPE;

	if ((err = range_func(parser)) != ERR_OK)
		return err;

	if ((err = parse_expr(parser, &end_type)) != ERR_OK)
		return err;
	if (end_type != TYPE_INT)
		return ERR_SEMANTIC_TYPE;

	if ((err = opt_step(parser)) != ERR_OK)
		return err;

	return ERR_OK;
}

ErrorCode range_func(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_TO) {
		if ((err = compare_token(parser, TOKEN_KW_TO)) != ERR_OK)
			return err;
	} else if (parser->current_token.type == TOKEN_KW_UNTIL) {
		if ((err = compare_token(parser, TOKEN_KW_UNTIL)) != ERR_OK)
			return err;
	} else {
		PARSER_ERR("range_func");
	}

	return ERR_OK;
}

ErrorCode opt_step(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType step_type;

	if (parser->current_token.type == TOKEN_KW_BY) {
		if ((err = compare_token(parser, TOKEN_KW_BY)) != ERR_OK)
			return err;
		if ((err = parse_expr(parser, &step_type)) != ERR_OK)
			return err;
		if (step_type != TYPE_INT)
			return ERR_SEMANTIC_TYPE;
	}

	return ERR_OK;
}

ErrorCode opt_guards(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType guard_type;

	if (parser->current_token.type == TOKEN_KW_IF) {
		if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
			return err;
		if ((err = parse_expr(parser, &guard_type)) != ERR_OK)
			return err;
		if (guard_type != TYPE_BOOLEAN)
			return ERR_SEMANTIC_TYPE;
		if ((err = opt_guards(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode eol_guards(Parser *parser) {
	ErrorCode err = ERR_OK;
	DataType guard_type;

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_KW_IF) {
			if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("eol_guards");
		}

		if ((err = parse_expr(parser, &guard_type)) != ERR_OK)
			return err;
		if (guard_type != TYPE_BOOLEAN)
			return ERR_SEMANTIC_TYPE;

		if ((err = eol_guards(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode for_body(Parser *parser) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_EOL) {
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_INDENT) {
			if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_body");
		}

		if ((err = f_body(parser)) != ERR_OK)
			return err;

		if (parser->current_token.type == TOKEN_DEDENT) {
			if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
				return err;
		} else {
			PARSER_ERR("for_body");
		}
	} else {
		if ((err = stmt(parser)) != ERR_OK)
			return err;
	}

	return ERR_OK;
}

ErrorCode exp_cont(Parser *parser, DataType left_type, DataType *out_type) {
	ErrorCode err = ERR_OK;
	TokenType t = parser->current_token.type;

	if (t == TOKEN_OP_PLUS || t == TOKEN_OP_MINUS || t == TOKEN_OP_MUL || t == TOKEN_OP_DIV || t == TOKEN_COM_EQ ||
	    t == TOKEN_COM_NE || t == TOKEN_COM_LT || t == TOKEN_COM_LE || t == TOKEN_COM_GT || t == TOKEN_COM_GE ||
	    t == TOKEN_OP_AND || t == TOKEN_OP_OR) {

		TokenType op = t;
		if ((err = compare_token(parser, t)) != ERR_OK)
			return err;

		DataType right_type;
		if ((err = val(parser, &right_type)) != ERR_OK)
			return err;

		DataType res_type = get_binary_op_type(left_type, right_type, op);
		if (res_type == TYPE_UNKNOWN) {
			return ERR_SEMANTIC_TYPE;
		}

		return exp_cont(parser, res_type, out_type);
	}

	*out_type = left_type;
	return ERR_OK;
}

ErrorCode parse_expr(Parser *parser, DataType *out_type) {
	ErrorCode err = ERR_OK;
	DataType left_type;
	if ((err = val(parser, &left_type)) != ERR_OK)
		return err;
	return exp_cont(parser, left_type, out_type);
}

ErrorCode type_func(Parser *parser, DataType *out_type) { return type(parser, out_type); }

void parser_free(Parser *parser) {
	if (parser != NULL) {
		token_free(&parser->current_token);
	}
}