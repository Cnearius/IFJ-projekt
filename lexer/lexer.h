/**
@file lexer.h
@author Oleksandr Savchenko
@brief Deklarace funkci pro lexer
*/

#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdio.h>

typedef struct Lexer Lexer; 
Lexer *LexerCreate(FILE *input);
void LexerFree(Lexer *lexer);
int LexerNextToken(Lexer *lexer, Token *token);
void FreeToken(Token *token);

#endif