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

/** Jednoduche cele cislo. */
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
