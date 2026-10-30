/**
 * @file lexer_test.cpp
 * @brief Unit testy lexera pomocou GoogleTest
 *
 * Projekt: Implementace prekladace imperativniho jazyka IFJ26
 * @author Matej Hudec (xhudecm01)
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>
extern "C" {
#include "../../lexer/lexer.h"
}

struct ExpectedToken {
    TokenType type;
    const char *value;
};

/**
 * @brief Spusti lexer na vstupe a porovna vratene tokeny s ocakavanymi.
 * @param input vstupny kod v jazyku IFJ26
 * @param expected zoznam ocakavanych tokenov v poradi
 * @param expected_error ocakavany navratovy kod po poslednom tokene (0 = ziadna chyba)
 */
static void ExpectedTokens(const char *input, std::vector<ExpectedToken> expected,
                           int expected_error = 0) {
    FILE *input_file = fmemopen((void *)input, strlen(input), "r");
    ASSERT_NE(input_file, nullptr);
    Lexer *lexer = LexerCreate(input_file);
    ASSERT_NE(lexer, nullptr);
    Token token;
    for (const ExpectedToken &exp : expected) {
        int result = LexerNextToken(lexer, &token);
        ASSERT_EQ(result, 0);
        EXPECT_EQ(token.type, exp.type);
        if (exp.value != nullptr) {
            EXPECT_STREQ(token.value, exp.value);
        }
        FreeToken(&token);
    }

    if (expected_error != 0) {
        int result = LexerNextToken(lexer, &token);
        EXPECT_EQ(result, expected_error);
    }
    LexerFree(lexer);
    fclose(input_file);
}

/** Prazdny vstup: lexer musi hned vratit TOKEN_EOF. */
TEST(LexerBasic, EmptyInput) {
    const char *input = "";
    FILE *input_file = fmemopen((void *)input, strlen(input), "r");
    ASSERT_NE(input_file, nullptr);
    Lexer *lexer = LexerCreate(input_file);
    ASSERT_NE(lexer, nullptr);
    Token token;
    int result = LexerNextToken(lexer, &token);
    ASSERT_EQ(result, 0);
    EXPECT_EQ(token.type, TOKEN_EOF);
    FreeToken(&token);
    LexerFree(lexer);
    fclose(input_file);
}
// ===== Testy pre cele cisla =====
/** Jednoduche cele cislo */
TEST(LexerInt, Basic) {
    ExpectedTokens("42", {{TOKEN_INT_LITERAL, "42"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Samotna nula je platny celociselny literal */
TEST(LexerInt, Zero) {
    ExpectedTokens("0", {{TOKEN_INT_LITERAL, "0"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Podciarkniky su povolene medzi dvoma cislicami */
TEST(LexerInt, Underscores) {
    ExpectedTokens("1_010_010",
                   {{TOKEN_INT_LITERAL, "1_010_010"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Druhy "_" nieje medzi dvoma cislami */
TEST(LexerInt, TwoUnderscoresAfterEachother) { ExpectedTokens("1__0", {}, 1); }

/** "_" na konci nieje medzi dvoma cislami */
TEST(LexerInt, TrailingUnderscore) { ExpectedTokens("1_", {}, 1); }

/** "_" na konci nieje medzi dvoma cislami */
TEST(LexerInt, TrailingUnderscoreAfterNumbers) { ExpectedTokens("1_011_", {}, 1); }

/** Cislo zacina "_" takze je to identifikator*/
TEST(LexerInt, UnderscoreBeginning) {
    ExpectedTokens("_1", {{TOKEN_IDENT, "_1"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Nenulove cislo nesmie zacinat nulou*/
TEST(LexerInt, LeadingZeros) { ExpectedTokens("009", {}, 1); }

/** Nenulove cislo nesmie zacinat nulou*/
TEST(LexerInt, LeadingZeroBiggerNumber) { ExpectedTokens("01954", {}, 1); }

/** Zaporne Cislo*/
TEST(LexerInt, NegativeNumber) {
    ExpectedTokens("-562", {{TOKEN_MINUS, "-"},
                            {TOKEN_INT_LITERAL, "562"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/** Vzorec s odcitanim*/
TEST(LexerInt, EquationMinus) {
    ExpectedTokens("x-15", {{TOKEN_IDENT, "x"},
                            {TOKEN_MINUS, "-"},
                            {TOKEN_INT_LITERAL, "15"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/** Cislo v beznom prikaze*/
TEST(LexerInt, DeclarationNumber) {
    ExpectedTokens("val x = 12", {{TOKEN_VAL, "val"},
                                  {TOKEN_IDENT, "x"},
                                  {TOKEN_ASSIGN, "="},
                                  {TOKEN_INT_LITERAL, "12"},
                                  {TOKEN_EOL, nullptr},
                                  {TOKEN_EOF, nullptr}});
}

/** Nenulove cislo nesmie zacinat nulou*/
TEST(LexerInt, DeclarationNumberStartingWithZero) {
    ExpectedTokens("val y = 001",
                   {
                       {TOKEN_VAL, "val"},
                       {TOKEN_IDENT, "y"},
                       {TOKEN_ASSIGN, "="},
                   },
                   1);
}

/** "000" je viac nul takze chyba 1 po prvom tokene*/
TEST(LexerInt, TwoNumbersSecondInvalid) { ExpectedTokens("1 000", {{TOKEN_INT_LITERAL, "1"}}, 1); }

/** Rozsah sa nemusi kontrolovat*/
TEST(LexerInt, LargeNumber) {
    ExpectedTokens(
        "54236985214785",
        {{TOKEN_INT_LITERAL, "54236985214785"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

// ===== Testy pre desatinne cisla =====
/**Zakladne desatinne cislo*/
TEST(LexerDouble, BasicDoubleNumber) {
    ExpectedTokens("3.14",
                   {{TOKEN_DOUBLE_LITERAL, "3.14"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Cela cast je nula*/
TEST(LexerDouble, WholeNumberZero) {
    ExpectedTokens("0.17",
                   {{TOKEN_DOUBLE_LITERAL, "0.17"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Cela cast desatinneho cisla moze chybat*/
TEST(LexerDouble, NonWholeNumber) {
    ExpectedTokens(".58963",
                   {{TOKEN_DOUBLE_LITERAL, ".58963"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Druha moznost zapisu : cela cast a exponent bez bodky*/
TEST(LexerDouble, WholeNumberPlusExponentWithoutDot) {
    ExpectedTokens("7e25",
                   {{TOKEN_DOUBLE_LITERAL, "7e25"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Velke E*/
TEST(LexerDouble, UppercaseE) {
    ExpectedTokens("7E25",
                   {{TOKEN_DOUBLE_LITERAL, "7E25"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Exponent so znamienkom "+"*/
TEST(LexerDouble, ExponentWithSign) {
    ExpectedTokens("9e+89",
                   {{TOKEN_DOUBLE_LITERAL, "9e+89"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Bodka, exponent aj znamienko "-"*/
TEST(LexerDouble, DotExponentSign) {
    ExpectedTokens("3.4e-71",
                   {{TOKEN_DOUBLE_LITERAL, "3.4e-71"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Exponent bez celej casti*/
TEST(LexerDouble, ExponentWithoutWholeNumber) {
    ExpectedTokens(".5e10",
                   {{TOKEN_DOUBLE_LITERAL, ".5e10"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Pripona "d"*/
TEST(LexerDouble, SuffixLowerD) {
    ExpectedTokens("9.4d",
                   {{TOKEN_DOUBLE_LITERAL, "9.4d"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Pripona "D"*/
TEST(LexerDouble, SuffixUpperD) {
    ExpectedTokens("9.4D",
                   {{TOKEN_DOUBLE_LITERAL, "9.4D"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Exponent s priponou*/
TEST(LexerDouble, ExponentWithSuffix) {
    ExpectedTokens(
        "9e45555d",
        {{TOKEN_DOUBLE_LITERAL, "9e45555d"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Podciarkniky v oboch castiach*/
TEST(LexerDouble, UnderscoreInBothParts) {
    ExpectedTokens(
        "222_111.333_444",
        {{TOKEN_DOUBLE_LITERAL, "222_111.333_444"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Podciarknik v exponente*/
TEST(LexerDouble, UnderscoreInExponent) {
    ExpectedTokens(
        "222.111e333_444",
        {{TOKEN_DOUBLE_LITERAL, "222.111e333_444"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Za exponent musi ist aspon 1 cislica*/
TEST(LexerDouble, AtLeastOneDigitAfterExponent) { ExpectedTokens("48e", {}, 1); }

/**Za exponent musi ist aspon 1 cislica*/
TEST(LexerDouble, AtLeastOneDigitAfterExponentDouble) { ExpectedTokens("4.8e", {}, 1); }

/**Za znamienko musi ist aspon 1 cislica*/
TEST(LexerDouble, AtLeastOneDigitAfterSign) { ExpectedTokens("48e+", {}, 1); }

/**Podciarknik na konci*/
TEST(LexerDouble, UnderscoreAtTheEnd) { ExpectedTokens("4.5_", {}, 1); }

/**Podciarknik nie je medzi cislicami ale pred bodkou*/
TEST(LexerDouble, UnderscoreBeforeDot) { ExpectedTokens("4_.3", {}, 1); }

/**Za bodkou musi byt cislica (ale toto nieje double)*/
TEST(LexerDouble, NoDigitAfterDot) {
    ExpectedTokens(
        "222.",
        {{TOKEN_INT_LITERAL, "222"}, {TOKEN_DOT, "."}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Za bodkou musi byt cislica (ale toto nieje double)*/
TEST(LexerDouble, NoDigitAfterDotNonDouble) {
    ExpectedTokens("222.z", {{TOKEN_INT_LITERAL, "222"},
                             {TOKEN_DOT, "."},
                             {TOKEN_IDENT, "z"},
                             {TOKEN_EOL, nullptr},
                             {TOKEN_EOF, nullptr}});
}

/**Prve cislo konci pri druhej bodke, .5 je nove cislo*/
TEST(LexerDouble, TwoDoubleNumbers) {
    ExpectedTokens("1.5.5", {{TOKEN_DOUBLE_LITERAL, "1.5"},
                             {TOKEN_DOUBLE_LITERAL, ".5"},
                             {TOKEN_EOL, nullptr},
                             {TOKEN_EOF, nullptr}});
}

/**Negative Number*/
TEST(LexerDouble, NegativeNumber) {
    ExpectedTokens("-1.5", {{TOKEN_MINUS, "-"},
                            {TOKEN_DOUBLE_LITERAL, "1.5"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/**Pouzitie v prikaze*/
TEST(LexerDouble, UseInCommand) {
    ExpectedTokens("val z = 1.152", {{TOKEN_VAL, "val"},
                                     {TOKEN_IDENT, "z"},
                                     {TOKEN_ASSIGN, "="},
                                     {TOKEN_DOUBLE_LITERAL, "1.152"},
                                     {TOKEN_EOL, nullptr},
                                     {TOKEN_EOF, nullptr}});
}

// ===== Testy pre identifikatory a klucove slova =====
/**Najkratsi indentifikator*/
TEST(LexerIdent, ShortestIdentifier) {
    ExpectedTokens("x", {{TOKEN_IDENT, "x"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Pismena aj cislice*/
TEST(LexerIdent, CharsAndNumbers) {
    ExpectedTokens("myVar543",
                   {{TOKEN_IDENT, "myVar543"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Zacina sa "_" */
TEST(LexerIdent, UnderscoreBeginning) {
    ExpectedTokens("_Var541",
                   {{TOKEN_IDENT, "_Var541"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/**Viac "_" na zaciatku */
TEST(LexerIdent, MultipleUnderscoresAtTheBeginning) {
    ExpectedTokens("___z", {{TOKEN_IDENT, "___z"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** "_" na konci */
TEST(LexerIdent, UnderscoreAtTheEnd) {
    ExpectedTokens("z_", {{TOKEN_IDENT, "z_"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Samotne "_" nieje ident */
TEST(LexerIdent, UnderscoreOnly) {
    ExpectedTokens("_", {{TOKEN_WILDCARD, "_"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Test pre vsetky klucove slova naraz */
TEST(LexerIdent, AllKeyWords) {
    ExpectedTokens(
        "def do Double else if import Int return String then Unit val var while Boolean true false",
        {{TOKEN_DEF, "def"},
         {TOKEN_DO, "do"},
         {TOKEN_DOUBLE, "Double"},
         {TOKEN_ELSE, "else"},
         {TOKEN_IF, "if"},
         {TOKEN_IMPORT, "import"},
         {TOKEN_INT, "Int"},
         {TOKEN_RETURN, "return"},
         {TOKEN_STRING, "String"},
         {TOKEN_THEN, "then"},
         {TOKEN_UNIT, "Unit"},
         {TOKEN_VAL, "val"},
         {TOKEN_VAR, "var"},
         {TOKEN_WHILE, "while"},
         {TOKEN_BOOL, "Boolean"},
         {TOKEN_TRUE_LITERAL, "true"},
         {TOKEN_FALSE_LITERAL, "false"},
         {TOKEN_EOL, nullptr},
         {TOKEN_EOF, nullptr}});
}

/** Velke/male pismena (Val != val) */
TEST(LexerIdent, UpperCaseVal) {
    ExpectedTokens("Val", {{TOKEN_IDENT, "Val"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Velke/male pismena (Int != int) */
TEST(LexerIdent, LowerCaseInt) {
    ExpectedTokens("int", {{TOKEN_IDENT, "int"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Velke/male pismena (Double != double) */
TEST(LexerIdent, LowerCaseDouble) {
    ExpectedTokens("double", {{TOKEN_IDENT, "double"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Zacina na def ale je to ine slovo */
TEST(LexerIdent, Define) {
    ExpectedTokens("define", {{TOKEN_IDENT, "define"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** iff nie je klucove slovo if */
TEST(LexerIdent, Iff) {
    ExpectedTokens("iff", {{TOKEN_IDENT, "iff"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Anotacia hlavnej funkcie */
TEST(LexerIdent, MainFunctionAnnotation) {
    ExpectedTokens("@main", {{TOKEN_MAIN, "@main"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Anotacia hlavnej funkcie na rovnakom riadku ako def */
TEST(LexerIdent, MainFunctionAnnotationSameLineAsDef) {
    ExpectedTokens(
        "@main def",
        {{TOKEN_MAIN, "@main"}, {TOKEN_DEF, "def"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Medzi @ a main nesmie byt ziadny znak */
TEST(LexerIdent, NoCharBetween) { ExpectedTokens("@ main", {}, 1); }

/** Case sensitive */
TEST(LexerIdent, CaseSensitive) { ExpectedTokens("@Main", {}, 1); }

/** Ine slovo ako main */
TEST(LexerIdent, DifferentWord) { ExpectedTokens("@mainz", {}, 1); }

/** @ sa inde nepouziva */
TEST(LexerIdent, AtOnlyUsedInMain) { ExpectedTokens("@", {}, 1); }

/** Neznamy znak*/
TEST(LexerIdent, UnknownCharDollar) { ExpectedTokens("$x", {}, 1); }

/** Neznamy znak*/
TEST(LexerIdent, UnknownCharHashtag) { ExpectedTokens("#", {}, 1); }

// ===== Testy pre operatory a oddelovace =====
/** Aritmeticke operatory*/
TEST(LexerOperators, ArithmeticOperators) {
    ExpectedTokens("+ - * /", {{TOKEN_PLUS, "+"},
                               {TOKEN_MINUS, "-"},
                               {TOKEN_MUL, "*"},
                               {TOKEN_DIV, "/"},
                               {TOKEN_EOL, nullptr},
                               {TOKEN_EOF, nullptr}});
}

/** Relacne operatory*/
TEST(LexerOperators, RelationalOperators) {
    ExpectedTokens("== != < <= > >=", {{TOKEN_EQ, "=="},
                                       {TOKEN_NEQ, "!="},
                                       {TOKEN_LT, "<"},
                                       {TOKEN_LTE, "<="},
                                       {TOKEN_GT, ">"},
                                       {TOKEN_GTE, ">="},
                                       {TOKEN_EOL, nullptr},
                                       {TOKEN_EOF, nullptr}});
}

/** Logicke (BOOLTHEN)*/
TEST(LexerOperators, LogicalOperators) {
    ExpectedTokens("! && ||", {{TOKEN_NOT, "!"},
                               {TOKEN_AND, "&&"},
                               {TOKEN_OR, "||"},
                               {TOKEN_EOL, nullptr},
                               {TOKEN_EOF, nullptr}});
}

/** Priradenie a sipka (LAMBDAS)*/
TEST(LexerOperators, Lambdas) {
    ExpectedTokens(
        "= =>",
        {{TOKEN_ASSIGN, "="}, {TOKEN_ARROW, "=>"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Oddelovace */
TEST(LexerOperators, Separators) {
    ExpectedTokens(": , . ;", {{TOKEN_COLON, ":"},
                               {TOKEN_COMMA, ","},
                               {TOKEN_DOT, "."},
                               {TOKEN_SEMICOLON, ";"},
                               {TOKEN_EOL, nullptr},
                               {TOKEN_EOF, nullptr}});
}

/** Zatvorky */
TEST(LexerOperators, Brackets) {
    ExpectedTokens("( ) [ ]", {{TOKEN_LPAREN, "("},
                               {TOKEN_RPAREN, ")"},
                               {TOKEN_LBRACKET, "["},
                               {TOKEN_RBRACKET, "]"},
                               {TOKEN_EOL, nullptr},
                               {TOKEN_EOF, nullptr}});
}

/**Bez medzier*/
TEST(LexerOperators, NoSpace) {
    ExpectedTokens("a<=b", {{TOKEN_IDENT, "a"},
                            {TOKEN_LTE, "<="},
                            {TOKEN_IDENT, "b"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/** < bez = za nim */
TEST(LexerOperators, LtWithoutEqual) {
    ExpectedTokens("a<b", {{TOKEN_IDENT, "a"},
                           {TOKEN_LT, "<"},
                           {TOKEN_IDENT, "b"},
                           {TOKEN_EOL, nullptr},
                           {TOKEN_EOF, nullptr}});
}

/** Porovnanie */
TEST(LexerOperators, Compare) {
    ExpectedTokens("a==b", {{TOKEN_IDENT, "a"},
                            {TOKEN_EQ, "=="},
                            {TOKEN_IDENT, "b"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/** Priradenie */
TEST(LexerOperators, Assignment) {
    ExpectedTokens("a=b", {{TOKEN_IDENT, "a"},
                           {TOKEN_ASSIGN, "="},
                           {TOKEN_IDENT, "b"},
                           {TOKEN_EOL, nullptr},
                           {TOKEN_EOF, nullptr}});
}

/** Nerovna sa */
TEST(LexerOperators, NotEqual) {
    ExpectedTokens("a!=b", {{TOKEN_IDENT, "a"},
                            {TOKEN_NEQ, "!="},
                            {TOKEN_IDENT, "b"},
                            {TOKEN_EOL, nullptr},
                            {TOKEN_EOF, nullptr}});
}

/** ! bez = za ním */
TEST(LexerOperators, NotWithoutAssign) {
    ExpectedTokens(
        "!a", {{TOKEN_NOT, "!"}, {TOKEN_IDENT, "a"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Maximal munch: najprv <= potom =*/
TEST(LexerOperators, MaximalMunch) {
    ExpectedTokens(
        "<==",
        {{TOKEN_LTE, "<="}, {TOKEN_ASSIGN, "="}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Najprv == potom = */
TEST(LexerOperators, EqAssign) {
    ExpectedTokens(
        "===", {{TOKEN_EQ, "=="}, {TOKEN_ASSIGN, "="}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Najprv != potom = */
TEST(LexerOperators, NeqAssign) {
    ExpectedTokens(
        "!==",
        {{TOKEN_NEQ, "!="}, {TOKEN_ASSIGN, "="}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** =< nie je operator takze dva tokeny */
TEST(LexerOperators, NonOperator) {
    ExpectedTokens(
        "=<", {{TOKEN_ASSIGN, "="}, {TOKEN_LT, "<"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** -> nie je takze dva samostatne znaky */
TEST(LexerOperators, Arrow) {
    ExpectedTokens(
        "->", {{TOKEN_MINUS, "-"}, {TOKEN_GT, ">"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** <- nie je takze dva samostatne znaky */
TEST(LexerOperators, ReverseArrow) {
    ExpectedTokens(
        "<-", {{TOKEN_LT, "<"}, {TOKEN_MINUS, "-"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Samotne & neexistuje len &&*/
TEST(LexerOperators, SingularAnd) { ExpectedTokens("&", {}, 1); }

/** Samotne | neexistuje */
TEST(LexerOperators, SingularOr) { ExpectedTokens("|", {}, 1); }

/** Chyba az po platnom tokene */
TEST(LexerOperators, ErrAfterToken) { ExpectedTokens("a&b", {{TOKEN_IDENT, "a"}}, 1); }

/** Ziadne zlozene zatvorky*/
TEST(LexerOperators, CurlyBracketOpen) { ExpectedTokens("{", {}, 1); }

/** Ziadne zlozene zatvorky*/
TEST(LexerOperators, CurlyBracketClose) { ExpectedTokens("}", {}, 1); }

/** Modulo neexistuje*/
TEST(LexerOperators, Modulo) { ExpectedTokens("%", {}, 1); }

// ===== Testy retazce =====
/** Zakladny retazec */
TEST(LexerStrings, BasicString) {
    ExpectedTokens(R"("ahoj")",
                   {{TOKEN_STRING_LITERAL, "ahoj"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Prazdny retazec */
TEST(LexerStrings, EmptyString) {
    ExpectedTokens(R"("")",
                   {{TOKEN_STRING_LITERAL, ""}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Medzery sa zachovavaju */
TEST(LexerStrings, Spaces) {
    ExpectedTokens(R"("a b c")",
                   {{TOKEN_STRING_LITERAL, "a b c"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Platna escape sekvencia */
TEST(LexerStrings, ValidEscapeSequence) {
    ExpectedTokens(R"("\\")",
                   {{TOKEN_STRING_LITERAL, nullptr}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** \" retazec neukoncí*/
TEST(LexerStrings, WontEndString) {
    ExpectedTokens(R"("\"")",
                   {{TOKEN_STRING_LITERAL, nullptr}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Dalsie platne escape */
TEST(LexerStrings, AnotherValidEscape) {
    ExpectedTokens(R"("\t\r")",
                   {{TOKEN_STRING_LITERAL, nullptr}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Neplatna escape sekvencia */
TEST(LexerStrings, NonValidEscapeSequence) { ExpectedTokens(R"("\q")", {}, 1); }

/** Ina neplatna escape sekvencia */
TEST(LexerStrings, DifferentNonValidExcapeSequence) { ExpectedTokens(R"("\x41")", {}, 1); }

/** Neukonceny retazec */
TEST(LexerStrings, NonEndingString) { ExpectedTokens(R"("ahoj)", {}, 1); }

/** Zamerne nie raw string */
TEST(LexerStrings, NonRawString) { ExpectedTokens("\"a\nb\"", {}, 1); }

/** Retazec zostane neukonceny */
TEST(LexerStrings, NonEndingEscapeString) { ExpectedTokens(R"("ab\")", {}, 1); }

/** Viacriadkovy */
TEST(LexerStrings, MultiLine) {
    ExpectedTokens(R"("""abc""")",
                   {{TOKEN_STRING_LITERAL, "abc"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Prazdny viacriadkovy */
TEST(LexerStrings, EmptyMultiLine) {
    ExpectedTokens(R"("""""")",
                   {{TOKEN_STRING_LITERAL, ""}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Skutocny novy riadok */
TEST(LexerStrings, RealNewLine) {
    ExpectedTokens("\"\"\"a\nb\"\"\"",
                   {{TOKEN_STRING_LITERAL, "a\nb"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** jedna " vo vnutri nekonci */
TEST(LexerStrings, OneQuotationMark) {
    ExpectedTokens(R"("""a"b""")",
                   {{TOKEN_STRING_LITERAL, R"(a"b)"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** dve " vo vnutri nekonci */
TEST(LexerStrings, TwoQuotationMark) {
    ExpectedTokens(R"("""a""b""")",
                   {{TOKEN_STRING_LITERAL, R"(a""b)"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Neukonecny viacriadkovy */
TEST(LexerStrings, NonEndingMultiLine) { ExpectedTokens(R"("""abc)", {}, 1); }

/** Spajanie retazcov */
TEST(LexerStrings, StringConcatenation) {
    ExpectedTokens(R"("a"+"b")", {{TOKEN_STRING_LITERAL, "a"},
                                  {TOKEN_PLUS, "+"},
                                  {TOKEN_STRING_LITERAL, "b"},
                                  {TOKEN_EOL, nullptr},
                                  {TOKEN_EOF, nullptr}});
}

/** Znaky ktore su mimo retazca chybou su vo vnutri v poriadku */
TEST(LexerStrings, InvalidCharsInside) {
    ExpectedTokens(
        R"("#$%@{}")",
        {{TOKEN_STRING_LITERAL, R"(#$%@{})"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** // v retazci nie je komentar */
TEST(LexerStrings, NotAComment) {
    ExpectedTokens(R"("//x")",
                   {{TOKEN_STRING_LITERAL, "//x"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Blokovy komentar v retazci nie je komentar */
TEST(LexerStrings, AlsoNotAComment) {
    ExpectedTokens(R"("/*x*/")",
                   {{TOKEN_STRING_LITERAL, "/*x*/"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Retazec v prikaze */
TEST(LexerStrings, StringInCommand) {
    ExpectedTokens(R"(val s = "hi")", {{TOKEN_VAL, "val"},
                                       {TOKEN_IDENT, "s"},
                                       {TOKEN_ASSIGN, "="},
                                       {TOKEN_STRING_LITERAL, "hi"},
                                       {TOKEN_EOL, nullptr},
                                       {TOKEN_EOF, nullptr}});
}

// ===== Testy pre komentare =====
/** Riadkovy komentar za kodom */
TEST(LexerComments, LineCommentAfterCode) {
    ExpectedTokens("x // komentar",
                   {{TOKEN_IDENT, "x"}, {TOKEN_EOL, nullptr}, {TOKEN_EOF, nullptr}});
}

/** Riadok na ktorom je len komentar sa ignoruje */
TEST(LexerComments, CommentOnlyLine) { ExpectedTokens("// len komentar", {{TOKEN_EOF, nullptr}}); }

/** Komentar konci na konci riadku */
TEST(LexerComments, EndOfLineComment) {
    ExpectedTokens("x // k\ny", {
                                    {TOKEN_IDENT, "x"},
                                    {TOKEN_EOL, nullptr},
                                    {TOKEN_IDENT, "y"},
                                    {TOKEN_EOL, nullptr},
                                    {TOKEN_EOF, nullptr},
                                });
}

/** Blokovy komentar bez noveho riadku = medzera */
TEST(LexerComments, BlockCommentNoNewLine) {
    ExpectedTokens("x /* k */ y", {
                                      {TOKEN_IDENT, "x"},
                                      {TOKEN_IDENT, "y"},
                                      {TOKEN_EOL, nullptr},
                                      {TOKEN_EOF, nullptr},
                                  });
}

/** Prazdny komentar bez medzier oddeluje tokeny */
TEST(LexerComments, EmptyCommentNoSpace) {
    ExpectedTokens("x/**/y", {
                                 {TOKEN_IDENT, "x"},
                                 {TOKEN_IDENT, "y"},
                                 {TOKEN_EOL, nullptr},
                                 {TOKEN_EOF, nullptr},
                             });
}

/** Len Komentar */
TEST(LexerComments, CommentOnly) {
    ExpectedTokens("/* k */", {
                                  {TOKEN_EOF, nullptr},
                              });
}

/** Vnoreny komentar */
TEST(LexerComments, NestedComment) {
    ExpectedTokens("x /* a /* b */ c */ y", {
                                                {TOKEN_IDENT, "x"},
                                                {TOKEN_IDENT, "y"},
                                                {TOKEN_EOL, nullptr},
                                                {TOKEN_EOF, nullptr},
                                            });
}

/** // vo vnutri blokoveho komentara neznamena nic */
TEST(LexerComments, SlashInComment) {
    ExpectedTokens("x /* // */ y", {
                                       {TOKEN_IDENT, "x"},
                                       {TOKEN_IDENT, "y"},
                                       {TOKEN_EOL, nullptr},
                                       {TOKEN_EOF, nullptr},
                                   });
}

/** Zaciatok blokoveho komentara v riadkovom komentari nic neznamena */
TEST(LexerComments, BlockStartInLineComment) {
    ExpectedTokens("x // a /* b", {
                                      {TOKEN_IDENT, "x"},
                                      {TOKEN_EOL, nullptr},
                                      {TOKEN_EOF, nullptr},
                                  });
}

/** * / s medzerou nie je koniec komentara */
TEST(LexerComments, SpacedStarSlash) {
    ExpectedTokens("/* * / */", {
                                    {TOKEN_EOF, nullptr},
                                });
}

/** 3 rozne tokeny  */
TEST(LexerComments, ThreeStars) {
    ExpectedTokens("/***/", {
                                {TOKEN_EOF, nullptr},
                            });
}

/** Komentar s novym riadkom je koniec riadku  */
TEST(LexerComments, MultiLineBlockComment) {
    ExpectedTokens("x /* a\nb */", {
                                       {TOKEN_IDENT, "x"},
                                       {TOKEN_EOL, nullptr},
                                       {TOKEN_EOF, nullptr},
                                   });
}

/** Po viacriadkovom komentari pokracuje kod na dalsom riadku  */
TEST(LexerComments, CodeAfterMultiLineComment) {
    ExpectedTokens("/* a\nb */\nx", {
                                        {TOKEN_IDENT, "x"},
                                        {TOKEN_EOL, nullptr},
                                        {TOKEN_EOF, nullptr},
                                    });
}

/** Za komentarom s novym riadkom nesmie byt token na tom istom riadku */
TEST(LexerComments, TokenAfterMultiLineComment) {
    ExpectedTokens("x /* a\nb */ y",
                   {
                       {TOKEN_IDENT, "x"},
                   },
                   1);
}

/** Neukonceny komentar */
TEST(LexerComments, UnterminatedBlock) { ExpectedTokens("/* neukonceny", {}, 1); }

/** Vnoreny a neukonceny */
TEST(LexerComments, UnterminatedNested) { ExpectedTokens("/* a /* b */", {}, 1); }

/** jedno / je delenie nie komentar */
TEST(LexerComments, SlashIsDivision) {
    ExpectedTokens("a/b", {
                              {TOKEN_IDENT, "a"},
                              {TOKEN_DIV, "/"},
                              {TOKEN_IDENT, "b"},
                              {TOKEN_EOL, nullptr},
                              {TOKEN_EOF, nullptr},
                          });
}

/** Hviezdicka a lomitko bez zaciatku nie su komentar ale dva operatory */
TEST(LexerComments, CloseWithoutOpen) {
    ExpectedTokens("x */ c", {
                                 {TOKEN_IDENT, "x"},
                                 {TOKEN_MUL, "*"},
                                 {TOKEN_DIV, "/"},
                                 {TOKEN_IDENT, "c"},
                                 {TOKEN_EOL, nullptr},
                                 {TOKEN_EOF, nullptr},
                             });
}
