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
		fprintf(stderr, "\n[SEMANTIC DEBUG] check_variable_assignment: undefined '%s'\n", name);
		return ERR_SEMANTIC_UNDEFINED;
	}
	/* val, function parameters and functions are not writable */
	if (sym->kind != SYMBOL_VAR || !sym->is_mutable) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] check_variable_assignment: '%s' is not modifiable\n", name);
		return ERR_SEMANTIC_FORBIDDEN_WRITING;
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
		if ((left == TYPE_DOUBLE || left == TYPE_INT) && (right == TYPE_DOUBLE || right == TYPE_INT)) {
			return (left == TYPE_DOUBLE || right == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
		}
		return TYPE_UNKNOWN;
	}

	/* TOKEN_COM_NE, EQ, LT, LE, GT, GE are consecutive in the enum */
	if (op >= TOKEN_COM_NE && op <= TOKEN_COM_GE) {
		return TYPE_BOOLEAN;
	}
	if (op == TOKEN_OP_AND || op == TOKEN_OP_OR) {
		return TYPE_BOOLEAN;
	}

	return TYPE_UNKNOWN;
}

ErrorCode validate_function_call(SymTable *scope, const char *func_name, const DataType *arg_types, int arg_count,
                                 const int *list_sizes, int list_count, DataType *out_return_type) {
	Symbol *sym = symtable_lookup(scope, func_name);
	if (sym == NULL) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] validate_function_call failed for: '%s'\n", func_name);
		return ERR_SEMANTIC_UNDEFINED;
	}
	if (sym->kind != SYMBOL_FUNC) {
		return ERR_SEMANTIC_FUNCTION_CALL;
	}

	if (sym->list_count != list_count) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] '%s' expects %d argument lists, got %d\n", func_name, sym->list_count,
		        list_count);
		return ERR_SEMANTIC_FUNCTION_CALL;
	}
	for (int i = 0; i < list_count; i++) {
		if (sym->list_sizes != NULL && list_sizes != NULL && sym->list_sizes[i] != list_sizes[i]) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] '%s': argument list %d expects %d arguments, got %d\n", func_name,
			        i + 1, sym->list_sizes[i], list_sizes[i]);
			return ERR_SEMANTIC_FUNCTION_CALL;
		}
	}
	if (sym->param_count != arg_count) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] '%s' expects %d arguments, got %d\n", func_name, sym->param_count,
		        arg_count);
		return ERR_SEMANTIC_FUNCTION_CALL;
	}

	for (int i = 0; sym->param_types != NULL && i < arg_count; i++) {
		/* TYPE_UNKNOWN in param_types (print/toStr "Any") accepts anything */
		if (sym->param_types[i] != TYPE_UNKNOWN && !is_type_compatible(sym->param_types[i], arg_types[i])) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] '%s': argument %d has wrong type\n", func_name, i + 1);
			return ERR_SEMANTIC_FUNCTION_CALL;
		}
	}

	*out_return_type = sym->type;
	return ERR_OK;
}

const char *type_name(DataType t) {
	switch (t) {
	case TYPE_INT:
		return "Int";
	case TYPE_DOUBLE:
		return "Double";
	case TYPE_STRING:
		return "String";
	case TYPE_BOOLEAN:
		return "Boolean";
	case TYPE_UNIT:
		return "Unit";
	default:
		return "?";
	}
}

void mangle_method(char *buf, size_t size, DataType receiver, const char *method) {
	snprintf(buf, size, "%s.%s", type_name(receiver), method);
}
