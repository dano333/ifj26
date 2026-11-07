/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: Hash table symbol table implementation
 */

#include "symtable.h"
#include <stdlib.h>
#include <string.h>

// DJB2 hash function for strings
static unsigned int hash(const char *str) {
	unsigned int hash = 5381;
	int c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}
	return hash % HASH_TABLE_SIZE;
}

ErrorCode symtable_init(SymTable *table, SymTable *parent) {
	for (int i = 0; i < HASH_TABLE_SIZE; i++) {
		table->buckets[i] = NULL;
	}
	table->parent = parent;
	return ERR_OK;
}

void symtable_free(SymTable *table) {
	for (int i = 0; i < HASH_TABLE_SIZE; i++) {
		Symbol *current = table->buckets[i];
		while (current != NULL) {
			Symbol *next = current->next;
			free(current->name);
			free(current);
			current = next;
		}
	}
}

Symbol *symtable_lookup_local(SymTable *table, const char *name) {
	unsigned int idx = hash(name);
	Symbol *current = table->buckets[idx];
	while (current != NULL) {
		if (strcmp(current->name, name) == 0) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

Symbol *symtable_lookup(SymTable *table, const char *name) {
	SymTable *current = table;
	while (current != NULL) {
		Symbol *entry = symtable_lookup_local(current, name);
		if (entry != NULL) {
			return entry;
		}
		current = current->parent; // Walk up to parent/global scope
	}
	return NULL;
}

ErrorCode symtable_insert(SymTable *table, const char *name, SymbolKind kind, DataType type, int is_mutable) {
	if (symtable_lookup_local(table, name) != NULL) {
		return ERR_SEMANTIC_REDEFINITION;
	}

	unsigned int idx = hash(name);
	Symbol *new_symbol = calloc(1, sizeof(Symbol));
	if (new_symbol == NULL) {
		return ERR_INTERNAL_COMPILER;
	}

	new_symbol->name = strdup(name);
	if (new_symbol->name == NULL) {
		free(new_symbol);
		return ERR_INTERNAL_COMPILER;
	}

	new_symbol->kind = kind;
	new_symbol->type = type;
	new_symbol->is_mutable = is_mutable;

	new_symbol->next = table->buckets[idx];
	table->buckets[idx] = new_symbol;

	return ERR_OK;
}