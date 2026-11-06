/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser
 */

#include "parser.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>



static ErrorCode parser_next_token(Parser *parser) {

    token_free(&parser->current_token);
    return get_token(&parser->current_token, parser->scanner);
}

ErrorCode parser_init(Parser *parser, Scanner *scanner){
    parser->scanner = scanner;
    memset(&parser->current_token, 0, sizeof(Token));
    return parser_next_token(parser);
}

ErrorCode compare_token(Parser *parser, TokenType expected) {
    if (parser->current_token.type == expected) {
        return parser_next_token(parser);
    } else {
        return ERR_PARSING;
    }
}

bool is_f_def(TokenType type) {
    return (type == TOKEN_KW_DEF || type == TOKEN_SE_AT); 
}

ErrorCode prolog(Parser *parser) {
    ErrorCode err = ERR_OK;

    if ((err = compare_token(parser, TOKEN_KW_IMPORT)) != ERR_OK) {
        return err;
    }

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK && 
        strcmp(parser->current_token.value, "ifj26") == 0) {
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
}

ErrorCode f_list (Parser *parser) {
    ErrorCode err = ERR_OK;

    TokenType current_type = parser->current_token.type;

    // 2 for <F_DEF>
    if (is_f_def(current_type)) { 
        if ((err = f_def(parser)) != ERR_OK) {
            return err;
        }
        return f_list(parser);
    } 

    // 3 for <EXTENTION>
    else if(current_type == TOKEN_KW_EXTENSION) {
        if ((err = compare_token(parser, TOKEN_KW_EXTENSION)) != ERR_OK) {
            return err;
        }

        if ((err = extension(parser)) != ERR_OK) {
            return err;
        }
        return f_list(parser);
    } 
    // for epsilonovy prechod
    else 
    {
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

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        return err;
    }


    if ((err = param_lists(parser)) != ERR_OK) {
        return err;
    }

    if ((err = data_type(parser)) != ERR_OK) {
        return err;
    }

    if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
        return err;
    }

    if ((err = f_body_end(parser)) != ERR_OK) {
        return err;
    }

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
            return ERR_PARSING;
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


    if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK) {
        return err;
    }


    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        return err;
    }


    if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
        return err;
    }

    // 4. Dátový typ <TYPE>
    if ((err = type(parser)) != ERR_OK) {
        return err;
    }


    if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK) {
        return err;
    }

    if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
        return err;
    }


    if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
        return err;
    }

    // 8. Telo extension <E_BODY>
    if ((err = e_body(parser)) != ERR_OK) {
        return err;
    }

    if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}

ErrorCode e_body(Parser *parser) {
    ErrorCode err = ERR_OK;

    if ((err = compare_token(parser, TOKEN_KW_DEF)) != ERR_OK) {
        return err;
    }

    if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
        return err;
    }

    // 3. Zoznam parametrov <PARAM_LISTS>
    if ((err = param_lists(parser)) != ERR_OK) {
        return err;
    }

    // 4. Návratový typ <D_TYPE>
    if ((err = data_type(parser)) != ERR_OK) {
        return err;
    }


    if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
        return err;
    }

    // 6. Ukončenie / telo funkcie <F_BODY_END>
    if ((err = f_body_end(parser)) != ERR_OK) {
        return err;
    }

     // 7. Ukončenie / telo funkcie <eb_next>
    if ((err = eb_next(parser)) != ERR_OK) {
        return err;
    }

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

ErrorCode type(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType current_token = parser->current_token.type;


    if (current_token == TOKEN_KW_INT || current_token == TOKEN_KW_DOUBLE || current_token == TOKEN_KW_STRING || 
        current_token == TOKEN_KW_UNIT || current_token == TOKEN_KW_BOOLEAN) {
        
        if ((err = parser_next_token(parser)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING; 
    }

    return ERR_OK;
}

ErrorCode param_lists(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_SE_LPAREN) {

        if ((err = compare_token(parser, TOKEN_SE_LPAREN)) != ERR_OK) {
            return err;
        }


        if ((err = parameter(parser)) != ERR_OK) {
            return err;
        }

        if ((err = compare_token(parser, TOKEN_SE_RPAREN)) != ERR_OK) {
            return err;
        }

        if ((err = param_lists(parser)) != ERR_OK) {
            return err;
        }
    }


    return ERR_OK;
}


ErrorCode data_type(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_SE_COLON) {
        if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
            return err;
        }

        if ((err = type(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}

ErrorCode f_body_end(Parser *parser) {
    ErrorCode err = ERR_OK;

    
    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }

        if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
            return err;
        }

        // 3. Parsoval telo príkazu/príkazov <F_BODY>
        if ((err = f_body(parser)) != ERR_OK) {
            return err;
        }

        if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
            return err;
        }
    } 

    else {
        // 1. Parsovanie hodnoty/výrazu <VAL>
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }

        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}

ErrorCode f_body(Parser *parser) {
    ErrorCode err = ERR_OK;
    if ((err = stmt(parser)) != ERR_OK) return err;
    if ((err = stmt_next(parser)) != ERR_OK) return err;
    return ERR_OK;
}

ErrorCode new_line(Parser *parser){
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


    if (parser->current_token.type == TOKEN_ID_IDENT) {
        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }

        if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
            return err;
        }

        if ((err = type_func(parser)) != ERR_OK) {
            return err;
        }

        if ((err = param_next(parser)) != ERR_OK) {
            return err;
        }
    }
    

    return ERR_OK;
}


ErrorCode param_next(Parser *parser) {
    ErrorCode err = ERR_OK;


    if (parser->current_token.type == TOKEN_SE_COMMA) {
        if ((err = compare_token(parser, TOKEN_SE_COMMA)) != ERR_OK) {
            return err;
        }


        if ((err = param_tail(parser)) != ERR_OK) {
            return err;
        }
    }
  
    return ERR_OK;
}


ErrorCode param_tail(Parser *parser) {
    ErrorCode err = ERR_OK;

     if (parser->current_token.type == TOKEN_ID_IDENT) {

        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }


        if ((err = compare_token(parser, TOKEN_SE_COLON)) != ERR_OK) {
            return err;
        }

        if ((err = type(parser)) != ERR_OK) {
            return err;
        }

        if ((err = param_next(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}

ErrorCode ret_val(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_ID_IDENT || t == TOKEN_LIT_INT || t == TOKEN_LIT_DOUBLE || 
        t == TOKEN_LIT_STRING || t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE || 
        t == TOKEN_SE_LPAREN || t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT || t == TOKEN_KW_IF) {
        
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}



ErrorCode val(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_KW_IF) {
        if ((err = if_control(parser)) != ERR_OK) {
            return err;
        }
    } 
   
    else if (t == TOKEN_ID_IDENT || t == TOKEN_LIT_INT || t == TOKEN_LIT_DOUBLE || 
             t == TOKEN_LIT_STRING || t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE || 
             t == TOKEN_SE_LPAREN || t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT) {
        
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }

    return ERR_OK;
}


ErrorCode dec(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_OP_ASSIGN) {
        if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
            return err;
        }
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }
    } 
   
    else if (t == TOKEN_OP_PLUS || t == TOKEN_OP_MINUS || t == TOKEN_OP_MUL || t == TOKEN_OP_DIV ||
             t == TOKEN_COM_EQ || t == TOKEN_COM_NE || t == TOKEN_COM_LT || t == TOKEN_COM_LE ||
             t == TOKEN_COM_GT || t == TOKEN_COM_GE || t == TOKEN_OP_AND || t == TOKEN_OP_OR ||
             t == TOKEN_SE_DOT || t == TOKEN_SE_LPAREN) {
        
        if ((err = exp_cont(parser)) != ERR_OK) {
            return err;
        }
    }
   
    return ERR_OK;
}


ErrorCode stmt_next(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (t == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
        if ((err = stmt(parser)) != ERR_OK) {
            return err;
        }
        if ((err = stmt_next(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (t == TOKEN_SE_SEMICOLON) {
    
        if ((err = compare_token(parser, TOKEN_SE_SEMICOLON)) != ERR_OK) {
            return err;
        }

        if ((err = stmt(parser)) != ERR_OK) {
            return err;
        }

        if ((err = stmt_next(parser)) != ERR_OK) {
            return err;
        }
    }


    return ERR_OK;
}


ErrorCode stmt(Parser *parser) {
    ErrorCode err = ERR_OK;
    TokenType t = parser->current_token.type;

    if (parser->current_token.type == TOKEN_KW_VAL) {
        if ((err = compare_token(parser, TOKEN_KW_VAL)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }
        if ((err = data_type(parser)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
            return err;
        }
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_KW_VAR) {
        if ((err = compare_token(parser, TOKEN_KW_VAR)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }
        if ((err = data_type(parser)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_OP_ASSIGN)) != ERR_OK) {
            return err;
        }
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_KW_WHILE) {
        if ((err = compare_token(parser, TOKEN_KW_WHILE)) != ERR_OK) {
            return err;
        }
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
            return err;
        }
        if ((err = f_body(parser)) != ERR_OK) {
            return err;
        }
        if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_KW_RETURN) {
        if ((err = compare_token(parser, TOKEN_KW_RETURN)) != ERR_OK) {
            return err;
        }
        if ((err = ret_val(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_ID_IDENT) {
        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }
        if ((err = dec(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = if_stmt_all(parser)) != ERR_OK) {
            return err;
        }
    }
    else if (parser->current_token.type == TOKEN_KW_FOR) {
        if ((err = for_stmt(parser)) != ERR_OK) {
            return err;
        }
    }

    else if (t == TOKEN_LIT_INT || t == TOKEN_LIT_DOUBLE || t == TOKEN_LIT_STRING ||
             t == TOKEN_KW_TRUE || t == TOKEN_KW_FALSE || t == TOKEN_SE_LPAREN ||
             t == TOKEN_OP_MINUS || t == TOKEN_OP_NOT) {
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
    }
    else {
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}


ErrorCode if_stmt_all(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }


    if ((err = exp(parser)) != ERR_OK) {
        return err;
    }

    if (parser->current_token.type == TOKEN_KW_THEN) {
        if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }

    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }

    if (parser->current_token.type == TOKEN_INDENT) {
        if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }

    if ((err = f_body(parser)) != ERR_OK) {
        return err;
    }

    if (parser->current_token.type == TOKEN_DEDENT) {
        if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }

    if ((err = if_next(parser)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}

ErrorCode if_next(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_ELSE) {
        if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK) {
            return err;
        }
        if ((err = else_next(parser)) != ERR_OK) {
            return err;
        }
    }


    return ERR_OK;
}


ErrorCode else_next(Parser *parser) {
    ErrorCode err = ERR_OK;


    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = if_stmt_all(parser)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
                return err;
            }
        } else {
            return ERR_PARSING;
        }
        if ((err = f_body(parser)) != ERR_OK) {
            return err;
        }
        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                return err;
            }
        } else {
            return ERR_PARSING;
        }
    } 
    else {
        return ERR_PARSING;
    }

    return ERR_OK;
}


ErrorCode if_control(Parser *parser) {
    ErrorCode err = ERR_OK;


    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }


    if ((err = exp(parser)) != ERR_OK) {
        return err;
    }


    if (parser->current_token.type == TOKEN_KW_THEN) {
        if ((err = compare_token(parser, TOKEN_KW_THEN)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }


    if ((err = if_control_next(parser)) != ERR_OK) {
        return err;
    }


    if (parser->current_token.type == TOKEN_KW_ELSE) {
        if ((err = compare_token(parser, TOKEN_KW_ELSE)) != ERR_OK) {
            return err;
        }
    } else {
        return ERR_PARSING;
    }


    if ((err = if_control_next(parser)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}


ErrorCode if_control_next(Parser *parser) {
    ErrorCode err = ERR_OK;


    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }
        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
                return err;
            }
        } else {
            return ERR_PARSING;
        }
        if ((err = f_body(parser)) != ERR_OK) {
            return err;
        }
        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                return err;
            }
        } else {
            return ERR_PARSING;
        }
    } 
    else {
        if ((err = val(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}

ErrorCode for_stmt(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_FOR) {
        if ((err = compare_token(parser, TOKEN_KW_FOR)) != ERR_OK) {
            return err;
        }
    } 
    else {
        return ERR_PARSING;
    }

    if ((err = for_content(parser)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}

ErrorCode for_content(Parser *parser) {
    ErrorCode err = ERR_OK;


    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if ((err = generator(parser)) != ERR_OK) {
            return err;
        }

        if ((err = eol_guards(parser)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if (parser->current_token.type == TOKEN_KW_DO) {
            if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if ((err = for_body(parser)) != ERR_OK) {
            return err;
        }
    } 

    else {
        if ((err = generator(parser)) != ERR_OK) {
            return err;
        }

        if ((err = opt_guards(parser)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_KW_DO) {
            if ((err = compare_token(parser, TOKEN_KW_DO)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if ((err = for_body(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}


ErrorCode generator(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_ID_IDENT) {
        if ((err = compare_token(parser, TOKEN_ID_IDENT)) != ERR_OK) {
            return err;
        }
    } 
    else {
        return ERR_PARSING;
    }

    if (parser->current_token.type == TOKEN_SE_LARROW) {
        if ((err = compare_token(parser, TOKEN_SE_LARROW)) != ERR_OK) {
            return err;
        }
    } 
    else {
        return ERR_PARSING;
    }

    if ((err = exp(parser)) != ERR_OK) {
        return err;
    }

    if ((err = range(parser)) != ERR_OK) {
        return err;
    }

    if ((err = exp(parser)) != ERR_OK) {
        return err;
    }

    if ((err = opt_step(parser)) != ERR_OK) {
        return err;
    }

    return ERR_OK;
}


ErrorCode range_func(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_TO) {
        if ((err = compare_token(parser, TOKEN_KW_TO)) != ERR_OK) {
            return err;
        }
    } 
    else if (parser->current_token.type == TOKEN_KW_UNTIL) {
        if ((err = compare_token(parser, TOKEN_KW_UNTIL)) != ERR_OK) {
            return err;
        }
    } 
    else {
        return ERR_PARSING;
    }

    return ERR_OK;
}


ErrorCode opt_step(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_BY) {
        if ((err = compare_token(parser, TOKEN_KW_BY)) != ERR_OK) {
            return err;
        }
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}


ErrorCode opt_guards(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_KW_IF) {
        if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
            return err;
        }
        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }
        if ((err = opt_guards(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}


ErrorCode eol_guards(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_KW_IF) {
            if ((err = compare_token(parser, TOKEN_KW_IF)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if ((err = exp(parser)) != ERR_OK) {
            return err;
        }

        if ((err = eol_guards(parser)) != ERR_OK) {
            return err;
        }
    }


    return ERR_OK;
}


ErrorCode for_body(Parser *parser) {
    ErrorCode err = ERR_OK;

    if (parser->current_token.type == TOKEN_EOL) {
        if ((err = compare_token(parser, TOKEN_EOL)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_INDENT) {
            if ((err = compare_token(parser, TOKEN_INDENT)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }

        if ((err = f_body(parser)) != ERR_OK) {
            return err;
        }

        if (parser->current_token.type == TOKEN_DEDENT) {
            if ((err = compare_token(parser, TOKEN_DEDENT)) != ERR_OK) {
                return err;
            }
        } 
        else {
            return ERR_PARSING;
        }
    } 
    else {
        if ((err = stmt(parser)) != ERR_OK) {
            return err;
        }
    }

    return ERR_OK;
}