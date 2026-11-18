/*
 * Project: IFJ26
 * Authors: David Pavlištík (xpavlid00)
 * Description: generator
 */

#ifndef GENERATOR_H
#define GENERATOR_H

#include "ast.h"
#include <stdio.h>

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