/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: buffer for sending strings in tokens
 */

#include "string_buffer.h"
#include <stdlib.h>

ErrorCode string_init(StringBuffer *s) {
	s->length = 0;
	s->capacity = 10;
	s->data[0] = '\0';

	s->data = malloc(sizeof(char) * s->capacity);
	if (s->data == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	return ERR_OK;
}

ErrorCode string_add_char(StringBuffer *s, char c) {
	if (s->length + 1 >= s->capacity) {
		char *new_data = realloc(s->data, s->capacity * 2 * sizeof(char));
		if (new_data == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		s->capacity *= 2;
		s->data = new_data;
	}

	s->data[s->length] = c;
	s->length++;
	s->data[s->length] = '\0';
	return ERR_OK;
}

void string_clear(StringBuffer *s) {
	s->data[0] = '\0';
	s->length = 0;
}

void string_free(StringBuffer *s) {
	free(s->data);
	s->data = NULL;
	s->capacity = 0;
	s->length = 0;
}