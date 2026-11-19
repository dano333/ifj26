/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00), Jakub Krčmárik (xkrcmaj00)
 * Description: Starts whole translator
 */

#include "ast.h"
#include "builtins.h"
#include "error.h"
#include "generator.h"
#include "parser.h"
#include "scanner.h"
#include "symtable.h"
#include <stdio.h>

int main(void) {
	// Before scanner starts
	Scanner scanner;
	ErrorCode err = scanner_init(&scanner);
	if (err != ERR_OK) {
		return err;
	}

	// Parser init & global symbol table
	Parser parser;
	SymTable global_table;
	symtable_init(&global_table, NULL);

	// Pre-register IFJ26 standard built-in functions
	if (register_builtins(&global_table) != ERR_OK) {
		fprintf(stderr, "Failed to register built-in functions.\n");
		symtable_free(&global_table);
		scanner_free(&scanner);
		return ERR_INTERNAL_COMPILER;
	}

	err = parser_init(&parser, &scanner, &global_table);
	if (err != ERR_OK) {
		symtable_free(&global_table);
		scanner_free(&scanner);
		return err;
	}

	// Root node for the whole abstract syntax tree of the program
	AstNode *program_node = NULL;
	err = prolog(&parser, &program_node);

	if (err == ERR_OK) {
		generator_init(stdout);
		ErrorCode gen_err = generate_program(program_node);
		ast_free(program_node);

		if (gen_err != ERR_OK) {
			fprintf(stderr, "Code generation failed (generator error %d)\n", gen_err);
			err = gen_err;
		}
	} else {
		fprintf(stderr, "Translation failed (error %d)\n", err);
	}

	// Cleanup
	symtable_free(&global_table);
	parser_free(&parser);
	scanner_free(&scanner);
	return err;
}