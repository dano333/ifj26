/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: Hash table symbol table for semantic analysis
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "error.h"

typedef enum { TYPE_INT, TYPE_DOUBLE, TYPE_STRING, TYPE_BOOLEAN, TYPE_UNIT, TYPE_UNKNOWN } DataType;

typedef enum { SYMBOL_VAR, SYMBOL_VAL, SYMBOL_FUNC } SymbolKind;

typedef struct Symbol {
	char *name;
	SymbolKind kind;
	DataType type;
	int is_mutable;      // 0 for val/params, 1 for var
	struct Symbol *next; // Pointer for collision chaining in the bucket
	int param_count;
	DataType *param_types;
} Symbol;

#define HASH_TABLE_SIZE 101 // A prime number for better hash distribution

typedef struct SymTable {
	Symbol *buckets[HASH_TABLE_SIZE];
	struct SymTable *parent; // Pointer to enclosing scope for lexical scoping
} SymTable;

/**
 * @brief Initializes a symbol table scope.
 *
 * @param table Pointer to the symbol table structure.
 * @param parent Pointer to the parent scope table (or NULL for global scope).
 * @return ErrorCode ERR_OK on success, ERR_INTERNAL_COMPILER on failure.
 */
ErrorCode symtable_init(SymTable *table, SymTable *parent);

/**
 * @brief Frees the symbol table and all entries within this scope.
 *
 * @param table Pointer to the symbol table structure.
 */
void symtable_free(SymTable *table);

/**
 * @brief Inserts a new symbol into the current scope's hash table.
 *
 * Checks for local redefinition and returns ERR_SEMANTIC_REDEFINITION if it exists.
 *
 * @return ErrorCode ERR_OK, ERR_SEMANTIC_REDEFINITION, or ERR_INTERNAL_COMPILER.
 */
ErrorCode symtable_insert(SymTable *table, const char *name, SymbolKind kind, DataType type, int is_mutable);

/**
 * @brief Looks up a symbol starting from the current scope up to parent scopes.
 *
 * @return Pointer to the Symbol if found, or NULL if undefined.
 */
Symbol *symtable_lookup(SymTable *table, const char *name);

/**
 * @brief Looks up a symbol strictly in the current scope (ignoring parents).
 */
Symbol *symtable_lookup_local(SymTable *table, const char *name);

#endif