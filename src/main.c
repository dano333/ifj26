/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: Starts whole translator
 */

#include "builtins.h"
#include "error.h"
#include "parser.h"
#include "scanner.h"
#include "symtable.h"
#include <stdio.h>

int main(void) {
	SymTable global_table;
	symtable_init(&global_table, NULL);

	// Pre-register IFJ26 standard built-in functions
	if (register_builtins(&global_table) != ERR_OK) {
		fprintf(stderr, "Failed to register built-in functions.\n");
		symtable_free(&global_table);
		return ERR_INTERNAL_COMPILER;
	}

	Scanner scanner;
	if (scanner_init(&scanner) != ERR_OK) {
		symtable_free(&global_table);
		return 1;
	}

	Parser parser;
	if (parser_init(&parser, &scanner, &global_table) != ERR_OK) {
		scanner_free(&scanner);
		symtable_free(&global_table);
		return 1;
	}

	// Run the top-level grammar rule
	ErrorCode err = prolog(&parser);
	if (err == ERR_OK) {
		printf("\n[SUCCESS] Parsing & basic semantic setup completed!\n");
	} else {
		fprintf(stderr, "\n[ERROR] Compilation failed with error code %d\n", err);
	}

	parser_free(&parser);
	scanner_free(&scanner);
	symtable_free(&global_table);
	return err;
}