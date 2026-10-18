/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: buffer for sending strings in tokens
 */

#ifndef STRING_BUFFER
#define STRING_BUFFER

#include <stdlib.h>
#include <string.h>

typedef struct {
	char *data;
	int lenght;
	int capacity;
} string;

int string_init(string *s);
int string_add_char(string *s, char c);
void string_clear(string *s);
void string_free(string *s);

#endif