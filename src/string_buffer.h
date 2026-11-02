/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: buffer for sending strings in tokens
 */

#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include "error.h"

typedef struct {
	char *data;
	int length;
	int capacity;
} StringBuffer;

ErrorCode string_init(StringBuffer *s);
ErrorCode string_add_char(StringBuffer *s, char c);
void string_clear(StringBuffer *s);
void string_free(StringBuffer *s);

#endif