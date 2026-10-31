/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: growable stack of indentation levels used by the scanner
 */

#ifndef INDENT_STACK_H
#define INDENT_STACK_H

#include "error.h"

/*
 * AI-generated code (Claude Sonnet 5.5).
 * Prompt: "Create the stack of ints (like string_buffer) with all its functions
 * for the scanner's INDENT/DEDENT handling."
 */

typedef struct {
	int *data;    // levels, data[0] is always the base level 0
	int size;     // number of stored levels
	int capacity; // allocated number of levels
} IndentStack;

/**
 * Initializes the stack and pushes the base level 0.
 *
 * @param stack structure
 * @return ERR_OK if successful, ERR_INTERNAL_COMPILER if malloc fails
 */
ErrorCode indent_stack_init(IndentStack *stack);

/**
 * Pushes a new indentation level, grows the stack when it is full.
 *
 * @param stack structure
 * @param level number of spaces of the new level
 * @return ERR_OK if successful, ERR_INTERNAL_COMPILER if realloc fails
 */
ErrorCode indent_stack_push(IndentStack *stack, int level);

/**
 * Removes the top level. The base level 0 is never removed.
 *
 * @param stack structure
 */
void indent_stack_pop(IndentStack *stack);

/**
 * Returns the top level.
 *
 * @param stack structure
 * @return the current (innermost) indentation level
 */
int indent_stack_top(const IndentStack *stack);

/**
 * Frees the memory of the stack.
 *
 * @param stack structure
 */
void indent_stack_free(IndentStack *stack);

#endif
