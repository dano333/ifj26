/*
- Project: IFJ26
- Authors: Daniel Roth (xrothda00)
- Description: Starts whole translator
 */

#include "error.h"
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include <stdio.h>
#include "symtable.h"

int main(void) {
    // Before scanner starts
    Scanner scanner;
    ErrorCode err = scanner_init(&scanner);
    if (err != ERR_OK) {
        return err;
    }
	ErrorCode register_builtins(SymTable *table);

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

    // Koreňový uzol pre celý AST strom programu
    AstNode *program_node = NULL;
    err = prolog(&parser, &program_node);

    if (err == ERR_OK) {
        printf("Jej ide to\n");
        // Tu môžeš spustiť sémantiku / generovanie kódu nad program_node
        
        ast_free(program_node);
    } else {
        printf("Ultra mega sad\n");
    }

    // Cleanup
    symtable_free(&global_table);
    parser_free(&parser);
    scanner_free(&scanner);
    return ERR_OK;
}