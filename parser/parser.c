/**
* Projekt: Implementace překladače imperativního jazyka IFJ26
* @file parser.c
* @author Matej Hudec (xhudecm01)
* @brief Implementacia syntaktickeho analyzatora (parseru) jazyka IFJ26
*/

#include "parser.h"
#include <stdbool.h>
#include <string.h>

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


static int FunList(Parser *parser){
    (void)parser;
    return 0;
}

/**
 * @brief Overi pravidlo import ifj26.* a vsetky funkcie za nim az po koniec programu
 * @param parser stav parsera s aktualnym tokenom
 * @return vysledok kontroly pravidla
 */
static int Program(Parser *parser){
    int result = Expect(parser, TOKEN_IMPORT);
    if (result != 0) return result;

    if(!Check(parser, TOKEN_IDENT)){
        return 2;
    }else if(strcmp(parser->current.value, "ifj26") != 0){
        return 2;
    }
    result = Advance(parser);
    if (result != 0) return result;

    result = Expect(parser, TOKEN_DOT);
    if (result != 0) return result;

    result = Expect(parser, TOKEN_MUL);
    if (result != 0) return result;

    result = Expect(parser, TOKEN_EOL);
    if (result != 0) return result;

    result = FunList(parser);
    if (result != 0) return result;

    result = Expect(parser, TOKEN_EOF);
    if (result != 0) return result;

    return result;
}

/**
* @brief Vykonanie syntaktickej analyzy celeho programu
* @param lexer z ktoreho parser cita tokeny
* @return 0 - OK, 1 - lexikalna chyba, 2 - syntakticka chyba, 99 - interna chyba
*/
int Parse(Lexer *lexer){
    Parser parser;
    parser.lexer = lexer;
    parser.current.value = NULL;
    int result = Advance(&parser);
    if(result != 0){
        FreeToken(&parser.current);
        return result;
    }
    result = Program(&parser);
    FreeToken(&parser.current);
    return result;
}
