/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file main.c
 * @author Matej Hudec (xhudecm01)
 * @brief Vstupny bod prekladaca: nacita program zo stdin a spusti syntakticku analyzu
 */

#include "parser.h"
#include <stdio.h>

int main(void) {
    Lexer *lexer = LexerCreate(stdin);
    if (lexer == NULL) {
        return 99;
    }
    int result = Parse(lexer);
    LexerFree(lexer);
    return result;
}
