/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: error codes
 */

#ifndef ERROR_H
#define ERROR_H

typedef enum {
	/** successful translation */
	ERR_OK = 0,

	/** wrong structure of the current lexeme, comment or indentation */
	ERR_LEXICAL = 1,

	/** invalid syntax, missing prologue, disallowed construct, or violation of indented block structure */
	ERR_PARSING = 2,

	/** use of undefined function, variable, or type; missing function annotated with @main */
	ERR_SEMANTIC_UNDEFINED = 3,

	/** illegal redefinition of a function, variable, or type */
	ERR_SEMANTIC_REDEFINITION = 4,

	/** incorrect number or type of arguments in a function call; disallowed parameters for a function annotated with
	@main */
	ERR_SEMANTIC_FUNCTION_CALL = 5,

	/** type compatibility or type inference error in an expression, assignment, initialization, return value, or
	control construct */
	ERR_SEMANTIC_COMPATIBILITY = 6,

	/** attempt to write to a non-modifiable variable or a formal function parameter */
	ERR_SEMANTIC_FORBIDDEN_WRITING = 7,

	/** other semantic errors */
	ERR_SEMANTIC_OTHER = 10,

	/** internal compiler error */
	ERR_INTERNAL_COMPILER = 99,
} ErrorCode;

#endif