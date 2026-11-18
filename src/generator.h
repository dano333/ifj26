/*
 * Project: IFJ26
 * Authors: David Pavlištík (xpavlid00)
 * Description: generator
 */

#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>
#include "ast.h"

/**
 * @brief Initialization of generator.
 * @param out Pointer for output (stdout/file).
 */
void generator_init(FILE *out);

/**
 * @brief Passes trough AST and generates IFJcode26.
 * @param node Pointer for root node of AST.
 */
void generate_program(AstNode *node);

#endif