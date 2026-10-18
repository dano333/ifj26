#include "string_buffer.h"

int string_init(string *s) {
	s->lenght = 0;
	s->capacity = 10;
	s->data = malloc(sizeof(char) * s->capacity);
	if (s->data == NULL)
		return 0;

	s->data[0] = '\0';
	return 1;
}

int string_add_char(string *s, char c) {
	if (s->lenght + 1 >= s->capacity) {
		s->capacity *= 2;
		char *new_data = realloc(s->data, s->capacity * sizeof(char));
		if (new_data == NULL) {
			return 0;
		}
		s->data = new_data;
	}

	s->data[s->lenght] = c;
	s->lenght++;
	s->data[s->lenght] = '\0';
	return 1;
}

void string_clear(string *s) {
	s->data[0] = '\0';
	s->lenght = 0;
}

void string_free(string *s) {
	free(s->data);
	s->data = NULL;
	s->capacity = 0;
	s->lenght = 0;
}