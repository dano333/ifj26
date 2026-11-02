/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: Starts whole translator
 */

#include "error.h"
#include "scanner.h"
#include <stdio.h>

int main(void) {

	// Before scanner starts
	Scanner scanner;
	ErrorCode err = scanner_init(&scanner);
	if (err != ERR_OK) {
		return err;
	}

	// After scanner finishes or error
	scanner_free(&scanner);
	return ERR_OK;
}
