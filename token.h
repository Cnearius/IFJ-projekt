/**
@file token.h
@author Michal Caban
@brief Tento subor obsahuje struktury pre token
*/

#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

// Definicia vsetkych typov tokenov ktore mozu existovat
typedef enum {
    // Klucove slova
    TOKEN_DEF,    // def
    TOKEN_DO,     // do
    TOKEN_DOUBLE, // Double
    TOKEN_ELSE,   // else
    TOKEN_IF,     // if
    TOKEN_IMPORT, // import
    TOKEN_INT,    // Int
    TOKEN_RETURN, // return
    TOKEN_STRING, // String
    TOKEN_THEN,   // then
    TOKEN_UNIT,   // Unit
    TOKEN_VAL,    // val
    TOKEN_VAR,    // var
    TOKEN_WHILE,  // while

    // Klucove slova v rozsireniach
    TOKEN_BOOL, // Boolean (BOOLTHEN)

    // Specialne symboly a identifikatory
    TOKEN_MAIN,  // @main
    TOKEN_IDENT, // [a-zA-Z_][a-zA-Z0-9_]*

    // Literaly
    TOKEN_INT_LITERAL,    // Int
    TOKEN_DOUBLE_LITERAL, // Double
    TOKEN_STRING_LITERAL, // String
    TOKEN_INTERPOLATED,   // Interpolovaný String s"..."
    TOKEN_TRUE_LITERAL,   // true
    TOKEN_FALSE_LITERAL,  // false
    TOKEN_UNIT_LITERAL,   // ()

    // Aritmeticke operatory
    TOKEN_PLUS,  // +
    TOKEN_MINUS, // -
    TOKEN_MUL,   // *
    TOKEN_DIV,   // /

    // Relacne operatory
    TOKEN_EQ,  // ==
    TOKEN_NEQ, // !=
    TOKEN_LT,  // <
    TOKEN_LTE, // <=
    TOKEN_GT,  // >
    TOKEN_GTE, // >=

    // Logicke operatory
    TOKEN_NOT, // !
    TOKEN_AND, // &&
    TOKEN_OR,  // ||

    // Specialne operatory pre rozsirenia
    TOKEN_ARROW, // => (LAMBDAS)

    // Separatory a zatvorky
    TOKEN_ASSIGN,    // =
    TOKEN_COLON,     // :
    TOKEN_COMMA,     // ,
    TOKEN_DOT,       // .
    TOKEN_SEMICOLON, // ;
    TOKEN_WILDCARD,  // _
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]

    // Riadenie toku, riadkovanie a riadenie bloku
    TOKEN_EOL,    // End of Line (\n)
    TOKEN_INDENT, // Odsadenie bloku
    TOKEN_DEDENT, // Zmensenie odsadenia bloku
    TOKEN_EOF     // End of File
} TokenType;

// Struktura pre samotny token
typedef struct {
    TokenType type;
    char *value;
    size_t line;
    size_t column;
} Token;

#endif
