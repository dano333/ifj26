/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: built-in functions
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include "error.h"
#include "symtable.h"

// Register standard IFJ26 functions into the global scope
ErrorCode register_builtins(SymTable *global_table);

#endif // BUILTINS_H