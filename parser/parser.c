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
static int FuncTypeParams(Parser *parser);
static int FuncTypeRest(Parser *parser);
static int FunBody(Parser *parser);
static int Expr(Parser *parser);
static int StmtList(Parser *parser);
static int StmtListRest(Parser *parser);
static int LineType(Parser *parser);
static int SimpleLineRest(Parser *parser);
static int BlockStmt(Parser *parser);
static int SimpleStmt(Parser *parser);
static int DefVar(Parser *parser);
static int VarRest(Parser *parser);
static int RetStmt(Parser *parser);
static int AssignAfter(Parser *parser);

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
 * @brief Spracovava pravidlo pre cely zoznam parametrov v zatvorkach (napriklad : (a: Int, b:
 * Double))
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

/**
 * @brief Spracovava volitelny navratovy typ funkcie ": Typ"
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FunOptions(Parser *parser) {
    if (Check(parser, TOKEN_COLON)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = DataType(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        return 0;
    }
}

/**
 * @brief Spracovava samotny jednoduchy datovy typ (Int, Double, ...) alebo funkcny typ ((T1, T2) =>
 * R)(LAMBDAS)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int DataType(Parser *parser) {
    if (Check(parser, TOKEN_UNIT) || Check(parser, TOKEN_INT) || Check(parser, TOKEN_DOUBLE) ||
        Check(parser, TOKEN_STRING) || Check(parser, TOKEN_BOOL)) {
        int result = Advance(parser);
        if (result != 0)
            return result;
        return result;
    } else if (Check(parser, TOKEN_LPAREN)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = FuncTypeParams(parser);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_RPAREN);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_ARROW);
        if (result != 0)
            return result;

        result = DataType(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        return 2;
    }
}

/**
 * @brief Spracovava zoznam typov v zatvorkach funkcneho typu (moze byt aj prazdny)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FuncTypeParams(Parser *parser) {
    if (Check(parser, TOKEN_RPAREN)) {
        return 0;
    } else {
        int result = DataType(parser);
        if (result != 0)
            return result;

        result = FuncTypeRest(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava dalsie typy za ciarkou vo funkcnom type
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FuncTypeRest(Parser *parser) {
    if (Check(parser, TOKEN_COMMA)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = DataType(parser);
        if (result != 0)
            return result;

        result = FuncTypeRest(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        return 0;
    }
}

/**
 * @brief Spracovava jednoriadkovy vyraz alebo blok na dalsich riadkoch
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FunHasBody(Parser *parser) {
    if (Check(parser, TOKEN_EOL)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = FunBody(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        // TODO A2: blokova lambda nema EOL
        int result = Expr(parser);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_EOL);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava odsadeny blok prikazov medzi INDENT a DEDENT
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FunBody(Parser *parser) {
    int result = Expect(parser, TOKEN_INDENT);
    if (result != 0)
        return result;

    result = StmtList(parser);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_DEDENT);
    if (result != 0)
        return result;

    return result;
}

// TODO: docasny stub, nahradit precedencnou analyzou
static int Expr(Parser *parser) {
    if (Check(parser, TOKEN_IDENT) || Check(parser, TOKEN_INT_LITERAL) ||
        Check(parser, TOKEN_DOUBLE_LITERAL) || Check(parser, TOKEN_STRING_LITERAL)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        return result;
    } else if (Check(parser, TOKEN_LPAREN)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_RPAREN);
        if (result != 0)
            return result;

        return result;
    } else {
        return 2;
    }
}

/**
 * @brief Spracovava zoznam prikazov v bloku (minimalne jeden)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int StmtList(Parser *parser) {
    int result = LineType(parser);
    if (result != 0)
        return result;

    result = StmtListRest(parser);
    if (result != 0)
        return result;

    return result;
}

/**
 * @brief Spracovava dalsie prikazy v bloku kym nepride DEDENT
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int StmtListRest(Parser *parser) {
    if (Check(parser, TOKEN_DEDENT)) {
        return 0;
    } else {
        int result = LineType(parser);
        if (result != 0)
            return result;

        result = StmtListRest(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava blokovy prikaz (if/while) alebo jednoduchy prikaz
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int LineType(Parser *parser) {
    if (Check(parser, TOKEN_IF) || Check(parser, TOKEN_WHILE)) {
        int result = BlockStmt(parser);
        if (result != 0)
            return result;
        return result;
    } else {
        int result = SimpleStmt(parser);
        if (result != 0)
            return result;

        result = SimpleLineRest(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava dalsie prikazy za ";" na tom istom riadku alebo koniec riadku
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int SimpleLineRest(Parser *parser) {
    if (Check(parser, TOKEN_SEMICOLON)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = SimpleStmt(parser);
        if (result != 0)
            return result;

        result = SimpleLineRest(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        int result = Expect(parser, TOKEN_EOL);
        if (result != 0)
            return result;
        return result;
    }
}

static int BlockStmt(Parser *parser) {
    (void)parser;
    return 0;
}

/**
 * @brief Spracovava jednoduchy prikaz (definicia premennej, return, vyraz alebo priradenie)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int SimpleStmt(Parser *parser) {
    if (Check(parser, TOKEN_VAL) || Check(parser, TOKEN_VAR)) {
        int result = DefVar(parser);
        if (result != 0)
            return result;

        return result;
    } else if (Check(parser, TOKEN_RETURN)) {
        int result = RetStmt(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        int result = Expr(parser);
        if (result != 0)
            return result;

        result = AssignAfter(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava definiciu premennej var/val
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int DefVar(Parser *parser) {
    if (Check(parser, TOKEN_VAL) || Check(parser, TOKEN_VAR)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = VarRest(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        return 2;
    }
}

/**
 * @brief Volitelny typ a inicializacia premennej
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int VarRest(Parser *parser) {
    if (Check(parser, TOKEN_COLON)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = DataType(parser);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_ASSIGN);
        if (result != 0)
            return result;

        result = Expr(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        int result = Expect(parser, TOKEN_ASSIGN);
        if (result != 0)
            return result;

        result = Expr(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava prikaz "return" s volitelnou hodnotou
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int RetStmt(Parser *parser) {
    int result = Expect(parser, TOKEN_RETURN);
    if (result != 0)
        return result;

    if (Check(parser, TOKEN_EOL) || Check(parser, TOKEN_SEMICOLON)) {
        return 0;
    } else {
        result = Expr(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava volitelne priradenie ("= výraz" za vyrazom)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int AssignAfter(Parser *parser) {
    if (Check(parser, TOKEN_ASSIGN)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = Expr(parser);
        if (result != 0)
            return result;

        return result;
    } else {
        return 0;
    }
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
