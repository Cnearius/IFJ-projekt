/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file parser.c
 * @author Matej Hudec (xhudecm01)
 * @brief Implementacia syntaktickeho analyzatora (parseru) jazyka IFJ26
 */

#include "parser.h"
#include "../codegen.h"
#include "../semantic.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief enum na druh posledneho prikazu
 */
enum StmtKind {
    STMT_NONE,
    STMT_EXPR,
    STMT_UNIT,
    STMT_RETURN,
    STMT_IF,
};

/**
 * @brief struktura ktora si pameta aktualny stav parsera takze vsetko co si parser potrebuje pocas
 * prace pametat
 */
typedef struct {
    Lexer *lexer;               ///< lexer z ktoreho parser berie tokeny
    Token current;              ///< konkretny token ktory parser aktualne kontroluje
    size_t prevLine;            ///< riadok predchadzajuceho tokenu
    size_t tokenCount;          ///< pocet zjedenych tokenov
    Token *fieldOfTokens;       ///< vsetky tokeny zo vstupu za sebou
    size_t count;               ///< kolko tokenov v poli naozaj je
    size_t capacity;            ///< na kolko tokenov je pamet zatial alokovana
    size_t position;            ///< index tokenu ktory je prave v current
    typeId paramTypes[64];      ///< pole na typy parametrov aktualnej funkcie
    const char *paramNames[64]; ///< pole na mena parametrov aktualnej funkcie
    int paramCount;             ///< pocet parametrov aktualnej funkcie
    enum StmtKind lastKind;     ///< druh posledneho vyrazu
    typeId lastComType;         ///< typ posledneho vyrazu
    bool lastIfAllReturn;       ///< ci vsetky vetvy posledneho if koncia returnom
    bool lastIfAllUnit;         ///< ci vsetky vetvy posledneho if koncia Unit
    bool inferPass;             ///< ci prave bezi inferencny prechod
    codeGen *cg;                ///< pointer na generator kodu
    bool genCode;   ///< hovori nam ci sa v tomto prechode generuje kod (len ak je to kompletny
                    ///< prechod)
    bool isMainFun; ///< ci prave spracuvame funckiu main
} Parser;

static int Advance(Parser *parser);
static bool Check(Parser *parser, TokenType type);
static int Expect(Parser *parser, TokenType type);
static int Program(Parser *parser);
static int FunList(Parser *parser);
static int Fun(Parser *parser, bool isMain);
static int FunMain(Parser *parser);
static int Params(Parser *parser);
static int FunOptions(Parser *parser, bool *hasRet, typeId *retType);
static int FunHasBody(Parser *parser);
static int RestParams(Parser *parser);
static int MoreParams(Parser *parser);
static int ParamAfterComma(Parser *parser);
static int DataType(Parser *parser, typeId *outType);
static int FuncTypeParams(Parser *parser);
static int FuncTypeRest(Parser *parser);
static int FunBody(Parser *parser);
static int Expr(Parser *parser, typeId *outType);
static int StmtList(Parser *parser);
static int StmtListRest(Parser *parser);
static int LineType(Parser *parser);
static int SimpleLineRest(Parser *parser);
static int BlockStmt(Parser *parser);
static int SimpleStmt(Parser *parser);
static int DefVar(Parser *parser);
static int VarRest(Parser *parser, bool *hasType, typeId *type, typeId *initType);
static int RetStmt(Parser *parser);
static int AssignAfter(Parser *parser, typeId *valueType);
static int If(Parser *parser, bool *allReturn, bool *allUnit);
static int Loop(Parser *parser);
static int IfRest(Parser *parser, bool *allReturn, bool *allUnit);
static int ElseRest(Parser *parser, bool *allReturn, bool *allUnit);
static int CallArgs(Parser *parser, typeId *argTypes, int *argCount);
static int MemberAccess(Parser *parser, typeId *type);
static int CloseStatement(Parser *parser);
static void BranchEnd(Parser *parser, bool *endsReturn, bool *endsUnit);
static int FunBodyCheck(Parser *parser, const char *name, bool isMain, bool hasRet, typeId retType);
static void ResetParserPosition(Parser *parser);

/**
 * @brief  Nacita vsetky tokeny zo vstupu do pola, skonci po TOKEN_EOF
 * @param  parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FillBuffer(Parser *parser) {
    size_t newCapacity = parser->capacity;
    while (true) {
        if (parser->count == newCapacity) {
            if (newCapacity == 0) {
                newCapacity = 64;
            } else {
                newCapacity = 2 * newCapacity;
            }
            Token *newField = realloc(parser->fieldOfTokens, newCapacity * sizeof(Token));
            if (newField == NULL) {
                return 99;
            } else {
                parser->fieldOfTokens = newField;
                parser->capacity = newCapacity;
            }
        }
        Token token;
        token.value = NULL;
        int result = LexerNextToken(parser->lexer, &token);
        if (result != 0) {
            FreeToken(&token);
            return result;
        }
        parser->fieldOfTokens[parser->count] = token;
        parser->count++;
        if (token.type == TOKEN_EOF) {
            return 0;
        }
    }
}

/**
 * @brief  Uvolni samotne pole a vsetky tokeny v nom
 * @param  parser stav parsera s aktualnym tokenom
 */
static void FreeBuffer(Parser *parser) {
    for (size_t i = 0; i < parser->count; i++) {
        FreeToken(&parser->fieldOfTokens[i]);
    }
    free(parser->fieldOfTokens);
    parser->fieldOfTokens = NULL;
    parser->count = 0;
    parser->capacity = 0;
}

/**
 * @brief  vrati parser na zaciatok pola tokenov
 * @param  parser stav parsera s aktualnym tokenom
 */
static void ResetParserPosition(Parser *parser) {
    parser->position = 0;
    parser->current = parser->fieldOfTokens[0];
    parser->tokenCount = 0;
    parser->prevLine = 0;
}

/**
 * @brief  Preskoci telo funkcie, bud blok alebo jeden riadok
 * @param  parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, 2 ak telo neskonci
 */
static int SkipBody(Parser *parser) {
    int depth = 0;
    if (Check(parser, TOKEN_EOL)) {
        int result = Advance(parser);
        if (result != 0)
            return result;
        result = Expect(parser, TOKEN_INDENT);
        if (result != 0)
            return result;
        depth = 1;
        while (depth > 0) {
            if (Check(parser, TOKEN_INDENT)) {
                depth++;
            } else if (Check(parser, TOKEN_DEDENT)) {
                depth--;
            } else if (Check(parser, TOKEN_EOF)) {
                return 2;
            }
            result = Advance(parser);
            if (result != 0)
                return result;
        }

    } else {
        while (!Check(parser, TOKEN_EOL)) {
            if (Check(parser, TOKEN_EOF)) {
                return 2;
            } else {
                int result = Advance(parser);
                if (result != 0)
                    return result;
            }
        }
        int result = Advance(parser);
        if (result != 0)
            return result;
    }
    return 0;
}

/**
 * @brief  Prvy prechod prejde hlavicky vsetkych funkcii a tela preskoci
 * @param  parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak error kod
 */
static int HeaderPass(Parser *parser) {
    bool hasRet;
    typeId retType;
    bool isMain = false;
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

    while (true) {
        if (Check(parser, TOKEN_MAIN)) {
            isMain = true;
            result = Advance(parser);
            if (result != 0)
                return result;
            if (Check(parser, TOKEN_EOL)) {
                result = Advance(parser);
                if (result != 0)
                    return result;
            }
        } else if (Check(parser, TOKEN_DEF)) {
            result = Advance(parser);
            if (result != 0)
                return result;

            const char *name = parser->current.value;

            result = Expect(parser, TOKEN_IDENT);
            if (result != 0)
                return result;

            result = Params(parser);
            if (result != 0)
                return result;

            result = FunOptions(parser, &hasRet, &retType);
            if (result != 0)
                return result;

            result = FunctionHeader(name, hasRet, retType, parser->paramTypes, parser->paramCount,
                                    isMain);
            if (result != 0)
                return result;

            isMain = false;

            result = Expect(parser, TOKEN_ASSIGN);
            if (result != 0)
                return result;

            result = SkipBody(parser);
            if (result != 0)
                return result;
        } else if (Check(parser, TOKEN_EOF)) {
            return 0;
        } else {
            return 2;
        }
    }
}

/**
 * @brief  Posuva sa na dalsi token v poli, zostane stat na EOF
 * @param  parser stav parsera s aktualnym tokenom
 * @return vzdy 0
 */
static int Advance(Parser *parser) {
    parser->tokenCount++;
    parser->prevLine = parser->current.line;
    if (parser->position + 1 < parser->count) {
        parser->position++;
    }
    parser->current = parser->fieldOfTokens[parser->position];
    return 0;
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
 * @return syntakticka chyba (2) ak typ nesedi inak 0
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
    parser->paramCount = 0;
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
    const char *name = parser->current.value;
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
        typeId type;
        result = DataType(parser, &type);
        if (result != 0)
            return result;

        if (parser->paramCount == 64) {
            return 99;
        } else {
            parser->paramNames[parser->paramCount] = name;
            parser->paramTypes[parser->paramCount] = type;
            parser->paramCount++;
        }

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
    const char *name = parser->current.value;
    if (Check(parser, TOKEN_RPAREN)) {
        if (parser->current.line ==
            parser->prevLine) { // A1: koncova ciarka len ak je medzi nou a ")" novy riadok
            return 2;
        }
        int result = Advance(parser);
        return result;
    } else {
        int result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_COLON);
        if (result != 0)
            return result;

        typeId type;
        result = DataType(parser, &type);
        if (result != 0)
            return result;

        if (parser->paramCount == 64) {
            return 99;
        } else {
            parser->paramNames[parser->paramCount] = name;
            parser->paramTypes[parser->paramCount] = type;
            parser->paramCount++;
        }

        result = MoreParams(parser);
        if (result != 0)
            return result;

        return result;
    }
}

/**
 * @brief Spracovava volitelny navratovy typ funkcie ": Typ"
 * @param parser stav parsera s aktualnym tokenom
 * @param hasRet ci ma funkcia navratovy typ
 * @param retType kam sa zapise navratovy typ
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int FunOptions(Parser *parser, bool *hasRet, typeId *retType) {
    if (Check(parser, TOKEN_COLON)) {
        *hasRet = true;
        int result = Advance(parser);
        if (result != 0)
            return result;
        result = DataType(parser, retType);
        if (result != 0)
            return result;

        return result;
    } else {
        *hasRet = false;
        *retType = TYPE_UNDEFINED;
        return 0;
    }
}

/**
 * @brief Spracovava samotny jednoduchy datovy typ (Int, Double, ...) alebo funkcny typ ((T1, T2) =>
 * R)(LAMBDAS)
 * @param parser stav parsera s aktualnym tokenom
 * @param outType kam sa zapise precitany typ
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int DataType(Parser *parser, typeId *outType) {
    if (Check(parser, TOKEN_UNIT)) {
        *outType = TYPE_UNIT;
        int result = Advance(parser);
        if (result != 0)
            return result;
    } else if (Check(parser, TOKEN_INT)) {
        *outType = TYPE_INT;
        int result = Advance(parser);
        if (result != 0)
            return result;
    } else if (Check(parser, TOKEN_DOUBLE)) {
        *outType = TYPE_DOUBLE;
        int result = Advance(parser);
        if (result != 0)
            return result;
    } else if (Check(parser, TOKEN_STRING)) {
        *outType = TYPE_STRING;
        int result = Advance(parser);
        if (result != 0)
            return result;
    } else if (Check(parser, TOKEN_BOOL)) {
        *outType = TYPE_BOOLEAN;
        int result = Advance(parser);
        if (result != 0)
            return result;
    } else if (Check(parser, TOKEN_LPAREN)) {
        typeId retType;
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

        result = DataType(parser, &retType);
        if (result != 0)
            return result;

        *outType = TYPE_UNDEFINED; // TODO LAMBDAS
        return result;
    } else {
        return 2;
    }
    return 0;
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
        typeId type;
        int result = DataType(parser, &type);
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

        typeId type;
        result = DataType(parser, &type);
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
        typeId exprType;
        int result = Expr(parser, &exprType);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_EOL);
        if (result != 0)
            return result;

        parser->lastKind = STMT_EXPR;
        parser->lastComType = exprType;

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

/**
 * @brief enum pre symboly precedencnej tabulky, poradie podla stlpcov tabulky, PREC_UMINUS je
 * unarne minus mimo tabulky
 */
enum PrecSymbol {
    PREC_I,
    PREC_MUL,
    PREC_DIV,
    PREC_PLUS,
    PREC_MINUS,
    PREC_LT,
    PREC_LTE,
    PREC_GT,
    PREC_GTE,
    PREC_EQ,
    PREC_NEQ,
    PREC_AND,
    PREC_OR,
    PREC_NOT,
    PREC_LPAREN,
    PREC_RPAREN,
    PREC_DOLLAR,
    PREC_UMINUS,
};

/**
 * @brief Precedencna tabulka od Michala, riadok je terminal na zasobniku, stlpec je vstup
 */
static const char PrecTable[17][17] = {
    {' ', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', ' ', ' ', '>', '>'},
    {'<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', ' ', ' ', ' ', ' ', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', ' ', ' ', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', ' ', ' ', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '>', '<', '<', '>', '>'},
    {'<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '<', '<', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '=', ' '},
    {' ', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>', ' ', ' ', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', '<', ' ', ' '},
};

/**
 * @brief Prevadza typ tokenu ktory dostane na symbol tabulky, co do vyrazu nepatri je $
 * @param type konkretny typ tokenu
 * @return prevedeny symbol tabulky
 */
static enum PrecSymbol TokenToPrec(TokenType type) {
    switch (type) {
    case TOKEN_IDENT:
    case TOKEN_INT_LITERAL:
    case TOKEN_STRING_LITERAL:
    case TOKEN_DOUBLE_LITERAL:
    case TOKEN_TRUE_LITERAL:
    case TOKEN_FALSE_LITERAL:
    case TOKEN_INTERPOLATED:
    case TOKEN_UNIT_LITERAL:
        return PREC_I;

    case TOKEN_MUL:
        return PREC_MUL;

    case TOKEN_DIV:
        return PREC_DIV;

    case TOKEN_PLUS:
        return PREC_PLUS;

    case TOKEN_MINUS:
        return PREC_MINUS;

    case TOKEN_LT:
        return PREC_LT;

    case TOKEN_LTE:
        return PREC_LTE;

    case TOKEN_GT:
        return PREC_GT;

    case TOKEN_GTE:
        return PREC_GTE;

    case TOKEN_EQ:
        return PREC_EQ;

    case TOKEN_NEQ:
        return PREC_NEQ;

    case TOKEN_AND:
        return PREC_AND;

    case TOKEN_OR:
        return PREC_OR;

    case TOKEN_NOT:
        return PREC_NOT;

    case TOKEN_RPAREN:
        return PREC_RPAREN;

    case TOKEN_LPAREN:
        return PREC_LPAREN;

    default:
        return PREC_DOLLAR;
    }
}

/**
 * @brief Zisti index symbolu v tabulke, unarne minus pouziva riadok/stlpec !
 * @param s precedencny symbol
 * @return index do tabulky
 */
static int PrecIndex(enum PrecSymbol s) {
    if (s == PREC_UMINUS) {
        return PREC_NOT;
    } else {
        return s;
    }
}

/**
 * @brief Druh polozky zasobnika -> terminal, E alebo znacka
 */
enum TypeOfItem {
    ITEM_TERM,
    ITEM_NONTERM,
    ITEM_MARK,
};

/**
 * @brief Jedna polozka zasobnika
 */
typedef struct {
    enum TypeOfItem type;
    enum PrecSymbol symbol;
    typeId valType; ///< Datovy typ polozky
} StackItem;

/**
 * @brief Zasobnik precedencnej analyzy
 */
typedef struct {
    StackItem FieldOfItems[256];
    int top;
} PrecStack;

/**
 * @brief Najde index najvrchnejsieho terminalu
 * @param stack pointer na zasobnik
 * @return index alebo -1 ak ziadny nie je
 */
static int StackTopTerm(PrecStack *stack) {
    for (int i = stack->top - 1; i >= 0; i--) {
        if (stack->FieldOfItems[i].type == ITEM_TERM) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Vlozi polozku na vrch zasobnika
 * @param stack pointer na zasobnik
 * @param item  polozka na vlozenie
 * @return 0 alebo 99
 */
static int StackPush(PrecStack *stack, StackItem item) {
    if (stack->top == 256) {
        return 99;
    } else {
        stack->FieldOfItems[stack->top] = item;
        stack->top++;
        return 0;
    }
}

/**
 * @brief Vlozi znacku "<" za najvrchnejsi terminal
 * @param stack pointer na zasobnik
 * @param termIndex index najvrchnejsieho terminalu.
 * @return 0 alebo 99
 */
static int StackInsertMark(PrecStack *stack, int termIndex) {
    if (stack->top == 256) {
        return 99;
    }
    for (int i = stack->top - 1; i >= termIndex + 1; i--) {
        stack->FieldOfItems[i + 1] = stack->FieldOfItems[i];
    }
    stack->FieldOfItems[termIndex + 1].type = ITEM_MARK;
    stack->top++;
    return 0;
}

/**
 * @brief Prevod symbolu operatora z tabulky na typ tokenu (unarne minus je TOKEN_MINUS)
 * @param s precedencny symbol operatora
 * @return typ tokenu, EOF ak symbol nie je operator
 */
static TokenType PrecToToken(enum PrecSymbol s) {
    switch (s) {
    case PREC_MUL:
        return TOKEN_MUL;

    case PREC_DIV:
        return TOKEN_DIV;

    case PREC_PLUS:
        return TOKEN_PLUS;

    case PREC_MINUS:
        return TOKEN_MINUS;

    case PREC_LT:
        return TOKEN_LT;

    case PREC_LTE:
        return TOKEN_LTE;

    case PREC_GT:
        return TOKEN_GT;

    case PREC_GTE:
        return TOKEN_GTE;

    case PREC_EQ:
        return TOKEN_EQ;

    case PREC_NEQ:
        return TOKEN_NEQ;

    case PREC_AND:
        return TOKEN_AND;

    case PREC_OR:
        return TOKEN_OR;

    case PREC_NOT:
        return TOKEN_NOT;

    case PREC_UMINUS:
        return TOKEN_MINUS;

    default:
        return TOKEN_EOF;
    }
}

/**
 * @brief Nahradi usek od poslednej znacky jednym E podla pravidiel (i, !E, -E, (), E op E, (E))
 * @param stack pointer na zasobnik
 * @return 0 ak OK, 2 ak usek nezodpoveda ziadnemu pravidlu, 99 plny zasobnik,
 * inak kod semantickej chyby
 */
static int StackReduce(PrecStack *stack) {
    int mark = -1;
    for (int i = stack->top - 1; i >= 0; i--) {
        if (stack->FieldOfItems[i].type == ITEM_MARK) {
            mark = i;
            break;
        }
    }
    if (mark == -1) {
        return 2;
    }

    int count = stack->top - mark - 1;

    bool ok = false;

    typeId newType = TYPE_UNDEFINED;
    int result;

    StackItem *a = &stack->FieldOfItems[mark + 1];
    StackItem *b = &stack->FieldOfItems[mark + 2];
    StackItem *c = &stack->FieldOfItems[mark + 3];

    if (count == 1 && a->type == ITEM_TERM && a->symbol == PREC_I) {
        ok = true;
        newType = a->valType;
    }

    if (count == 2 && a->type == ITEM_TERM && (a->symbol == PREC_NOT || a->symbol == PREC_UMINUS) &&
        b->type == ITEM_NONTERM) {
        ok = true;
        result = Operator(b->valType, TYPE_UNDEFINED, PrecToToken(a->symbol), &newType);
        if (result != 0)
            return result;
    }

    if (count == 2 && a->type == ITEM_TERM && a->symbol == PREC_LPAREN && b->type == ITEM_TERM &&
        b->symbol == PREC_RPAREN) {
        ok = true;
        newType = TYPE_UNIT;
    }

    if (count == 3 && a->type == ITEM_NONTERM && b->type == ITEM_TERM && b->symbol >= PREC_MUL &&
        b->symbol <= PREC_OR && c->type == ITEM_NONTERM) {
        ok = true;
        result = Operator(a->valType, c->valType, PrecToToken(b->symbol), &newType);
        if (result != 0)
            return result;
    }

    if (count == 3 && a->type == ITEM_TERM && a->symbol == PREC_LPAREN && b->type == ITEM_NONTERM &&
        c->type == ITEM_TERM && c->symbol == PREC_RPAREN) {
        ok = true;
        newType = b->valType;
    }

    if (ok != true) {
        return 2;
    }

    stack->top = mark;

    StackItem item;
    item.type = ITEM_NONTERM;
    item.symbol = PREC_I;
    item.valType = newType;
    return StackPush(stack, item);
}

/**
 * @brief Spracovava pristup k clenu .id za operandom (VALMEM), vratane retazenia (n.floor.toInt)
 * @param parser stav parsera s aktualnym tokenom
 * @param type typ toho co je pred bodkou na vstupe a na vystupe typ po pristupe k clenom (VALMEM)
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int MemberAccess(Parser *parser, typeId *type) {
    while (Check(parser, TOKEN_DOT)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        const char *member = parser->current.value;

        result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = ValmemMember(*type, member, type);
        if (result != 0)
            return result;
    }
    return 0;
}

/**
 * @brief Precedencna syntakticka analyza vyrazu, skonci na tokene ktory tam nepatri a ten
 * nezje. Pouziva zasobnik a precedencnu tabulku: pri < vlozi znacku a posunie token, pri > redukuje
 * podla StackReduce, pri = spoji zatvorky. Volania funkcii riesi cez CallArgs, unarne minus
 * rozlisuje podla predchadzajuceho tokenu.
 * @param parser stav parsera s aktualnym tokenom
 * @param outType adresa kam sa zapise typ vyrazu
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int Expr(Parser *parser, typeId *outType) {
    typeId argTypes[64];
    int argCount;
    bool prevOperand = false;
    bool isIdent;
    TokenType tokType;
    const char *tokName;
    typeId opType;
    bool isCall;
    PrecStack stack;
    stack.top = 0;
    StackItem item;
    item.type = ITEM_TERM;
    item.symbol = PREC_DOLLAR;
    item.valType = TYPE_UNDEFINED;
    int result = StackPush(&stack, item);
    if (result != 0)
        return result;
    int depth = 0;
    while (true) {
        enum PrecSymbol a = TokenToPrec(parser->current.type);
        if (a == PREC_RPAREN && depth == 0) {
            a = PREC_DOLLAR;
        }
        if (a == PREC_MINUS && !prevOperand) {
            a = PREC_UMINUS;
        }

        int bi = StackTopTerm(&stack);
        enum PrecSymbol b = stack.FieldOfItems[bi].symbol;

        if (a == PREC_DOLLAR && b == PREC_DOLLAR) {
            if (stack.top == 2 && stack.FieldOfItems[1].type == ITEM_NONTERM) {
                *outType = stack.FieldOfItems[1].valType;
                return 0;
            } else {
                return 2;
            }
        }

        switch (PrecTable[PrecIndex(b)][PrecIndex(a)]) {
        case '<':
            result = StackInsertMark(&stack, bi);
            if (result != 0)
                return result;
            item.type = ITEM_TERM;
            item.symbol = a;
            item.valType = TYPE_UNDEFINED;
            result = StackPush(&stack, item);
            if (result != 0)
                return result;
            prevOperand = (a == PREC_I || a == PREC_RPAREN);
            if (a == PREC_LPAREN) {
                depth++;
            }
            isIdent = Check(parser, TOKEN_IDENT);
            tokType = parser->current.type;
            tokName = parser->current.value;
            result = Advance(parser);
            if (result != 0)
                return result;
            if (isIdent && Check(parser, TOKEN_LPAREN)) {
                isCall = true;
                result = CallArgs(parser, argTypes, &argCount);
                if (result != 0)
                    return result;
            } else {
                isCall = false;
            }
            if (a == PREC_I) {
                opType = TYPE_UNDEFINED;
                if (tokType == TOKEN_INT_LITERAL) {
                    opType = TYPE_INT;
                } else if (tokType == TOKEN_DOUBLE_LITERAL) {
                    opType = TYPE_DOUBLE;
                } else if (tokType == TOKEN_STRING_LITERAL || tokType == TOKEN_INTERPOLATED) {
                    opType = TYPE_STRING;
                } else if (tokType == TOKEN_TRUE_LITERAL || tokType == TOKEN_FALSE_LITERAL) {
                    opType = TYPE_BOOLEAN;
                } else if (tokType == TOKEN_UNIT_LITERAL) {
                    opType = TYPE_UNIT;
                }

                if (tokType == TOKEN_IDENT) {
                    if (isCall) {
                        opType = TYPE_UNDEFINED;
                        result = Call(tokName, argCount, argTypes, &opType);
                        if (result != 0)
                            return result;
                    } else {
                        result = GetVarType(tokName, &opType);
                        if (result != 0)
                            return result;
                    }
                }

                if (parser->genCode) {
                    result = 0;
                    if (tokType == TOKEN_INT_LITERAL) {
                        result = GenPushInt(parser->cg, strtoll(tokName, NULL, 10));
                    } else if (tokType == TOKEN_DOUBLE_LITERAL) {
                        result = GenPushFloat(parser->cg, strtod(tokName, NULL));
                    } else if (tokType == TOKEN_TRUE_LITERAL) {
                        result = GenPushBool(parser->cg, true);
                    } else if (tokType == TOKEN_FALSE_LITERAL) {
                        result = GenPushBool(parser->cg, false);
                    } else if (tokType == TOKEN_UNIT_LITERAL) {
                        result = GenPushUnit(parser->cg);
                    } else if (tokType == TOKEN_IDENT && !isCall) {
                        result = GenPushVar(parser->cg, tokName);
                    } else if (tokType == TOKEN_IDENT && isCall) {
                        result = GenCall(parser->cg, tokName);
                    }
                    if (result != 0)
                        return result;
                }

                result = MemberAccess(parser, &opType);
                if (result != 0)
                    return result;

                stack.FieldOfItems[stack.top - 1].valType = opType;
            }
            break;

        case '=':
            item.type = ITEM_TERM;
            item.symbol = a;
            item.valType = TYPE_UNDEFINED;
            result = StackPush(&stack, item);
            if (result != 0)
                return result;
            prevOperand = (a == PREC_I || a == PREC_RPAREN);
            depth--;
            result = Advance(parser);
            if (result != 0)
                return result;

            if (stack.FieldOfItems[stack.top - 2].type == ITEM_NONTERM) {
                typeId parenType = stack.FieldOfItems[stack.top - 2].valType;
                result = MemberAccess(parser, &parenType);
                if (result != 0)
                    return result;
                stack.FieldOfItems[stack.top - 2].valType = parenType;
            } else {
                typeId parenType = TYPE_UNIT;
                result = MemberAccess(parser, &parenType);
                if (result != 0)
                    return result;
            }

            break;

        case '>':
            result = StackReduce(&stack);
            if (result != 0)
                return result;
            break;

        case ' ':
            return 2;
        }
    }
}

/**
 * @brief Argumenty volania funkcie, kazdy cez Expr
 * @param parser stav parsera s aktualnym tokenom
 * @param argTypes pole na zapisanie typov argumentov
 * @param argCount adresa kam sa zapise pocet argumentov
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int CallArgs(Parser *parser, typeId *argTypes, int *argCount) {
    *argCount = 0;
    int result = Expect(parser, TOKEN_LPAREN);
    if (result != 0)
        return result;
    if (Check(parser, TOKEN_RPAREN)) {
        result = Advance(parser);
        if (result != 0)
            return result;
        return 0;
    }

    while (true) {
        typeId exprType;
        result = Expr(parser, &exprType);
        if (result != 0)
            return result;

        if (*argCount == 64) {
            return 99;
        } else {
            argTypes[*argCount] = exprType;
            (*argCount)++;
        }

        if (Check(parser, TOKEN_COMMA)) {
            result = Advance(parser);
            if (result != 0)
                return result;
            if (Check(parser, TOKEN_RPAREN)) {
                if (parser->current.line == parser->prevLine) { // A1
                    return 2;
                }
                result = Advance(parser);
                if (result != 0)
                    return result;
                return 0;
            }
        } else {
            return Expect(parser, TOKEN_RPAREN);
        }
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
 * @brief Uzatvara odlozeny prikaz, ak to bol vyraz posle jeho type do StatementValueType
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int CloseStatement(Parser *parser) {
    if (parser->lastKind == STMT_EXPR) {
        int result = StatementValueType(parser->lastComType);
        if (result != 0)
            return result;

        if (parser->genCode) {
            result = GenPopDiscard(parser->cg);
            if (result != 0)
                return result;
        }
    }
    parser->lastKind = STMT_NONE;
    return 0;
}

/**
 * @brief Overi ci aktualny token operator, ktorym nesmie zacinat logicky riadok
 * @param parser stav parsera s aktualnym tokenom
 * @return true alebo false
 */
static bool StartsWithOperator(Parser *parser) {
    if (Check(parser, TOKEN_PLUS) || Check(parser, TOKEN_MINUS) || Check(parser, TOKEN_MUL) ||
        Check(parser, TOKEN_DIV) || Check(parser, TOKEN_EQ) || Check(parser, TOKEN_NEQ) ||
        Check(parser, TOKEN_LT) || Check(parser, TOKEN_LTE) || Check(parser, TOKEN_GT) ||
        Check(parser, TOKEN_GTE) || Check(parser, TOKEN_AND) || Check(parser, TOKEN_OR) ||
        Check(parser, TOKEN_ASSIGN) || Check(parser, TOKEN_ARROW)) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Spracovava blokovy prikaz (if/while) alebo jednoduchy prikaz
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int LineType(Parser *parser) {
    if (StartsWithOperator(parser)) {
        return 2;
    }
    int result = CloseStatement(parser);
    if (result != 0)
        return result;
    if (Check(parser, TOKEN_IF) || Check(parser, TOKEN_WHILE)) {
        result = BlockStmt(parser);
        if (result != 0)
            return result;
        return result;
    } else {
        result = SimpleStmt(parser);
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

        result = CloseStatement(parser);
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

/**
 * @brief Spracovava blokovy prikaz (if alebo while)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int BlockStmt(Parser *parser) {
    bool alsoReturn;
    bool alsoUnit;
    if (Check(parser, TOKEN_IF)) {
        int result = If(parser, &alsoReturn, &alsoUnit);
        if (result != 0)
            return result;
        parser->lastKind = STMT_IF;
        parser->lastIfAllReturn = alsoReturn;
        parser->lastIfAllUnit = alsoUnit;
        return result;
    } else {
        int result = Loop(parser);
        if (result != 0)
            return result;
        parser->lastKind = STMT_UNIT;
        return result;
    }
}

/**
 * @brief Zisti ako sa konci vetva if podla posledneho prikazu (return alebo unit)
 * @param parser stav parsera s aktualnym tokenom
 * @param endsReturn miesto kam sa zapise ci vetva konci returnom
 * @param endsUnit miesto kam sa zapise ci vetva konci unit
 */
static void BranchEnd(Parser *parser, bool *endsReturn, bool *endsUnit) {
    if (parser->lastKind == STMT_RETURN ||
        (parser->lastKind == STMT_IF && parser->lastIfAllReturn)) {
        *endsReturn = true;
    } else {
        *endsReturn = false;
    }

    if (parser->lastKind == STMT_UNIT || (parser->lastKind == STMT_IF && parser->lastIfAllUnit) ||
        (parser->lastKind == STMT_EXPR && parser->lastComType == TYPE_UNIT)) {
        *endsUnit = true;
    } else {
        *endsUnit = false;
    }
}

/**
 * @brief Spracovava podmieneny prikaz (if podmienka then) a blok
 * @param parser stav parsera s aktualnym tokenom
 * @param allReturn adresa kam sa zapise ci vsetky vetvy koncia return
 * @param allUnit adresa kam sa zapise ci vsetky vetvy koncia unit
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int If(Parser *parser, bool *allReturn, bool *allUnit) {
    bool thenUnit;
    bool thenReturn;
    bool restReturn;
    bool restUnit;
    int result = Expect(parser, TOKEN_IF);
    if (result != 0)
        return result;

    typeId exprType;
    result = Expr(parser, &exprType);
    if (result != 0)
        return result;

    result = ConditionType(exprType);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_THEN);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_EOL);
    if (result != 0)
        return result;

    result = EnterBlock();
    if (result != 0)
        return result;

    result = FunBody(parser);
    if (result != 0)
        return result;

    BranchEnd(parser, &thenReturn, &thenUnit);

    result = CloseStatement(parser);
    if (result != 0)
        return result;

    result = LeaveBlock();
    if (result != 0)
        return result;

    result = IfRest(parser, &restReturn, &restUnit);
    if (result != 0)
        return result;

    *allReturn = thenReturn && restReturn;
    *allUnit = thenUnit && restUnit;

    return result;
}

/**
 * @brief Spracovava cyklus (while podmienka do) a blok
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int Loop(Parser *parser) {
    int result = Expect(parser, TOKEN_WHILE);
    if (result != 0)
        return result;

    typeId exprType;
    result = Expr(parser, &exprType);
    if (result != 0)
        return result;

    result = ConditionType(exprType);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_DO);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_EOL);
    if (result != 0)
        return result;

    result = EnterBlock();
    if (result != 0)
        return result;

    result = FunBody(parser);
    if (result != 0)
        return result;

    result = CloseStatement(parser);
    if (result != 0)
        return result;

    result = LeaveBlock();
    if (result != 0)
        return result;

    return result;
}

/**
 * @brief Spracovava volitelnu vetvu else
 * @param parser stav parsera s aktualnym tokenom
 * @param allReturn adresa kam sa zapise ci vsetky vetvy koncia return
 * @param allUnit adresa kam sa zapise ci vsetky vetvy koncia unit
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int IfRest(Parser *parser, bool *allReturn, bool *allUnit) {
    if (Check(parser, TOKEN_ELSE)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = ElseRest(parser, allReturn, allUnit);
        if (result != 0)
            return result;

        return result;
    } else {
        *allReturn = false;
        *allUnit = true;
        return 0;
    }
}

/**
 * @brief Spracovava obycajne else s blokom alebo else if
 * @param parser stav parsera s aktualnym tokenom
 * @param allReturn adresa kam sa zapise ci vsetky vetvy koncia return
 * @param allUnit adresa kam sa zapise ci vsetky vetvy koncia unit
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int ElseRest(Parser *parser, bool *allReturn, bool *allUnit) {
    if (Check(parser, TOKEN_EOL)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = EnterBlock();
        if (result != 0)
            return result;

        result = FunBody(parser);
        if (result != 0)
            return result;

        BranchEnd(parser, allReturn, allUnit);

        result = CloseStatement(parser);
        if (result != 0)
            return result;

        result = LeaveBlock();
        if (result != 0)
            return result;

        return result;
    } else if (Check(parser, TOKEN_IF)) {
        int result = If(parser, allReturn, allUnit);
        if (result != 0)
            return result;

        return result;
    } else {
        return 2;
    }
}

/**
 * @brief Spracovava jednoduchy prikaz (definicia premennej, return, vyraz alebo priradenie)
 * @param parser stav parsera s aktualnym tokenom
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int SimpleStmt(Parser *parser) {
    typeId valueType;
    if (Check(parser, TOKEN_VAL) || Check(parser, TOKEN_VAR)) {
        int result = DefVar(parser);
        if (result != 0)
            return result;
        parser->lastKind = STMT_UNIT;

        return result;
    } else if (Check(parser, TOKEN_RETURN)) {
        int result = RetStmt(parser);
        if (result != 0)
            return result;
        parser->lastKind = STMT_RETURN;

        return result;
    } else {
        const char *name = parser->current.value;
        bool startsIdent = Check(parser, TOKEN_IDENT);
        size_t before = parser->tokenCount;
        typeId exprType;
        int result = Expr(parser, &exprType);
        if (result != 0)
            return result;

        if (Check(parser, TOKEN_ASSIGN) && !(startsIdent && parser->tokenCount - before == 1)) {
            return 2; // A3
        }

        bool isAssign = Check(parser, TOKEN_ASSIGN);

        result = AssignAfter(parser, &valueType);
        if (result != 0)
            return result;

        if (isAssign) {
            result = AssignValueToVar(name, valueType);
            if (result != 0)
                return result;
            parser->lastKind = STMT_UNIT;
        } else {
            parser->lastKind = STMT_EXPR;
            parser->lastComType = exprType;
        }
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
        typeId initType;
        bool isConst;
        if (Check(parser, TOKEN_VAL)) {
            isConst = true;
        } else {
            isConst = false;
        }

        int result = Advance(parser);
        if (result != 0)
            return result;

        const char *name = parser->current.value;
        bool hasType;
        typeId type;

        result = Expect(parser, TOKEN_IDENT);
        if (result != 0)
            return result;

        result = VarRest(parser, &hasType, &type, &initType);
        if (result != 0)
            return result;

        result = DeclareVar(name, hasType, type, isConst, initType);
        if (result != 0)
            return result;

        if (parser->genCode) {
            result = GenDefVar(parser->cg, name);
            if (result != 0)
                return result;

            result = GenPopToVar(parser->cg, name);
            if (result != 0)
                return result;
        }

        return result;
    } else {
        return 2;
    }
}

/**
 * @brief Volitelny typ a inicializacia premennej
 * @param parser stav parsera s aktualnym tokenom
 * @param hasType adresa kam sa zapise ci ma premenna uvedeny typ
 * @param type adresa kam sa zapise uvedeny typ
 * @param initType adresa kam sa zapise typ inicializovaneho vyrazu
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int VarRest(Parser *parser, bool *hasType, typeId *type, typeId *initType) {
    if (Check(parser, TOKEN_COLON)) {

        *hasType = true;

        int result = Advance(parser);
        if (result != 0)
            return result;

        result = DataType(parser, type);
        if (result != 0)
            return result;

        result = Expect(parser, TOKEN_ASSIGN);
        if (result != 0)
            return result;

        result = Expr(parser, initType);
        if (result != 0)
            return result;

        return result;
    } else {
        *hasType = false;
        *type = TYPE_UNDEFINED;

        int result = Expect(parser, TOKEN_ASSIGN);
        if (result != 0)
            return result;

        result = Expr(parser, initType);
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
        result = Return(false, TYPE_UNDEFINED);
        if (result != 0)
            return result;
        if (parser->genCode) {
            result = GenReturn(parser->cg, false, parser->isMainFun);
            if (result != 0)
                return result;
        }
        return 0;
    } else {
        typeId exprType;
        result = Expr(parser, &exprType);
        if (result != 0)
            return result;
        result = Return(true, exprType);
        if (result != 0)
            return result;
        if (parser->genCode) {
            result = GenReturn(parser->cg, true, parser->isMainFun);
            if (result != 0)
                return result;
        }

        return result;
    }
}

/**
 * @brief Spracovava volitelne priradenie ("= vyraz" za vyrazom)
 * @param parser stav parsera s aktualnym tokenom
 * @param valueType adresa kam sa zapise typ pravej strany, UNDEFINED ak nie je priradenie
 * @return 0 ak OK, inak kod chyby (1/2/99)
 */
static int AssignAfter(Parser *parser, typeId *valueType) {
    if (Check(parser, TOKEN_ASSIGN)) {
        int result = Advance(parser);
        if (result != 0)
            return result;

        result = Expr(parser, valueType);
        if (result != 0)
            return result;

        return result;
    } else {
        *valueType = TYPE_UNDEFINED;
        return 0;
    }
}

/**
 * @brief skontroluje telo funkcie od FuncBodyStart po FuncBodyEnd
 * @param parser stav parsera s aktualnym tokenom
 * @param name meno funkcie
 * @param isMain ci ide o funkciu main
 * @param hasRet ci ma funkcia uvedeny navratovy typ
 * @param retType uvedeny navratovy typ funkcie
 * @return 0 ak OK, alebo kod chyby (1/2/99)
 */
static int FunBodyCheck(Parser *parser, const char *name, bool isMain, bool hasRet,
                        typeId retType) {
    int result =
        FuncBodyStart(name, parser->paramNames, parser->paramTypes, parser->paramCount, isMain);
    if (result != 0)
        return result;

    if (parser->genCode) {
        parser->isMainFun = isMain;
        result = GenFunStart(parser->cg, name, isMain);
        if (result != 0)
            return result;

        result = GenFuncParams(parser->cg, parser->paramNames, parser->paramCount);
        if (result != 0)
            return result;
    }

    parser->lastKind = STMT_NONE;

    result = FunHasBody(parser);
    if (result != 0)
        return result;

    bool hasResult;
    typeId resultType;
    switch (parser->lastKind) {
    case STMT_EXPR:
        hasResult = true;
        resultType = parser->lastComType;
        break;

    case STMT_UNIT:
        hasResult = true;
        resultType = TYPE_UNIT;
        break;

    case STMT_RETURN:
        hasResult = false;
        resultType = TYPE_UNDEFINED;
        break;

    case STMT_IF:
        if (hasRet && retType == TYPE_UNIT) {
            hasResult = true;
            resultType = TYPE_UNIT;
        } else if (parser->lastIfAllReturn) {
            hasResult = false;
            resultType = TYPE_UNDEFINED;
        } else if (!hasRet && parser->lastIfAllUnit) {
            hasResult = true;
            resultType = TYPE_UNIT;
        } else {
            hasResult = true;
            resultType = TYPE_UNDEFINED;
        }
        break;

    default:
        hasResult = true;
        resultType = TYPE_UNIT;
        break;
    }

    result = FuncBodyEnd(hasResult, resultType);
    if (result != 0)
        return result;

    if (parser->genCode) {
        result = GenFunEnd(parser->cg, isMain, parser->lastKind == STMT_EXPR);
        if (result != 0)
            return result;
    }

    return 0;
}

/**
 * @brief Spracuje definiciu funkcie v tvare : def meno(parametre) [: typ] = telo
 * @param parser stav parsera s aktualnym tokenom
 * @param isMain ci ide o funkciu main
 * @return vysledok spracovania pravidla definicie funkcie
 */
static int Fun(Parser *parser, bool isMain) {
    typeId retType;
    bool hasRet;
    int result = Expect(parser, TOKEN_DEF);
    if (result != 0)
        return result;

    const char *name = parser->current.value;

    result = Expect(parser, TOKEN_IDENT);
    if (result != 0)
        return result;

    result = Params(parser);
    if (result != 0)
        return result;

    result = FunOptions(parser, &hasRet, &retType);
    if (result != 0)
        return result;

    result = Expect(parser, TOKEN_ASSIGN);
    if (result != 0)
        return result;

    if (parser->inferPass && FunctionHasType(name)) {
        result = SkipBody(parser);
        return result;
    }

    size_t bodyStart = parser->position;

    result = FunBodyCheck(parser, name, isMain, hasRet, retType);
    if (result != 0 && parser->inferPass) {
        result = AbortFuncBody();
        if (result != 0) {
            return result;
        }
        parser->position = bodyStart;
        parser->current = parser->fieldOfTokens[bodyStart];
        return SkipBody(parser);
    }

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
    result = Fun(parser, true);
    return result;
}

/**
 * @brief Spracuje zoznam funkcii za prologom ifj26.* a podla aktualneho tokenu vyberie pravidlo
 * @param parser stav parsera s aktualnym tokenom
 * @return vysledok spracovania funkcii za prologom
 */
static int FunList(Parser *parser) {
    if (Check(parser, TOKEN_DEF)) {
        int result = Fun(parser, false);
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

    if (parser->genCode) {
        result = GenProlog(parser->cg);
        if (result != 0) {
            return result;
        }
    }

    result = FunList(parser);
    if (result != 0)
        return result;

    result = CheckMain();
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
    codeGen cg;
    parser.tokenCount = 0;
    parser.lexer = lexer;
    parser.prevLine = 0;
    parser.count = 0;
    parser.capacity = 0;
    parser.position = 0;
    parser.fieldOfTokens = NULL;
    parser.lastComType = TYPE_UNDEFINED;
    parser.lastKind = STMT_NONE;
    parser.lastIfAllReturn = false;
    parser.lastIfAllUnit = false;
    parser.inferPass = false;
    parser.isMainFun = false;
    int result = SemAnalysisInit();
    if (result != 0)
        return result;
    result = CodeGenInit(&cg);
    if (result != 0) {
        SemAnalysisFree();
        return result;
    }
    parser.cg = &cg;
    parser.genCode = false;
    result = FillBuffer(&parser);
    if (result != 0) {
        FreeBuffer(&parser);
        SemAnalysisFree();
        CodeGenFree(&cg);
        return result;
    }
    parser.current = parser.fieldOfTokens[0];
    result = HeaderPass(&parser);
    if (result != 0) {
        FreeBuffer(&parser);
        SemAnalysisFree();
        CodeGenFree(&cg);
        return result;
    }
    int before;
    parser.inferPass = true;
    SemInferMode(true);
    do {
        before = DefinedFunctionCount();
        ResetParserPosition(&parser);
        result = Program(&parser);
        if (result == 99) {
            FreeBuffer(&parser);
            SemAnalysisFree();
            CodeGenFree(&cg);
            return 99;
        }

    } while (DefinedFunctionCount() > before);

    result = ResetSemAnalysisForFinalAnalysis();
    if (result != 0) {
        FreeBuffer(&parser);
        SemAnalysisFree();
        CodeGenFree(&cg);
        return result;
    }

    parser.genCode = true;
    parser.inferPass = false;
    ResetParserPosition(&parser);
    result = Program(&parser);
    if (result == 0) {
        fputs(cg.out.data, stdout);
    }
    FreeBuffer(&parser);
    SemAnalysisFree();
    CodeGenFree(&cg);
    return result;
}
