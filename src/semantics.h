/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: Semantic analysis implementation
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "error.h"
#include "symtable.h"
#include "token.h"
#include <stdbool.h>
#include <stddef.h>

// Check mutability when reassigning a variable
ErrorCode check_variable_assignment(SymTable *scope, const char *name);

// Determine resulting type for binary operations (handles Int -> Double conversion)
DataType get_binary_op_type(DataType left, DataType right, TokenType op);

// Check if an argument type can be implicitly assigned/passed to a parameter type
bool is_type_compatible(DataType target_type, DataType source_type);

// Validate function call arity and argument types
ErrorCode validate_function_call(SymTable *scope, const char *func_name, DataType *arg_types, int arg_count,
                                 DataType *out_return_type);

// Human readable name of a type (used e.g. to key extension methods)
const char *type_name(DataType t);

// Builds the symbol-table key of an extension method: "<ReceiverType>.<method>"
void mangle_method(char *buf, size_t size, DataType receiver, const char *method);

#endif // SEMANTICS_H