/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: Semantic analysis
 */

#include "semantics.h"
#include "error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

ErrorCode check_variable_assignment(SymTable *scope, const char *name) {
	Symbol *sym = symtable_lookup(scope, name);
	if (sym == NULL) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] check_variable_assignment failed for: '%s'\n", name);
		return ERR_SEMANTIC_UNDEFINED;
	}
	if (sym->kind != SYMBOL_VAR || !sym->is_mutable) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] check_variable_assignment failed for: '%s'\n", name);
		return ERR_SEMANTIC_COMPATIBILITY;
	}
	return ERR_OK;
}

bool is_type_compatible(DataType target, DataType source) {
	if (target == TYPE_UNKNOWN || source == TYPE_UNKNOWN) {
		return true;
	}
	if (target == source)
		return true;
	if (target == TYPE_DOUBLE && source == TYPE_INT)
		return true;
	return false;
}

DataType get_binary_op_type(DataType left, DataType right, TokenType op) {
	if (op == TOKEN_OP_PLUS || op == TOKEN_OP_MINUS || op == TOKEN_OP_MUL || op == TOKEN_OP_DIV) {
		if (left == TYPE_STRING && right == TYPE_STRING && op == TOKEN_OP_PLUS) {
			return TYPE_STRING;
		}
		if (left == TYPE_DOUBLE || right == TYPE_DOUBLE) {
			return TYPE_DOUBLE;
		}
		if (left == TYPE_INT && right == TYPE_INT) {
			return TYPE_INT;
		}
	}

	if (op >= TOKEN_COM_EQ && op <= TOKEN_COM_GE) {
		return TYPE_BOOLEAN;
	}

	return TYPE_UNKNOWN;
}

ErrorCode validate_function_call(SymTable *scope, const char *func_name, DataType *arg_types, int arg_count, DataType *out_return_type) {
	Symbol *sym = symtable_lookup(scope, func_name);
	if (sym == NULL) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] validate_function_call failed for: '%s'\n", func_name);
		return ERR_SEMANTIC_UNDEFINED;
	}

	if (sym->param_types == NULL) {
		*out_return_type = sym->type;
		return ERR_OK;
	}

	if (sym->param_count != arg_count) {
		return ERR_SEMANTIC_FUNCTION_CALL;
	}

	for (int i = 0; i < arg_count; i++) {
		// TYPE_UNKNOWN in param_types (like for print/toStr Any) accepts anything
		if (sym->param_types[i] != TYPE_UNKNOWN && !is_type_compatible(sym->param_types[i], arg_types[i])) {
			return ERR_SEMANTIC_COMPATIBILITY;
		}
	}

	*out_return_type = sym->type;
	return ERR_OK;
}