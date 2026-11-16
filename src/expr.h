/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: expression parser (precedence syntax analysis) with AST construction
 */

#ifndef EXPR_H
#define EXPR_H

#include "ast.h"
#include "error.h"
#include "parser.h"
#include "symtable.h"
#include "token.h"
#include <stdbool.h>

/**
 * @brief Parses one expression using a precedence table (bottom-up, shift/reduce).
 *
 * Handles + - * / < <= > >= == != && || unary - and !, parentheses, literals,
 * identifiers, (curried) calls f(a)(b, c), member access x.foo / x.foo(a),
 * if-expressions and interpolated strings. Parsing stops in front of the first
 * token that cannot continue the expression (EOL, then, do, ',', ')', ...).
 * Types are checked while reducing, the resulting AST is annotated with types.
 *
 * @param out_node receives the AST of the expression (owned by the caller)
 * @param out_type receives the resulting type (TYPE_UNKNOWN if not known statically)
 */
ErrorCode parse_expr(Parser *parser, AstNode **out_node, DataType *out_type);

/** Parses an interpolated string (TOKEN_INTERP_START ... TOKEN_INTERP_END). */
ErrorCode interp_string(Parser *parser, AstNode **out_node, DataType *out_type);

/** Frees an array of AST nodes together with the array itself. */
void interp_free_parts(AstNode **parts, int count);

/** True if a token can start an expression. */
bool is_expr_start(TokenType type);

#endif