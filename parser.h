/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser.h
 * @author Matej Hudec (xhudecm01)
 * @brief Rozhranie syntaktickeho analyzatora (parseru) jazyka IFJ26
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "token.h"

/**
 * @brief Vykonanie syntaktickej analyzy celeho programu
 * @param lexer z ktoreho parser cita tokeny
 * @return 0 - OK, 1 - lexikalna chyba, 2 - syntakticka chyba, 99 - interna chyba
 */
int Parse(Lexer *lexer);

#endif
