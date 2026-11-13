/**
* Projekt: Implementace překladače imperativního jazyka IFJ26
* @file parser.c
* @author Matej Hudec (xhudecm01)
* @brief Implementacia syntaktickeho analyzatora (parseru) jazyka IFJ26
*/

#include "parser.h"
#include <stdbool.h>

/**
 * @brief struktura ktora si pameta aktualny stav parsera takze vsetko co si parser potrebuje pocas prace pametat
 */
typedef struct{
    Lexer *lexer; ///< lexer z ktoreho parser berie tokeny
    Token current; ///< konkretny token ktory parser aktualne kontroluje
}Parser;

/**
 * @brief  Uvolni stary token a posunie novy na current
 * @param  parser stav parsera s aktualnym tokenom
 * @return cislo od lexera
 */
static int Advance(Parser *parser){
    FreeToken(&parser->current); 
    return LexerNextToken(parser->lexer,&parser->current);
}

/**
 * @brief Overi, ci je aktualny token toho isteho typu co sme dostali
 * @param parser stav parsera s aktualnym tokenom
 * @param type typ tokenu, ktory ocakavame
 * @return true ak typ sedi, inak false
 */
static bool Check(Parser *parser, TokenType type){
    return parser->current.type == type;
}

/**
 * @brief Overi typ aktualneho tokenu, ak sedi posunie sa na dalsi, inak vrati chybu
 * @param parser stav parsera s aktualnym tokenom
 * @param type typ tokenu, ktory ocakavame
 * @return syntakticka chyba ak typ nesedi, inak vysledok Advance (0/1/99)
 */
static int Expect(Parser *parser, TokenType type){
    if (!Check(parser, type)){
        return 2;  
    }
    return Advance(parser);
}
