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

AstNode *ast_new_return(AstNode *expr, int line, int col) {
    AstNode *node = malloc(sizeof(AstNode));
    if (!node) return NULL;
    node->kind = NODE_RETURN;
    node->line = line;
    node->column = col;
    node->ast.expr_stmt.child = expr; // Prípadne expr_stmt podľa štruktúry v tvojej unii pre NODE_RETURN
    return node;
}

static bool is_f_def(TokenType type) { return (type == TOKEN_KW_DEF || type == TOKEN_SE_AT); }


// =============================================================================================================================
//                                              PROLOG (Pravidlo 1)
// =============================================================================================================================
ErrorCode prolog(Parser *parser , AstNode **out_program_node ) {
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

	AstNode **functions = NULL;
    int func_count = 0;
    AstNode **extensions = NULL;
    int ext_count = 0;

    if ((err = f_list(parser, &functions, &func_count, &extensions, &ext_count)) != ERR_OK) {
        free(functions);
        free(extensions);
        return err;
    }

    if ((err = compare_token(parser, TOKEN_EOF)) != ERR_OK) {
        free(functions);
        free(extensions);
        return err;
    }

	*out_program_node = ast_new_program(functions, func_count, extensions, ext_count, parser->current_token.line, parser->current_token.column);


	return ERR_OK;
}


// =============================================================================================================================
//                                              F_LIST (Pravidlá 2, 3, 4)
// =============================================================================================================================
ErrorCode f_list(Parser *parser, AstNode ***functions, int *func_count, AstNode ***extensions, int *ext_count) {
	ErrorCode err = ERR_OK;
	TokenType current_type = parser->current_token.type;

	if (is_f_def(current_type)) {
		AstNode *func_node = NULL;

		if ((err = f_def(parser, &func_node)) != ERR_OK) {
			return err;
		}

        *func_count += 1;
        *functions = realloc(*functions, (*func_count) * sizeof(AstNode *));
        (*functions)[*func_count - 1] = func_node;

        return f_list(parser, functions, func_count, extensions, ext_count);

	} else if (current_type == TOKEN_KW_EXTENSION) {
		
		if ((err = compare_token(parser, TOKEN_KW_EXTENSION)) != ERR_OK) {
			return err;
		}

		AstNode *ext_node = NULL;

		if ((err = extension(parser, &ext_node)) != ERR_OK) {
			return err;
		}

        *ext_count += 1;
        *extensions = realloc(*extensions, (*ext_count) * sizeof(AstNode *));
        (*extensions)[*ext_count - 1] = ext_node;

        return f_list(parser, functions, func_count, extensions, ext_count);

	} else {
        // Pravidlo 4: <F_List> -> ε
		return ERR_OK;
	}
}


// =============================================================================================================================
//                                              F_DEF (Pravidlo 5)
// =============================================================================================================================
ErrorCode f_def(Parser *parser, AstNode **func_node) {
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

	AstNode **params = NULL;
    int param_count = 0;

	if ((err = param_lists(parser, &params, &param_count)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		free(func_name);
		free(params);
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
		free(func_name);
		return err;
	}
	AstNode *body_node = NULL;
	if ((err = f_body_end(parser, &body_node)) != ERR_OK) {
		parser->current_scope = func_scope.parent;
		symtable_free(&func_scope);
		free(func_name);
		return err;
	}

	*func_node = ast_new_func_def(func_name, NULL, 0, NULL, 0, true, ret_type, body_node,false, parser->current_token.line, parser->current_token.column);
    free(func_name);

	parser->current_scope = func_scope.parent;
	symtable_free(&func_scope);

	return ERR_OK;
}


// =============================================================================================================================
//                                              ANOTACIA (Pravidlá 10, 11)
// =============================================================================================================================
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
	} else {
        // Pravidlo 11: <ANOTACIA> -> ε
    }

	return ERR_OK;
}


// =============================================================================================================================
//                                              EXTENSION (Pravidlo 14)
// =============================================================================================================================
ErrorCode extension(Parser *parser, AstNode **ext_node) {
    ErrorCode err = ERR_OK;
    DataType dummy_type;

    if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
        return err;

    if (parser->current_token.type != TOKEN_ID_IDENT) {
        PARSER_ERR("extension");
    }

    char *ext_name = strdup(parser->current_token.value.string_value);

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        free(ext_name);
        return err;
    }
    if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
        free(ext_name);
        return err;
    }
    if ((err = type(parser, &dummy_type)) != ERR_OK) {
        free(ext_name);
        return err;
    }
    if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK) {
        free(ext_name);
        return err;
    }

    SymTable ext_scope;
    symtable_init(&ext_scope, parser->current_scope);
    parser->current_scope = &ext_scope;

    if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
        parser->current_scope = ext_scope.parent;
        symtable_free(&ext_scope);
        free(ext_name);
        return err;
    }
	
    if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
        parser->current_scope = ext_scope.parent;
        symtable_free(&ext_scope);
        free(ext_name);
        return err;
    }


    AstNode *body_node = NULL;
    if ((err = e_body(parser, &body_node)) != ERR_OK) {
        parser->current_scope = ext_scope.parent;
        symtable_free(&ext_scope);
        free(ext_name);
        return err;
    }

    if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
        parser->current_scope = ext_scope.parent;
        symtable_free(&ext_scope);
        free(ext_name);
        return err;
    }

    *ext_node = ast_new_extension(ext_name, dummy_type, body_node, 0, parser->current_token.line, parser->current_token.column);
    free(ext_name);

    parser->current_scope = ext_scope.parent;
    symtable_free(&ext_scope);

    return ERR_OK;
}


// =============================================================================================================================
//                                              E_BODY (Pravidlo 15)
// =============================================================================================================================
ErrorCode e_body(Parser *parser, AstNode **ext_body_node) {
    ErrorCode err = ERR_OK;
    DataType dummy_type;

    if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK)
        return err;

    if (parser->current_token.type != TOKEN_ID_IDENT) {
        PARSER_ERR("e_body: expected method name");
    }

    char *func_name = strdup(parser->current_token.value.string_value);
    if (!func_name) return ERR_NOMEM;

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        free(func_name);
        return err;
    }

    AstNode **params = NULL;
    int param_count = 0;

    if ((err = param_lists(parser, &params, &param_count)) != ERR_OK) {
        free(func_name);
        return err;
    }

    if ((err = data_type(parser, &dummy_type)) != ERR_OK) {
        free(func_name);
        return err;
    }

    if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
        free(func_name);
        return err;
    }

    AstNode *body_block = NULL;
    if ((err = f_body_end(parser, &body_block)) != ERR_OK) {
        free(func_name);
        return err;
    }


    *ext_body_node = ast_new_func_def(func_name, params, param_count, NULL, 0, true, dummy_type, body_block, false, parser->current_token.line, parser->current_token.column);

    free(func_name);

	AstNode **funcs = NULL;
	int func_count = 0;
	int func_capacity = 0;
	if ((err = eb_next(parser, &funcs, &func_count, &func_capacity)) != ERR_OK)
        return err;

    return ERR_OK;
}


// =============================================================================================================================
//                                              EB_NEXT (Pravidlá 16, 17)
// =============================================================================================================================
ErrorCode eb_next(Parser *parser, AstNode ***funcs, int *func_count, int *func_capacity) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_DEF) {
        // Pravidlo 16: <EB_NEXT> -> <E_BODY>
        AstNode *func_node = NULL;
        
        if ((err = e_body(parser, &func_node)) != ERR_OK) {
            return err;
        }
        
        if (*func_count >= *func_capacity) {
            int new_capacity = (*func_capacity == 0) ? 4 : (*func_capacity * 2);
            AstNode **new_funcs = realloc(*funcs, new_capacity * sizeof(AstNode *));
            if (!new_funcs) {
                ast_free(func_node); 
                return ERR_NOMEM;
            }
            *funcs = new_funcs;
            *func_capacity = new_capacity;
        }

        (*funcs)[*func_count] = func_node;
        (*func_count)++;
		
    } else {
        // Pravidlo 17: <EB_NEXT> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              TYPE (Pravidlá 20–24)
// =============================================================================================================================
ErrorCode type(Parser *parser, DataType *out_type) {
	TokenType current_token = parser->current_token.type;

	if (current_token == TOKEN_KW_INT) {       // Pravidlo 20
		*out_type = TYPE_INT;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_DOUBLE) { // Pravidlo 21
		*out_type = TYPE_DOUBLE;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_STRING) { // Pravidlo 22
		*out_type = TYPE_STRING;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_UNIT) {   // Pravidlo 23
		*out_type = TYPE_UNIT;
		return parser_next_token(parser);
	} else if (current_token == TOKEN_KW_BOOLEAN) { // Pravidlo 24
		*out_type = TYPE_BOOLEAN;
		return parser_next_token(parser);
	} else {
		PARSER_ERR("type");
	}
}


// =============================================================================================================================
//                                              PARAM_LISTS (Pravidlá 6, 7)
// =============================================================================================================================
ErrorCode param_lists(Parser *parser, AstNode ***params, int *param_count) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_SE_LPAREN) {
        // Pravidlo 6: <PARAM_LISTS> -> (<PARAMETER>) <PARAM_LISTS>
		if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
			return err;

		AstNode *param_node = NULL;
		if ((err = parameter(parser, &param_node, params, param_count)) != ERR_OK)
   			return err;

		if (param_node != NULL) {
            *param_count += 1;
            AstNode **new_params = realloc(*params, (*param_count) * sizeof(AstNode *));
            if (new_params == NULL) {
                ast_free(param_node); 
                return ERR_NOMEM;
            }
            *params = new_params;
            (*params)[*param_count - 1] = param_node;
        }

		if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK)
			return err;

		if ((err = param_lists(parser, params, param_count)) != ERR_OK)
			return err;
	} else {
        // Pravidlo 7: <PARAM_LISTS> -> ε
    }

	return ERR_OK;
}


// =============================================================================================================================
//                                              DATA_TYPE (Pravidlá 18, 19)
// =============================================================================================================================
ErrorCode data_type(Parser *parser, DataType *out_type) {
	ErrorCode err = ERR_OK;
	*out_type = TYPE_UNKNOWN;

	if (parser->current_token.type == TOKEN_SE_COLON) {
        // Pravidlo 18: <D_TYPE> -> : <TYPE>
		if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK)
			return err;
		if ((err = type(parser, out_type)) != ERR_OK)
			return err;
	} else {
        // Pravidlo 19: <D_TYPE> -> ε
    }

	return ERR_OK;
}


// =============================================================================================================================
//                                              F_BODY_END (Pravidlá 8, 9)
// =============================================================================================================================
ErrorCode f_body_end(Parser *parser, AstNode **body_node) {
    ErrorCode err = ERR_OK;
    DataType dummy_type;

    while (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;
    }

    if (parser->current_token.type == TOKEN_INDENT) {
        // Pravidlo 8: <F_BODY_END> -> EOL INDENT <F_BODY> DEDENT
        if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
            return err;
            
        if ((err = f_body(parser, body_node)) != ERR_OK)
            return err;
            
        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
                return err;
        } else {
            PARSER_ERR("f_body_end");
        }
    } else {
        // Pravidlo 9: <F_BODY_END> -> <VAL> EOL
        AstNode *expr_node = NULL;
        DataType expr_type;
        if ((err = parse_expr(parser, &expr_node, &expr_type)) != ERR_OK)
            return err;
            
        AstNode **stmt_array = malloc(sizeof(AstNode *));
        if (!stmt_array) {
            ast_free(expr_node);
            return ERR_NOMEM;
        }
        stmt_array[0] = expr_node;
        
        *body_node = ast_new_block(stmt_array, 1, 1, parser->current_token.line, parser->current_token.column);

        if (parser->current_token.type == TOKEN_EOL) {
            if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
                return err;
        }
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              F_BODY (Pravidlo 31)
// =============================================================================================================================
ErrorCode f_body(Parser *parser, AstNode **body_node) {
    ErrorCode err = ERR_OK;
    AstNode **statements = NULL;
    int stmt_count = 0;

    // Pravidlo 31: <F_BODY> -> <STMT> <STMT_NEXT>
    AstNode *first_stmt = NULL;
    if ((err = stmt(parser, &first_stmt)) != ERR_OK) {
        free(statements);
        return err;
    }
    
    if (first_stmt != NULL) {
        stmt_count++;
        AstNode **new_statements = realloc(statements, stmt_count * sizeof(AstNode *));
        if (new_statements == NULL) {
            ast_free(first_stmt); 
            free(statements);
            return ERR_NOMEM;
        }
        statements = new_statements;
        statements[stmt_count - 1] = first_stmt;
    }
    
    if ((err = stmt_next(parser, &statements, &stmt_count)) != ERR_OK) {
        for (int i = 0; i < stmt_count; i++) {
            ast_free(statements[i]);
        }
        free(statements);
        return err;
    }

    *body_node = ast_new_block(statements, stmt_count, 1, parser->current_token.line, parser->current_token.column);

    return ERR_OK;
}


// =============================================================================================================================
//                                              NEW_LINE (Pravidlá 12, 13)
// =============================================================================================================================
ErrorCode new_line(Parser *parser) {
	ErrorCode err = ERR_OK;
	if (parser->current_token.type == TOKEN_EOL) {
        // Pravidlo 12: <NEW_LINE> -> EOL
		if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
			return err;
		}
	} else {
        // Pravidlo 13: <NEW_LINE> -> ε
    }
	return ERR_OK;
}


// =============================================================================================================================
//                                              PARAMETER (Pravidlá 25, 26)
// =============================================================================================================================
ErrorCode parameter(Parser *parser, AstNode **param_node, AstNode ***params, int *param_count){
    ErrorCode err = ERR_OK;
    DataType p_type;

    if (parser->current_token.type == TOKEN_ID_IDENT) {
        // Pravidlo 25: <PARAMETER> -> id: <TYPE> <PARAM_NEXT>
        char *param_name = strdup(parser->current_token.value.string_value);
        if (!param_name) return ERR_NOMEM;

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

        *param_node = ast_new_param(param_name, p_type, parser->current_token.line, parser->current_token.column);
        free(param_name);

        if ((err = param_next(parser, params, param_count)) != ERR_OK)
            return err;
    } else {
        // Pravidlo 26: <PARAMETER> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              PARAM_NEXT (Pravidlá 27, 28)
// =============================================================================================================================
ErrorCode param_next(Parser *parser, AstNode ***params, int *param_count) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_SE_COMMA) {
        // Pravidlo 27: <PARAM_NEXT> -> , <PARAM_TAIL>
        if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK)
            return err;
        if ((err = param_tail(parser, params, param_count)) != ERR_OK)
            return err;
    } else {
        // Pravidlo 28: <PARAM_NEXT> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              PARAM_TAIL (Pravidlá 29, 30)
// =============================================================================================================================
ErrorCode param_tail(Parser *parser, AstNode ***params, int *param_count) {
    ErrorCode err = ERR_OK;
    DataType p_type;

    if (parser->current_token.type == TOKEN_ID_IDENT) {
        // Pravidlo 29: <PARAM_TAIL> -> id: <TYPE> <PARAM_NEXT>
        char *param_name = strdup(parser->current_token.value.string_value);
        if (!param_name) return ERR_NOMEM;

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

        AstNode *param_node = ast_new_param(param_name, p_type, parser->current_token.line, parser->current_token.column);
        free(param_name);

        if (param_node != NULL) {
            *param_count += 1;
            AstNode **new_params = realloc(*params, (*param_count) * sizeof(AstNode *));
            if (new_params == NULL) {
                ast_free(param_node);
                return ERR_NOMEM;
            }
            *params = new_params;
            (*params)[*param_count - 1] = param_node;
        }

        if ((err = param_next(parser, params, param_count)) != ERR_OK)
            return err;
    } else {
        // Pravidlo 30: <PARAM_TAIL> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              RET_VAL (Pravidlá 41, 42)
// =============================================================================================================================
ErrorCode ret_val(Parser *parser, DataType expected_ret_type, AstNode **return_node) {
    ErrorCode err = ERR_OK;
    DataType expr_type = TYPE_UNIT;
    AstNode *expr_node = NULL;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_ID_IDENT || t == TOKEN_LIT_INT || t == TOKEN_LIT_DOUBLE || t == TOKEN_LIT_STRING ||
        t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE || t == TOKEN_SE_LPAREN || t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT ||
        t == TOKEN_KW_IF) {
        
        // Pravidlo 41: <RET_VAL> -> <VAL>
        if ((err = parse_expr(parser, &expr_node, &expr_type)) != ERR_OK) {
            return err;
        }
        
        if (!is_type_compatible(expected_ret_type, expr_type)) {
            fprintf(stderr, "\n[SEMANTIC DEBUG] Return type mismatch: expected=%d, got=%d\n", expected_ret_type,
                    expr_type);
            ast_free(expr_node);
            return ERR_SEMANTIC_COMPATIBILITY;
        }
    } else if (expected_ret_type != TYPE_UNIT && expected_ret_type != TYPE_UNKNOWN) {
        return ERR_SEMANTIC_COMPATIBILITY;
    } else {
        // Pravidlo 42: <RET_VAL> -> ε
    }
    
    *return_node = ast_new_return(expr_node, parser->current_token.line, parser->current_token.column);

    return ERR_OK;
}


// =============================================================================================================================
//                                              VAL (Pravidlá 46, 47)
// =============================================================================================================================
ErrorCode val(Parser *parser, AstNode **out_node, DataType *out_type) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;
    *out_node = NULL;

    if (t == TOKEN_KW_IF) {
        // Pravidlo 47: <VAL> -> <IF_CONTROL>
        if ((err = if_control(parser, out_node)) != ERR_OK) {
            return err;
        }
        *out_type = TYPE_UNKNOWN;
    } else if (t == TOKEN_SE_LPAREN) {
        if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK)
            return err;
        if (parser->current_token.type != TOKEN_SE_RPAREN) {
            AstNode *expr_node = NULL;
            if ((err = parse_expr(parser, &expr_node, out_type)) != ERR_OK) {
                ast_free(expr_node);
                return err;
            }
            *out_node = expr_node;
        } else {
            *out_type = TYPE_UNIT;
        }
        if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK)
            return err;
    } else if (t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT) {
        TokenType op = t;
        if ((err = compare_token(parser, t)) != ERR_OK)
            return err;
        if ((err = val(parser, out_node, out_type)) != ERR_OK)
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
                AstNode *arg_node = NULL;
                
                if ((err = parse_expr(parser, &arg_node, &a_type)) != ERR_OK) {
                    free(id_name);
                    ast_free(arg_node);
                    return err;
                }
                arg_types[arg_count++] = a_type;
                ast_free(arg_node);

                while (parser->current_token.type == TOKEN_SE_COMMA) {
                    if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK) {
                        free(id_name);
                        return err;
                    }
                    AstNode *next_arg_node = NULL;
                    if ((err = parse_expr(parser, &next_arg_node, &a_type)) != ERR_OK) {
                        free(id_name);
                        ast_free(next_arg_node);
                        return err;
                    }
                    if (arg_count < 10)
                        arg_types[arg_count++] = a_type;
                    ast_free(next_arg_node);
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
        // Pravidlo 46: <VAL> -> <EXP> (resp. štandardný termín výrazu)
        PARSER_ERR("val");
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              DEC (Pravidlá 43, 44, 45)
// =============================================================================================================================
ErrorCode dec(Parser *parser, AstNode **expr_node, DataType *out_type) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_OP_ASSIGN) {
        // Pravidlo 43: <DEC> -> = <VAL>
        if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK)
            return err;
        
        if ((err = parse_expr(parser, expr_node, out_type)) != ERR_OK)
            return err;
            
    } else if (t == TOKEN_OP_PLUS || t == TOKEN_OP_MINUS || t == TOKEN_OP_MUL || t == TOKEN_OP_DIV ||
               t == TOKEN_COM_EQ || t == TOKEN_COM_NE || t == TOKEN_COM_LT || t == TOKEN_COM_LE || t == TOKEN_COM_GT ||
               t == TOKEN_COM_GE || t == TOKEN_OP_AND || t == TOKEN_OP_OR || t == TOKEN_SE_DOT ||
               t == TOKEN_SE_LPAREN) {
        
        // Pravidlo 44: <DEC> -> <EXP_CONT>
        DataType left_dummy = TYPE_UNKNOWN;
        if ((err = exp_cont(parser, left_dummy, out_type)) != ERR_OK)
            return err;
            
    } else {
        // Pravidlo 45: <DEC> -> ε
        *expr_node = NULL;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              STMT_NEXT (Pravidlá 32, 33, 34)
// =============================================================================================================================
ErrorCode stmt_next(Parser *parser, AstNode ***statements_out, int *count_out) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_EOL) {
        // Pravidlo 32: <STMT_NEXT> -> EOL <STMT> <STMT_NEXT>
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;
            
        AstNode *next_stmt = NULL;
        if ((err = stmt(parser, &next_stmt)) != ERR_OK)
            return err;
            
        if (next_stmt != NULL) {
            (*count_out)++;
            AstNode **new_statements = realloc(*statements_out, (*count_out) * sizeof(AstNode *));
            if (new_statements == NULL) {
                ast_free(next_stmt);
                return ERR_NOMEM;
            }
            *statements_out = new_statements;
            (*statements_out)[*count_out - 1] = next_stmt;
        }
        
        if ((err = stmt_next(parser, statements_out, count_out)) != ERR_OK)
            return err;

    } else if (t == TOKEN_SE_SEMICOLON) {
        // Pravidlo 33: <STMT_NEXT> -> ; <STMT> <STMT_NEXT>
        if ((err = compare_token(parser, TOKEN_SE_SEMICOLON)) != ERR_OK)
            return err;
        
        AstNode *next_stmt = NULL;
        if ((err = stmt(parser, &next_stmt)) != ERR_OK)
            return err;
            
        if (next_stmt != NULL) {
            (*count_out)++;
            AstNode **new_statements = realloc(*statements_out, (*count_out) * sizeof(AstNode *));
            if (new_statements == NULL) {
                ast_free(next_stmt);
                return ERR_NOMEM;
            }
            *statements_out = new_statements;
            (*statements_out)[*count_out - 1] = next_stmt;
        }
        
        if ((err = stmt_next(parser, statements_out, count_out)) != ERR_OK)
            return err;

    } else if (t == TOKEN_ID_IDENT || t == TOKEN_KW_VAL || t == TOKEN_KW_VAR || t == TOKEN_KW_RETURN ||
               t == TOKEN_KW_IF || t == TOKEN_KW_WHILE || t == TOKEN_KW_FOR) {

        AstNode *next_stmt = NULL;
        if ((err = stmt(parser, &next_stmt)) != ERR_OK)
            return err;
            
        if (next_stmt != NULL) {
            (*count_out)++;
            AstNode **new_statements = realloc(*statements_out, (*count_out) * sizeof(AstNode *));
            if (new_statements == NULL) {
                ast_free(next_stmt);
                return ERR_NOMEM;
            }
            *statements_out = new_statements;
            (*statements_out)[*count_out - 1] = next_stmt;
        }
        
        if ((err = stmt_next(parser, statements_out, count_out)) != ERR_OK)
            return err;
    } else {
        // Pravidlo 34: <STMT_NEXT> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              STMT (Pravidlá 35, 36, 37, 38, 39, 40, 48, 57)
// =============================================================================================================================
ErrorCode stmt(Parser *parser, AstNode **first_stmt) {
	ErrorCode err = ERR_OK;

	if (parser->current_token.type == TOKEN_KW_VAL || parser->current_token.type == TOKEN_KW_VAR) {
		// Pravidlá 35, 36: <STMT> -> val / var id <D_TYPE> = <VAL>
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
		if ((err = parse_expr(parser, &first_stmt, &expr_type)) != ERR_OK) {
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
		// Pravidlo 37: <STMT> -> while <EXP> do EOL INDENT <F_BODY> DEDENT
        DataType cond_type;
        AstNode *cond_node = NULL;
        AstNode *body_node = NULL;
        
        if ((err = compare_token(parser, TOKEN_KW_WHILE)) != ERR_OK)
            return err;
        if ((err = parse_expr(parser, &cond_node, &cond_type)) != ERR_OK) 
            return err;
        if (cond_type != TYPE_BOOLEAN) {
            ast_free(cond_node);
            return ERR_SEMANTIC_TYPE;
        }
        if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
        if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
        if ((err = f_body(parser, &body_node)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
        if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
            ast_free(cond_node);
            ast_free(body_node);
            return err;
        }

        *first_stmt = ast_new_while(cond_node, body_node, parser->current_token.line, parser->current_token.column);

	} else if (parser->current_token.type == TOKEN_KW_RETURN) {
        // Pravidlo 38: <STMT> -> return <RET_VAL>
        if ((err = compare_token(parser, TOKEN_KW_RETURN)) != ERR_OK)
            return err;

        if ((err = ret_val(parser, parser->current_ret_type, first_stmt)) != ERR_OK)
            return err;

    } else if (parser->current_token.type == TOKEN_ID_IDENT) {
		// Pravidlo 39: <STMT> -> id <DEC>
        DataType expr_type;
		if ((err = parse_expr(parser, first_stmt, &expr_type)) != ERR_OK) {
			return err;
		}
		if (parser->current_token.type == TOKEN_OP_ASSIGN) {
			if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK)
				return err;
			DataType rhs_type;
			if ((err = parse_expr(parser, first_stmt, &rhs_type)) != ERR_OK)
				return err;
			if (!is_type_compatible(expr_type, rhs_type)) {
				fprintf(stderr, "\n[SEMANTIC DEBUG] Type mismatch in assignment: left=%d, rhs=%d\n", expr_type,
				        rhs_type);
				return ERR_SEMANTIC_COMPATIBILITY;
			}
		}
	} else if (parser->current_token.type == TOKEN_KW_IF) {
		// Pravidlo 48: <STMT> -> <IF_STMT_ALL>
        if ((err = if_stmt_all(parser, &first_stmt)) != ERR_OK)
			return err;

	} else if (parser->current_token.type == TOKEN_KW_FOR) {
		// Pravidlo 57: <STMT> -> <FOR_STMT>
        if ((err = for_stmt(parser, &first_stmt)) != ERR_OK)
			return err;

	} else {
		// Pravidlo 40: <STMT> -> <OTHER_EXP>
        DataType dummy_type;
		if ((err = parse_expr(parser, first_stmt, &dummy_type)) != ERR_OK) {
			return err;
		}
	}

	return ERR_OK;
}


// =============================================================================================================================
//                                              IF_STMT_ALL (Pravidlá 49, 50, 51)
// =============================================================================================================================
ErrorCode if_stmt_all(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    DataType cond_type;
    AstNode *cond_node = NULL;
    AstNode *then_body = NULL;
    AstNode *else_body = NULL;

    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
            return err;
    }

    // Pravidlo 49: <IF_STMT_ALL> -> if <EXP> then EOL INDENT <F_BODY> DEDENT <IF_NEXT>
    if ((err = parse_expr(parser, &cond_node, &cond_type)) != ERR_OK)
        return err;
    if (cond_type != TYPE_BOOLEAN) {
        ast_free(cond_node);
        return ERR_SEMANTIC_TYPE;
    }

    if (parser->current_token.type == TOKEN_KW_THEN) {
        if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
    }

    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
                ast_free(cond_node);
                return err;
            }
            if ((err = f_body(parser, &then_body)) != ERR_OK) {
                ast_free(cond_node);
                return err;
            }
            if (parser->current_token.type == TOKEN_DEDENT) {
                if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                    ast_free(cond_node);
                    ast_free(then_body);
                    return err;
                }
            }
        } else {
            if ((err = stmt(parser, &then_body)) != ERR_OK) {
                ast_free(cond_node);
                return err;
            }
        }
    } else {
        if ((err = stmt(parser, &then_body)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
    }

    if ((err = if_next(parser, &else_body)) != ERR_OK) {
        ast_free(cond_node);
        ast_free(then_body);
        return err;
    }

    *out_node = ast_new_if(cond_node, then_body, else_body, parser->current_token.line, parser->current_token.column);

    return ERR_OK;
}


// =============================================================================================================================
//                                              IF_NEXT (Pravidlá 50, 51)
// =============================================================================================================================
ErrorCode if_next(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_KW_ELSE) {
        // Pravidlo 50: <IF_NEXT> -> else <ELSE_NEXT>
        if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK)
            return err;
        if ((err = else_next(parser, out_node)) != ERR_OK)
            return err;
    } else {
        // Pravidlo 51: <IF_NEXT> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              ELSE_NEXT (Pravidlá 52, 53)
// =============================================================================================================================
ErrorCode else_next(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_KW_IF) {
        // Pravidlo 52: <ELSE_NEXT> -> <IF_STMT_ALL>
        if ((err = if_stmt_all(parser, out_node)) != ERR_OK)
            return err;
    } else if (parser->current_token.type == TOKEN_EOL) {
        // Pravidlo 53: <ELSE_NEXT> -> EOL INDENT <F_BODY> DEDENT
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;
        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
                return err;
            if ((err = f_body(parser, out_node)) != ERR_OK)
                return err;
            if (parser->current_token.type == TOKEN_DEDENT) {
                if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK)
                    return err;
            } 
        } else {
            if ((err = stmt(parser, out_node)) != ERR_OK)
                return err;
        }
    } else {
        if ((err = stmt(parser, out_node)) != ERR_OK)
            return err;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              IF_CONTROL (Pravidlo 54)
// =============================================================================================================================
ErrorCode if_control(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    DataType cond_type;
    AstNode *cond_node = NULL;
    AstNode *then_body = NULL;
    AstNode *else_body = NULL;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK)
            return err;
    }

    // Pravidlo 54: <IF_CONTROL> -> if <EXP> then <IF_CONTROL_NEXT> else <IF_CONTROL_NEXT>
    if ((err = parse_expr(parser, &cond_node, &cond_type)) != ERR_OK)
        return err;
        
    if (cond_type != TYPE_BOOLEAN) {
        ast_free(cond_node);
        return ERR_SEMANTIC_TYPE;
    }

    if (parser->current_token.type == TOKEN_KW_THEN) {
        if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
            ast_free(cond_node);
            return err;
        }
    }

    if ((err = if_control_next(parser, &then_body)) != ERR_OK) {
        ast_free(cond_node);
        return err;
    }

    if (parser->current_token.type == TOKEN_KW_ELSE) {
        if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK) {
            ast_free(cond_node);
            ast_free(then_body);
            return err;
        }
    }

    if ((err = if_control_next(parser, &else_body)) != ERR_OK) {
        ast_free(cond_node);
        ast_free(then_body);
        return err;
    }

    *out_node = ast_new_if(cond_node, then_body, else_body, parser->current_token.line, parser->current_token.column);

    return ERR_OK;
}


// =============================================================================================================================
//                                              IF_CONTROL_NEXT (Pravidlá 55, 56)
// =============================================================================================================================
ErrorCode if_control_next(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    DataType dummy_type;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_EOL) {
        // Pravidlo 56: <IF_CONTROL_NEXT> -> EOL INDENT <F_BODY> DEDENT
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;
            
        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
                return err;
        } 
        
        if ((err = f_body(parser, out_node)) != ERR_OK)
            return err;
            
        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                ast_free(*out_node);
                *out_node = NULL;
                return err;
            }
        }
    } else {
        // Pravidlo 55: <IF_CONTROL_NEXT> -> <VAL>
        if ((err = parse_expr(parser, out_node, &dummy_type)) != ERR_OK)
            return err;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              FOR_STMT (Pravidlo 58)
// =============================================================================================================================
ErrorCode for_stmt(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_KW_FOR) {
        // Pravidlo 58: <FOR_STMT> -> for <FOR_CONTENT>
        if ((err = compare_token(parser, TOKEN_KW_FOR)) != ERR_OK)
            return err;
    }

    if ((err = for_content(parser, out_node)) != ERR_OK)
        return err;

    return ERR_OK;
}


// =============================================================================================================================
//                                              FOR_CONTENT (Pravidlá 59, 60)
// =============================================================================================================================
ErrorCode for_content(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    *out_node = NULL;

    char *var_name = NULL;
    AstNode *from_node = NULL;
    AstNode *to_node = NULL;
    AstNode *step_node = NULL;
    bool is_until = false;
    AstNode **guards = NULL;
    int guard_count = 0;
    AstNode *body_node = NULL;

    if (parser->current_token.type == TOKEN_EOL) {
        // Pravidlo 60: <FOR_CONTENT> -> EOL INDENT <GENERATOR> <EOL_GUARDS> DEDENT do <FOR_BODY>
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;

        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
                return err;
        } else {
            PARSER_ERR("for_content");
        }

        if ((err = generator(parser, &var_name, &from_node, &to_node, &is_until, &step_node)) != ERR_OK)
            return err;

        if ((err = eol_guards(parser, &guards, &guard_count)) != ERR_OK) {
            free(var_name);
            ast_free(from_node);
            ast_free(to_node);
            ast_free(step_node);
            return err;
        }

        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                free(var_name);
                ast_free(from_node);
                ast_free(to_node);
                ast_free(step_node);
                for (int i = 0; i < guard_count; i++) ast_free(guards[i]);
                free(guards);
                return err;
            }
        }

    } else {
        // Pravidlo 59: <FOR_CONTENT> -> <GENERATOR> <OPT_GUARDS> do <FOR_BODY>
        if ((err = generator(parser, &var_name, &from_node, &to_node, &is_until, &step_node)) != ERR_OK)
            return err;

        if ((err = opt_guards(parser, &guards, &guard_count)) != ERR_OK) {
            free(var_name);
            ast_free(from_node);
            ast_free(to_node);
            ast_free(step_node);
            return err;
        }
    }

    if (parser->current_token.type == TOKEN_KW_DO) {
        if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK) {
            free(var_name);
            ast_free(from_node);
            ast_free(to_node);
            ast_free(step_node);
            for (int i = 0; i < guard_count; i++) ast_free(guards[i]);
            free(guards);
            return err;
        }
    } else {
        PARSER_ERR("for_content");
        for (int i = 0; i < guard_count; i++) ast_free(guards[i]);
        free(guards);
    }

    if ((err = for_body(parser, &body_node)) != ERR_OK) {
        free(var_name);
        ast_free(from_node);
        ast_free(to_node);
        ast_free(step_node);
        for (int i = 0; i < guard_count; i++) ast_free(guards[i]);
        free(guards);
        return err;
    }

    *out_node = ast_new_for(var_name, from_node, to_node, is_until, step_node, guards, guard_count, body_node, parser->current_token.line, parser->current_token.column);

    free(guards);
    free(var_name);
    return ERR_OK;
}


// =============================================================================================================================
//                                              GENERATOR (Pravidlo 61)
// =============================================================================================================================
ErrorCode generator(Parser *parser, char **out_var_name, AstNode **out_from, AstNode **out_to, bool *out_is_until, AstNode **out_step) {
    ErrorCode err = ERR_OK;
    DataType start_type, end_type;
    *out_var_name = NULL;
    *out_from = NULL;
    *out_to = NULL;
    *out_step = NULL;
    *out_is_until = false;

    // Pravidlo 61: <GENERATOR> -> id <- <EXP> <RANGE> <EXP> <OPT_STEP>
    if (parser->current_token.type != TOKEN_ID_IDENT) {
        PARSER_ERR("generator");
    }
    
    *out_var_name = strdup(parser->current_token.value.string_value);
    if (!*out_var_name) return ERR_NOMEM;

    symtable_insert(parser->current_scope, *out_var_name, SYMBOL_VAR, TYPE_INT, true);

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        return err;
    }

    if (parser->current_token.type != TOKEN_SE_LARROW) {
        free(*out_var_name);
        *out_var_name = NULL;
        PARSER_ERR("generator");
    }
    if ((err = compare_token(parser, TOKEN_SE_LARROW)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        return err;
    }

    if ((err = parse_expr(parser, out_from, &start_type)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        return err;
    }
    if (start_type != TYPE_INT) {
        free(*out_var_name);
        *out_var_name = NULL;
        ast_free(*out_from);
        *out_from = NULL;
        return ERR_SEMANTIC_TYPE;
    }

    if ((err = range_func(parser, out_is_until)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        ast_free(*out_from);
        *out_from = NULL;
        return err;
    }

    if ((err = parse_expr(parser, out_to, &end_type)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        ast_free(*out_from);
        *out_from = NULL;
        return err;
    }
    if (end_type != TYPE_INT) {
        free(*out_var_name);
        *out_var_name = NULL;
        ast_free(*out_from);
        *out_from = NULL;
        ast_free(*out_to);
        *out_to = NULL;
        return ERR_SEMANTIC_TYPE;
    }

    if ((err = opt_step(parser, out_step)) != ERR_OK) {
        free(*out_var_name);
        *out_var_name = NULL;
        ast_free(*out_from);
        *out_from = NULL;
        ast_free(*out_to);
        *out_to = NULL;
        return err;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              RANGE_FUNC (Pravidlá 62, 63)
// =============================================================================================================================
ErrorCode range_func(Parser *parser, bool *is_until) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_TO) {
        // Pravidlo 62: <RANGE> -> to
        if ((err = compare_token(parser, TOKEN_KW_TO)) != ERR_OK)
            return err;
        *is_until = false;
    } else if (parser->current_token.type == TOKEN_KW_UNTIL) {
        // Pravidlo 63: <RANGE> -> until
        if ((err = compare_token(parser, TOKEN_KW_UNTIL)) != ERR_OK)
            return err;
        *is_until = true;
    } else {
        PARSER_ERR("range_func");
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              OPT_STEP (Pravidlá 64, 65)
// =============================================================================================================================
ErrorCode opt_step(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    DataType step_type;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_KW_BY) {
        // Pravidlo 64: <OPT_STEP> -> by <EXP>
        if ((err = compare_token(parser, TOKEN_KW_BY)) != ERR_OK)
            return err;
            
        if ((err = parse_expr(parser, out_node, &step_type)) != ERR_OK)
            return err;
            
        if (step_type != TYPE_INT) {
            ast_free(*out_node);
            *out_node = NULL;
            return ERR_SEMANTIC_TYPE;
        }
    } else {
        // Pravidlo 65: <OPT_STEP> -> ε
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              OPT_GUARDS (Pravidlá 66, 67)
// =============================================================================================================================
ErrorCode opt_guards(Parser *parser, AstNode ***guards_out, int *count_out) {
    ErrorCode err = ERR_OK;
    *guards_out = NULL;
    *count_out = 0;

    // Pravidlá 66, 67: <OPT_GUARDS> -> if <EXP> <OPT_GUARDS> | epsilon
    while (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return err;
        }

        DataType guard_type;
        AstNode *cond_node = NULL;
        if ((err = parse_expr(parser, &cond_node, &guard_type)) != ERR_OK) {
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return err;
        }

        if (guard_type != TYPE_BOOLEAN) {
            ast_free(cond_node);
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return ERR_SEMANTIC_TYPE;
        }

        (*count_out)++;
        AstNode **new_guards = realloc(*guards_out, (*count_out) * sizeof(AstNode *));
        if (!new_guards) {
            ast_free(cond_node);
            for (int i = 0; i < *count_out - 1; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return ERR_NOMEM;
        }
        *guards_out = new_guards;
        (*guards_out)[*count_out - 1] = cond_node;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              EOL_GUARDS (Pravidlá 68, 69)
// =============================================================================================================================
ErrorCode eol_guards(Parser *parser, AstNode ***guards_out, int *count_out) {
    ErrorCode err = ERR_OK;
    *guards_out = NULL;
    *count_out = 0;

    // Pravidlá 68, 69: <EOL_GUARDS> -> EOL if <EXP> <EOL_GUARDS> | epsilon
    while (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return err;
        }

        if (parser->current_token.type != TOKEN_KW_IF) {
            break;
        }

        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return err;
        }

        DataType guard_type;
        AstNode *cond_node = NULL;
        if ((err = parse_expr(parser, &cond_node, &guard_type)) != ERR_OK) {
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return err;
        }

        if (guard_type != TYPE_BOOLEAN) {
            ast_free(cond_node);
            for (int i = 0; i < *count_out; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return ERR_SEMANTIC_TYPE;
        }

        (*count_out)++;
        AstNode **new_guards = realloc(*guards_out, (*count_out) * sizeof(AstNode *));
        if (!new_guards) {
            ast_free(cond_node);
            for (int i = 0; i < *count_out - 1; i++) ast_free((*guards_out)[i]);
            free(*guards_out);
            return ERR_NOMEM;
        }
        *guards_out = new_guards;
        (*guards_out)[*count_out - 1] = cond_node;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              FOR_BODY (Pravidlá 70, 71)
// =============================================================================================================================
ErrorCode for_body(Parser *parser, AstNode **out_node) {
    ErrorCode err = ERR_OK;
    *out_node = NULL;

    if (parser->current_token.type == TOKEN_EOL) {
        // Pravidlo 71: <FOR_BODY> -> EOL INDENT <F_BODY> DEDENT
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK)
            return err;

        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK)
                return err;
        } else {
            PARSER_ERR("for_body");
        }

        if ((err = f_body(parser, out_node)) != ERR_OK)
            return err;

        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                ast_free(*out_node);
                *out_node = NULL;
                return err;
            }
        } 
    } else {
        // Pravidlo 70: <FOR_BODY> -> <STMT>
        if ((err = stmt(parser, out_node)) != ERR_OK)
            return err;
    }

    return ERR_OK;
}


// =============================================================================================================================
//                                              EXP_CONT & PARSE_EXPR (Výrazy)
// =============================================================================================================================
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
        AstNode *right_node = NULL; 
        if ((err = val(parser, &right_node, &right_type)) != ERR_OK) { 
            ast_free(right_node);
            return err;
        }

		DataType res_type = get_binary_op_type(left_type, right_type, op);
		if (res_type == TYPE_UNKNOWN) {
			return ERR_SEMANTIC_TYPE;
		}

		return exp_cont(parser, res_type, out_type);
	}

	*out_type = left_type;
	return ERR_OK;
}

ErrorCode parse_expr(Parser *parser, AstNode **out_node, DataType *out_type) {
	ErrorCode err = ERR_OK;
	DataType left_type;
	if ((err = val(parser, out_node, &left_type)) != ERR_OK)
		return err;
	return exp_cont(parser, left_type, out_type);
}


// =============================================================================================================================
//                                              TYPE_FUNC & FREE
// =============================================================================================================================
ErrorCode type_func(Parser *parser, DataType *out_type) { 
    return type(parser, out_type); 
}

void parser_free(Parser *parser) {
	if (parser != NULL) {
		token_free(&parser->current_token);
	}
}