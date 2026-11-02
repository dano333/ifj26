/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: growable stack of indentation levels used by the scanner
 */

#include "indent_stack.h"
#include <stdlib.h>

#define INDENT_STACK_INITIAL_CAPACITY 16

ErrorCode indent_stack_init(IndentStack *stack) {
	stack->data = malloc(INDENT_STACK_INITIAL_CAPACITY * sizeof(int));
	if (stack->data == NULL) {
		stack->size = 0;
		stack->capacity = 0;
		return ERR_INTERNAL_COMPILER;
	}
	stack->capacity = INDENT_STACK_INITIAL_CAPACITY;
	stack->data[0] = 0;
	stack->size = 1;
	return ERR_OK;
}

ErrorCode indent_stack_push(IndentStack *stack, int level) {
	if (stack->size == stack->capacity) {
		int new_capacity = stack->capacity * 2;
		int *new_data = realloc(stack->data, new_capacity * sizeof(int));
		if (new_data == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		stack->data = new_data;
		stack->capacity = new_capacity;
	}
	stack->data[stack->size] = level;
	stack->size++;
	return ERR_OK;
}

void indent_stack_pop(IndentStack *stack) {
	if (stack->size > 1) {
		stack->size--;
	}
}

int indent_stack_top(const IndentStack *stack) { return stack->data[stack->size - 1]; }

void indent_stack_free(IndentStack *stack) {
	free(stack->data);
	stack->data = NULL;
	stack->size = 0;
	stack->capacity = 0;
}
