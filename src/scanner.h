/*
 * Project: IFJ26
 * Authors: Daniel Roth (xrothda00)
 * Description: scanner
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "error.h"
#include "token.h"

ErrorCode get_token(Token *token); // token only valid until next call
static TokenType check_for_token(const char *identifier);
ErrorCode scannet_init()

#endif