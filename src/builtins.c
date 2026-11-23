#include "builtins.h"
#include <stdbool.h>
#include <stdlib.h>

static void add_builtin(SymTable *table, const char *name, DataType ret_type, DataType *params, int param_count) {
	symtable_insert(table, name, SYMBOL_FUNC, ret_type, false);

	Symbol *sym = symtable_lookup_local(table, name);
	if (sym != NULL) {
		sym->type = ret_type;
		sym->param_count = param_count;
		sym->param_types = params;
		// every built-in has exactly one (possibly empty) parameter list: print(x), readInt()
		sym->list_count = 1;
		sym->list_sizes = malloc(sizeof(int));
		if (sym->list_sizes != NULL) {
			sym->list_sizes[0] = param_count;
		}
		sym->ret_explicit = true;
		sym->defined = true;
	}
}

ErrorCode register_builtins(SymTable *global_table) {
	// 1. strLength(value: String): Int
	DataType *p_strLength = malloc(sizeof(DataType) * 1);
	p_strLength[0] = TYPE_STRING;
	add_builtin(global_table, "strLength", TYPE_INT, p_strLength, 1);

	// 2. ord(value: String, index: Int): Int
	DataType *p_ord = malloc(sizeof(DataType) * 2);
	p_ord[0] = TYPE_STRING;
	p_ord[1] = TYPE_INT;
	add_builtin(global_table, "ord", TYPE_INT, p_ord, 2);

	// 3. chr(code: Int): String
	DataType *p_chr = malloc(sizeof(DataType) * 1);
	p_chr[0] = TYPE_INT;
	add_builtin(global_table, "chr", TYPE_STRING, p_chr, 1);

	// 4. subStr(value: String, start: Int, end: Int): String
	DataType *p_subStr = malloc(sizeof(DataType) * 3);
	p_subStr[0] = TYPE_STRING;
	p_subStr[1] = TYPE_INT;
	p_subStr[2] = TYPE_INT;
	add_builtin(global_table, "subStr", TYPE_STRING, p_subStr, 3);

	// 5. toStr(value: Any): String
	DataType *p_toStr = malloc(sizeof(DataType) * 1);
	p_toStr[0] = TYPE_UNKNOWN; // Any type compatibility
	add_builtin(global_table, "toStr", TYPE_STRING, p_toStr, 1);

	// 6. toInt(value: Double): Int
	DataType *p_toInt = malloc(sizeof(DataType) * 1);
	p_toInt[0] = TYPE_DOUBLE;
	add_builtin(global_table, "toInt", TYPE_INT, p_toInt, 1);

	// 7. readLine(): String
	add_builtin(global_table, "readLine", TYPE_STRING, NULL, 0);

	// 8. readInt(): Int
	add_builtin(global_table, "readInt", TYPE_INT, NULL, 0);

	// 9. readDouble(): Double
	add_builtin(global_table, "readDouble", TYPE_DOUBLE, NULL, 0);

	// 10. readBoolean(): Boolean
	add_builtin(global_table, "readBoolean", TYPE_BOOLEAN, NULL, 0);

	// 11. print(value: Any): Unit
	DataType *p_print = malloc(sizeof(DataType) * 1);
	p_print[0] = TYPE_UNKNOWN; // Any type compatibility
	add_builtin(global_table, "print", TYPE_UNIT, p_print, 1);

	return ERR_OK;
}