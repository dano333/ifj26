/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: built-in functions
 */

#include "builtins.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

/* registers a built-in with one parameter list of `param_count` parameters, given as DataType arguments */
static ErrorCode add_builtin(SymTable *table, const char *name, DataType ret_type, int param_count, ...) {
	ErrorCode err = symtable_insert(table, name, SYMBOL_FUNC, ret_type, false);
	if (err != ERR_OK) {
		return err;
	}

	Symbol *sym = symtable_lookup_local(table, name);
	sym->param_types = param_count > 0 ? malloc((size_t)param_count * sizeof(DataType)) : NULL;
	sym->list_sizes = malloc(sizeof(int));
	if ((param_count > 0 && sym->param_types == NULL) || sym->list_sizes == NULL) {
		return ERR_INTERNAL_COMPILER;
	}

	va_list args;
	va_start(args, param_count);
	for (int i = 0; i < param_count; i++) {
		sym->param_types[i] = va_arg(args, DataType);
	}
	va_end(args);

	sym->param_count = param_count;
	sym->list_count = 1;
	sym->list_sizes[0] = param_count;
	sym->ret_explicit = true;
	sym->defined = true;
	return ERR_OK;
}

ErrorCode register_builtins(SymTable *global_table) {
	ErrorCode err;

	/* TYPE_UNKNOWN as a parameter type means "any type" */
	if ((err = add_builtin(global_table, "strLength", TYPE_INT, 1, TYPE_STRING)) != ERR_OK ||
	    (err = add_builtin(global_table, "ord", TYPE_INT, 2, TYPE_STRING, TYPE_INT)) != ERR_OK ||
	    (err = add_builtin(global_table, "chr", TYPE_STRING, 1, TYPE_INT)) != ERR_OK ||
	    (err = add_builtin(global_table, "subStr", TYPE_STRING, 3, TYPE_STRING, TYPE_INT, TYPE_INT)) != ERR_OK ||
	    (err = add_builtin(global_table, "toStr", TYPE_STRING, 1, TYPE_UNKNOWN)) != ERR_OK ||
	    (err = add_builtin(global_table, "toInt", TYPE_INT, 1, TYPE_DOUBLE)) != ERR_OK ||
	    (err = add_builtin(global_table, "readLine", TYPE_STRING, 0)) != ERR_OK ||
	    (err = add_builtin(global_table, "readInt", TYPE_INT, 0)) != ERR_OK ||
	    (err = add_builtin(global_table, "readDouble", TYPE_DOUBLE, 0)) != ERR_OK ||
	    (err = add_builtin(global_table, "readBoolean", TYPE_BOOLEAN, 0)) != ERR_OK ||
	    (err = add_builtin(global_table, "print", TYPE_UNIT, 1, TYPE_UNKNOWN)) != ERR_OK) {
		return err;
	}
	return ERR_OK;
}