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
 * @brief struktura ktora si pameta aktualny stav parsera takze vsetko co si parser potrebuje pocas
 * prace pametat
 */
typedef struct {
    Lexer *lexer;  ///< lexer z ktoreho parser berie tokeny
    Token current; ///< konkretny token ktory parser aktualne kontroluje
} Parser;

static int Program(Parser *parser);
static int FunList(Parser *parser);
static int Fun(Parser *parser);
static int FunMain(Parser *parser);
static int Params(Parser *parser);
static int FunOptions(Parser *parser);
static int FunHasBody(Parser *parser);
static int RestParams(Parser *parser);
static int MoreParams(Parser *parser);
static int ParamAfterComma(Parser *parser);
static int DataType(Parser *parser);

/**
 * @brief  Uvolni stary token a posunie novy na current
 * @param  parser stav parsera s aktualnym tokenom
 * @return cislo od lexera
 */
static int Advance(Parser *parser) {
    FreeToken(&parser->current);
    return LexerNextToken(parser->lexer, &parser->current);
}

/**
 * @brief Overi, ci je aktualny token toho isteho typu co sme dostali
 * @param parser stav parsera s aktualnym tokenom
 * @param type typ tokenu, ktory ocakavame
 * @return true ak typ sedi, inak false
 */
static bool Check(Parser *parser, TokenType type) { return parser->current.type == type; }

/**
 * @brief Overi typ aktualneho tokenu, ak sedi posunie sa na dalsi, inak vrati chybu
 * @param parser stav parsera s aktualnym tokenom
 * @param type typ tokenu, ktory ocakavame
 * @return syntakticka chyba ak typ nesedi, inak vysledok Advance (0/1/99)
 */
static int Expect(Parser *parser, TokenType type) {
    if (!Check(parser, type)) {
        return 2;
    }
    return Advance(parser);
}

/**
 * @brief Spracovava pravidlo pre cely zoznam parametrov v zatvorkach (napriklad : (a: Int, b: Double))
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int Params(Parser *parser) {
    int result = Expect(parser, TOKEN_LPAREN);
    if (result != 0)
        return result;

    result = RestParams(parser);
    if (result != 0)
        return result;

    return result;
}

/**
 * @brief Spracovava co presne nasleduje za LPAREN (RPAREN alebo prvy parameter)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int RestParams(Parser *parser) {
    if (Check(parser, TOKEN_RPAREN)) {
        int result = Advance(parser);
        return result;
    } else {
        int result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_COLON);
        if (result != 0)
            return result;
        
        result = DataType(parser);
        if (result != 0)
            return result;
        
        result = MoreParams(parser);
        if (result != 0)
            return result;
        
        return result;
    }
}

/**
 * @brief Spracovava co nasleduje po parametri (RPAREN alebo ciarka a dalsi parameter)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int MoreParams(Parser *parser) {
    if (Check(parser, TOKEN_RPAREN)) {
        int result = Advance(parser);
        return result;
    } else {
        int result = Expect(parser, TOKEN_COMMA);
        if (result != 0)
            return result;
        
        result = ParamAfterComma(parser);
        if (result != 0)
            return result;
        
        return result;
    }
}

/**
 * @brief Spracovava co nasleduje po ciarke (dalsi parameter alebo RPAREN)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int ParamAfterComma(Parser *parser) {
    if (Check(parser, TOKEN_RPAREN)) {
        int result = Advance(parser);
        // TODO A1: koncova ciarka
        return result;
    } else {
        int result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_COLON);
        if (result != 0)
            return result;
        
        result = DataType(parser);
        if (result != 0)
            return result;
        
        result = MoreParams(parser);
        if (result != 0)
            return result;
        
        return result;
    }
}

static int DataType(Parser *parser) {
    (void)parser;
    return 0;
}

static int FunOptions(Parser *parser) {
    (void)parser;
    return 0;
}

static int FunHasBody(Parser *parser) {
    (void)parser;
    return 0;
}

/**
 * @brief Spracuje definiciu funkcie v tvare : def meno(parametre) [: typ] = telo
 * @param parser stav parsera s aktualnym tokenom
 * @return vysledok spracovania pravidla definicie funkcie
 */
static int Fun(Parser *parser) {
    int result = Expect(parser, TOKEN_DEF);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_IDENT);
    if (result != 0)
        return result;

    result = Params(parser);
    if (result != 0)
        return result;

    result = FunOptions(parser);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_ASSIGN);
    if (result != 0)
        return result;

    result = FunHasBody(parser);
    if (result != 0)
        return result;

    return result;
}

/**
 * @brief Funkcia ktora pokryva oba povolene zapisy main (main na jednom riadku a main s EOL)
 * @param parser stav parsera s aktualnym tokenom
 * @return Vysledok zapisu main pre funkciu Fun
 */
static int FunMain(Parser *parser) {
    int result = Expect(parser, TOKEN_MAIN);
    if (result != 0)
        return result;
    if (Check(parser, TOKEN_EOL)) {
        result = Advance(parser);
        if (result != 0)
            return result;
    }
    result = Fun(parser);
    return result;
}

/**
 * @brief Spracuje zoznam funkcii za prologom ifj26.* a podla aktualneho tokenu vyberie pravidlo
 * @param parser stav parsera s aktualnym tokenom
 * @return vysledok spracovania funkcii za prologom
 */
static int FunList(Parser *parser) {
    if (Check(parser, TOKEN_DEF)) {
        int result = Fun(parser);
        if (result != 0)
            return result;
        result = FunList(parser);
        return result;
    } else if (Check(parser, TOKEN_MAIN)) {
        int result = FunMain(parser);
        if (result != 0)
            return result;
        result = FunList(parser);
        return result;
    } else {
        return 0;
    }
}

/**
 * @brief Overi pravidlo import ifj26.* a vsetky funkcie za nim az po koniec programu
 * @param parser stav parsera s aktualnym tokenom
 * @return vysledok kontroly pravidla
 */
static int Program(Parser *parser) {
    int result = Expect(parser, TOKEN_IMPORT);
    if (result != 0)
        return result;

    if (!Check(parser, TOKEN_IDENT)) {
        return 2;
    } else if (strcmp(parser->current.value, "ifj26") != 0) {
        return 2;
    }
    result = Advance(parser);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_DOT);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_MUL);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_EOL);
    if (result != 0)
        return result;

    result = FunList(parser);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_EOF);
    if (result != 0)
        return result;

    return result;
}

/**
 * @brief Vykonanie syntaktickej analyzy celeho programu
 * @param lexer z ktoreho parser cita tokeny
 * @return 0 - OK, 1 - lexikalna chyba, 2 - syntakticka chyba, 99 - interna chyba
 */
int Parse(Lexer *lexer) {
    Parser parser;
    parser.lexer = lexer;
    parser.current.value = NULL;
    int result = Advance(&parser);
    if (result != 0) {
        FreeToken(&parser.current);
        return result;
    }
    result = Program(&parser);
    FreeToken(&parser.current);
    return result;
}
