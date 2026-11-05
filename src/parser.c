/*
 * Project: IFJ26
 * Authors: Nazar Zhyla (xzhylan00)
 * Description: parser
 */


#include "parser.h"
#include "error.h"
#include <stdlib.h>
#include <stdio.h>



static ErrorCode parser_next_token(Parser *parser) {

    token_free(&parser->current_token);
    return get_token(&parser->current_token, parser->scanner);
}

ErrorCode parser_init(Parser *parser, Scanner *scanner){
    parser->scanner = scanner;
    return parser_next_token(parser);
}

ErrorCode compare_token(Parser *parser, TokenType expected) {
    if (parser->current_token.type == expected) {
        return parser_next_token(parser);
    } else {
        return ERR_PARSING;
    }

    
}