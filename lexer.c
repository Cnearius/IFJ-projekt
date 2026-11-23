/**
@file lexer.c
@author Oleksandr Savchenko
@brief Implementace lexeru
*/

#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Lexer {
    FILE *input;
    int CurrentCharacter;
    size_t Line;
    size_t Column;
    size_t *IndentStack;
    size_t IndentCount;
    size_t IndentCapacity;
    size_t PendingDegents;
    size_t Depth;
    size_t LogIndent;
    int LineStart;
    int HasCurrentCharacter;
    int HasToken;
};

Lexer *LexerCreate(FILE *input) {
    if (input == NULL) {
        return NULL;
    }

    Lexer *lexer = malloc(sizeof(Lexer));
    if (lexer == NULL) {
        return NULL;
    }

    lexer->input = input;
    lexer->CurrentCharacter = EOF;
    lexer->Line = 1;
    lexer->Column = 0;
    lexer->IndentStack = malloc(8 * sizeof(size_t));
    lexer->IndentCount = 1;
    lexer->IndentCapacity = 8;
    lexer->PendingDegents = 0;
    lexer->Depth = 0;
    lexer->LogIndent = 0;
    lexer->LineStart = 1;
    lexer->HasCurrentCharacter = 0;
    lexer->HasToken = 0;

    if (lexer->IndentStack == NULL) {
        free(lexer);
        return NULL;
    }

    lexer->IndentStack[0] = 0;

    return lexer;
}

void LexerFree(Lexer *lexer) {
    if (lexer == NULL) {
        return;
    }
    free(lexer->IndentStack);
    free(lexer);
}

int LexerPreviewCharacter(Lexer *lexer) {
    if (lexer->HasCurrentCharacter == 0) {
        int character = fgetc(lexer->input);
        if (character == '\r') {
            int next = fgetc(lexer->input);
            if (next != '\n' && next != EOF) {
                if (ungetc(next, lexer->input) == EOF) {
                    return EOF;
                }
            }
            character = '\n';
        }
        lexer->CurrentCharacter = character;
        lexer->HasCurrentCharacter = 1;
    }

    return lexer->CurrentCharacter;
}

int LexerNextCharacter(Lexer *lexer) {
    int character = LexerPreviewCharacter(lexer);

    lexer->HasCurrentCharacter = 0;

    if (character == EOF) {
        lexer->CurrentCharacter = EOF;
        return EOF;
    }

    if (character == '\n') {
        lexer->Line++;
        lexer->Column = 0;
    } else {
        lexer->Column++;
    }
    return character;
}

int SetToken(Token *token, TokenType type, char *value, size_t line, size_t column) {
    if (token == NULL || value == NULL) {
        return 99;
    }

    size_t length = strlen(value);
    token->value = malloc(length + 1);
    if (token->value == NULL) {
        return 99;
    }

    memcpy(token->value, value, length + 1);
    token->type = type;
    token->line = line;
    token->column = column;

    return 0;
}

void FreeToken(Token *token) {
    if (token == NULL) {
        return;
    }

    free(token->value);
    token->value = NULL;
}

int PushIndent(Lexer *lexer, size_t indent) {
    if (lexer->IndentCount == lexer->IndentCapacity) {
        if (lexer->IndentCapacity > ((size_t)-1) / sizeof(size_t) / 2) {
            return 99;
        }
        size_t NewCapacity = lexer->IndentCapacity * 2;
        size_t *NewStack = realloc(lexer->IndentStack, NewCapacity * sizeof(size_t));
        if (NewStack == NULL) {
            return 99;
        }
        lexer->IndentStack = NewStack;
        lexer->IndentCapacity = NewCapacity;
    }
    lexer->IndentStack[lexer->IndentCount] = indent;
    lexer->IndentCount++;

    return 0;
}

int CheckIndent(Lexer *lexer, size_t indent, Token *toke) {
    size_t current = lexer->IndentStack[lexer->IndentCount - 1];
    if (indent == current) {
        return 0;
    }
    if (indent > current) {
        int result = SetToken(toke, TOKEN_INDENT, "", lexer->Line, 1);
        if (result != 0) {
            return result;
        }
        result = PushIndent(lexer, indent);
        if (result != 0) {
            FreeToken(toke);
            return result;
        }
        return 0;
    }
    size_t count = lexer->IndentCount;
    while (count > 1 && lexer->IndentStack[count - 1] > indent) {
        count--;
    }
    if (lexer->IndentStack[count - 1] != indent) {
        return 1;
    }
    lexer->PendingDegents += lexer->IndentCount - count;
    lexer->IndentCount = count;
    return 0;
}

int IdentifierStart(int character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           character == '_';
}
int IdentifierCharacter(int character) {
    return IdentifierStart(character) || (character >= '0' && character <= '9');
}

int AddChar(char **buffer, size_t *length, size_t *capacity, int character) {
    if (*length + 1 >= *capacity) {
        if (*capacity > ((size_t)-1) / 2) {
            return 99;
        }

        size_t NewCapacity = *capacity * 2;
        char *NewBuffer = realloc(*buffer, NewCapacity);
        if (NewBuffer == NULL) {
            return 99;
        }

        *buffer = NewBuffer;
        *capacity = NewCapacity;
    }

    (*buffer)[*length] = (char)character;
    (*length)++;
    (*buffer)[*length] = '\0';

    return 0;
}

int ReadIdentifier(Lexer *lexer, char **value) {
    size_t capacity = 16;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return 99;
    }

    int character = LexerPreviewCharacter(lexer);
    while (IdentifierCharacter(character)) {
        int result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
        if (result != 0) {
            free(buffer);
            return result;
        }
        character = LexerPreviewCharacter(lexer);
    }

    *value = buffer;
    return 0;
}

TokenType IdentifierType(char *value) {
    if (strcmp(value, "def") == 0) {
        return TOKEN_DEF;
    }
    if (strcmp(value, "do") == 0) {
        return TOKEN_DO;
    }
    if (strcmp(value, "Double") == 0) {
        return TOKEN_DOUBLE;
    }
    if (strcmp(value, "else") == 0) {
        return TOKEN_ELSE;
    }
    if (strcmp(value, "if") == 0) {
        return TOKEN_IF;
    }
    if (strcmp(value, "import") == 0) {
        return TOKEN_IMPORT;
    }
    if (strcmp(value, "Int") == 0) {
        return TOKEN_INT;
    }
    if (strcmp(value, "return") == 0) {
        return TOKEN_RETURN;
    }
    if (strcmp(value, "String") == 0) {
        return TOKEN_STRING;
    }
    if (strcmp(value, "then") == 0) {
        return TOKEN_THEN;
    }
    if (strcmp(value, "Unit") == 0) {
        return TOKEN_UNIT;
    }
    if (strcmp(value, "val") == 0) {
        return TOKEN_VAL;
    }
    if (strcmp(value, "var") == 0) {
        return TOKEN_VAR;
    }
    if (strcmp(value, "while") == 0) {
        return TOKEN_WHILE;
    }
    if (strcmp(value, "Boolean") == 0) {
        return TOKEN_BOOL;
    }
    if (strcmp(value, "true") == 0) {
        return TOKEN_TRUE_LITERAL;
    }
    if (strcmp(value, "false") == 0) {
        return TOKEN_FALSE_LITERAL;
    }
    return TOKEN_IDENT;
}

int ReadDigits(Lexer *lexer, char **buffer, size_t *length, size_t *capacity) {
    int character = LexerPreviewCharacter(lexer);
    if (!(character >= '0' && character <= '9')) {
        return 1;
    }

    while (1) {
        character = LexerPreviewCharacter(lexer);
        if (character >= '0' && character <= '9') {
            int result = AddChar(buffer, length, capacity, LexerNextCharacter(lexer));
            if (result != 0) {
                return result;
            }
        } else if (character == '_') {
            LexerNextCharacter(lexer);
            character = LexerPreviewCharacter(lexer);
            if (!(character >= '0' && character <= '9')) {
                return 1;
            }
            int result = AddChar(buffer, length, capacity, '_');
            if (result != 0) {
                return result;
            }
        } else {
            break;
        }
    }

    return 0;
}

int ReadNumber(Lexer *lexer, Token *token, size_t line, size_t column) {
    size_t capacity = 16;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return 99;
    }

    int result = 0;
    int Double = 0;
    int character = LexerPreviewCharacter(lexer);
    if (character != '.') {
        result = ReadDigits(lexer, &buffer, &length, &capacity);
        if (result != 0) {
            free(buffer);
            return result;
        }
    }

    character = LexerPreviewCharacter(lexer);
    if (character == '.') {
        int next = fgetc(lexer->input);
        if (next != EOF) {
            if (ungetc(next, lexer->input) == EOF) {
                free(buffer);
                return 99;
            }
        } else if (ferror(lexer->input)) {
            free(buffer);
            return 99;
        }
        if (next >= '0' && next <= '9') {
            Double = 1;
            result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
            if (result != 0) {
                free(buffer);
                return result;
            }
            result = ReadDigits(lexer, &buffer, &length, &capacity);
            if (result != 0) {
                free(buffer);
                return result;
            }
        }
    }
    character = LexerPreviewCharacter(lexer);
    if (character == 'e' || character == 'E') {
        Double = 1;
        result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
        if (result != 0) {
            free(buffer);
            return result;
        }

        character = LexerPreviewCharacter(lexer);
        if (character == '+' || character == '-') {
            result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
            if (result != 0) {
                free(buffer);
                return result;
            }
        }

        result = ReadDigits(lexer, &buffer, &length, &capacity);
        if (result != 0) {
            free(buffer);
            return result;
        }
    }
    character = LexerPreviewCharacter(lexer);
    if (Double && (character == 'd' || character == 'D')) {
        result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
        if (result != 0) {
            free(buffer);
            return result;
        }
    }
    TokenType type;
    if (Double) {
        type = TOKEN_DOUBLE_LITERAL;
    } else {
        if (buffer[0] == '0' && length > 1) {
            free(buffer);
            return 1;
        }
        type = TOKEN_INT_LITERAL;
    }

    result = SetToken(token, type, buffer, line, column);
    free(buffer);
    return result;
}

int ReadString(Lexer *lexer, Token *token, TokenType type, size_t line, size_t column) {
    size_t capacity = 16;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        return 99;
    }
    buffer[0] = '\0';
    int result = 0;
    int count = 0;
    LexerNextCharacter(lexer);
    if (LexerPreviewCharacter(lexer) == '"') {
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '"') {
            LexerNextCharacter(lexer);
            count = 1;
        } else {
            result = SetToken(token, type, buffer, line, column);
            free(buffer);
            return result;
        }
    }

    if (!count) {
        while (1) {
            int character = LexerPreviewCharacter(lexer);
            if (character == EOF) {
                if (ferror(lexer->input)) {
                    result = 99;
                } else {
                    result = 1;
                }
                break;
            }

            if (character == '\n') {
                result = 1;
                break;
            }

            if (character == '"') {
                LexerNextCharacter(lexer);
                result = SetToken(token, type, buffer, line, column);
                break;
            }

            if (character == '\\'){
                LexerNextCharacter(lexer);
                character = LexerPreviewCharacter(lexer);
                switch (character){
                    case 'n':
                        character = '\n';
                        break;
                    case 'r':
                        character = '\r';
                        break;
                    case 't':
                        character = '\t';
                        break;
                    case '"':
                        character = '"';
                        break;
                    case '\\':
                        character = '\\';
                        break;
                    default:
                        result = 1;
                        break;
                }
                if (result != 0){
                    break;
                }
                LexerNextCharacter(lexer);
            } else {
                character = LexerNextCharacter(lexer);
            }
            result = AddChar(&buffer, &length, &capacity, character);
            if (result != 0){
                break;
            }
        }
    } else {
        int quotes = 0;
        while (1) {
            int character = LexerPreviewCharacter(lexer);
            if (character == EOF) {
                if (ferror(lexer->input)) {
                    result = 99;
                } else {
                    result = 1;
                }
                break;
            }

            if (character == '"') {
                LexerNextCharacter(lexer);
                quotes++;
                if (quotes == 3) {
                    while (LexerPreviewCharacter(lexer) == '"') {
                        result = AddChar(&buffer, &length, &capacity, '"');
                        if (result != 0) {
                            break;
                        }
                        LexerNextCharacter(lexer);
                    }
                    if (result == 0) {
                        result = SetToken(token, type, buffer, line, column);
                    }
                    break;
                }
                continue;
            }

            while (quotes > 0) {
                result = AddChar(&buffer, &length, &capacity, '"');
                if (result != 0) {
                    break;
                }
                quotes--;
            }

            if (result != 0) {
                break;
            }

            result = AddChar(&buffer, &length, &capacity, LexerNextCharacter(lexer));
            if (result != 0) {
                break;
            }
        }
    }

    free(buffer);
    return result;
}

void SkipLineComment(Lexer *lexer) {
    while (1) {
        int character = LexerPreviewCharacter(lexer);
        if (character == '\n' || character == EOF) {
            break;
        }
        LexerNextCharacter(lexer);
    }
}

int SkipBlockComment(Lexer *lexer, int *NewLine) {
    size_t depth = 1;
    *NewLine = 0;
    while (depth > 0) {
        int character = LexerNextCharacter(lexer);
        if (character == EOF) {
            if (ferror(lexer->input)) {
                return 99;
            }
            return 1;
        }
        if (character == '\n') {
            *NewLine = 1;
        }
        if (character == '/' && LexerPreviewCharacter(lexer) == '*') {
            LexerNextCharacter(lexer);
            if (depth == (size_t)-1) {
                return 99;
            }
            depth++;
        } else if (character == '*' && LexerPreviewCharacter(lexer) == '/') {
            LexerNextCharacter(lexer);
            depth--;
        }
    }

    return 0;
}

int LexerNextToken(Lexer *lexer, Token *token) {
    if (lexer == NULL || token == NULL) {
        return 99;
    }
    int character;
    while (1) {
        if (lexer->PendingDegents > 0) {
            lexer->PendingDegents--;
            return SetToken(token, TOKEN_DEDENT, "", lexer->Line, lexer->Column + 1);
        }
        if (lexer->LineStart && lexer->Depth == 0) {
            size_t indent = 0;
            int RestartLine = 0;
            int comment = 0;
            int IndentTab = 0;
            while (1) {
                character = LexerPreviewCharacter(lexer);
                if (character == ' ') {
                    LexerNextCharacter(lexer);
                    if (comment == 0) {
                        indent++;
                    }
                    continue;
                }
                if (character == '\t') {
                    LexerNextCharacter(lexer);
                    IndentTab = 1;
                    continue;
                    ;
                }
                if (character == '\n') {
                    LexerNextCharacter(lexer);
                    RestartLine = 1;
                    break;
                }
                if (character == '/') {
                    int next = fgetc(lexer->input);
                    if (next != EOF) {
                        if (ungetc(next, lexer->input) == EOF) {
                            return 99;
                        }
                    } else if (ferror(lexer->input)) {
                        return 99;
                    }
                    if (next == '/') {
                        LexerNextCharacter(lexer);
                        LexerNextCharacter(lexer);
                        SkipLineComment(lexer);
                        RestartLine = 1;
                        break;
                    }
                    if (next == '*') {
                        LexerNextCharacter(lexer);
                        LexerNextCharacter(lexer);
                        int NewLine = 0;
                        int result = SkipBlockComment(lexer, &NewLine);
                        if (result != 0) {
                            return result;
                        }
                        if (NewLine) {
                            while (1) {
                                int character1 = LexerPreviewCharacter(lexer);
                                if (character1 == ' ' || character1 == '\t') {
                                    LexerNextCharacter(lexer);
                                    continue;
                                }
                                if (character1 != '\n' && character1 != EOF) {
                                    return 1;
                                }
                                break;
                            }
                            if (LexerPreviewCharacter(lexer) == '\n') {
                                LexerNextCharacter(lexer);
                            }
                            RestartLine = 1;
                            break;
                        }
                        comment = 1;
                        continue;
                    }
                }
                break;
            }
            if (RestartLine) {
                continue;
            }
            character = LexerPreviewCharacter(lexer);
            if (character != EOF) {
                if (IndentTab) {
                    return 1;
                }
                size_t count0 = lexer->IndentCount;
                int result = CheckIndent(lexer, indent, token);
                if (result != 0) {
                    return result;
                }
                lexer->LogIndent = indent;
                lexer->LineStart = 0;
                if (lexer->PendingDegents > 0) {
                    continue;
                }
                if (lexer->IndentCount > count0) {
                    return 0;
                }
            }
        }
        character = LexerPreviewCharacter(lexer);
        if (character == ' ' || character == '\t') {
            LexerNextCharacter(lexer);
            continue;
        }
        if (character == '\n') {
            if (lexer->Depth > 0) {
                LexerNextCharacter(lexer);
                while (1) {
                    size_t indent = 0;
                    int tab = 0;
                    while (1) {
                        character = LexerPreviewCharacter(lexer);
                        if (character == ' ') {
                            LexerNextCharacter(lexer);
                            indent++;
                            continue;
                        }
                        if (character == '\t') {
                            LexerNextCharacter(lexer);
                            tab = 1;
                            continue;
                        }
                        break;
                    }
                    character = LexerPreviewCharacter(lexer);
                    if (character == '\n') {
                        LexerNextCharacter(lexer);
                        continue;
                    }
                    if (character == EOF) {
                        break;
                    }
                    if (character == '/') {
                        int next = fgetc(lexer->input);
                        if (next != EOF) {
                            if (ungetc(next, lexer->input) == EOF) {
                                return 99;
                            }
                        } else if (ferror(lexer->input)) {
                            return 99;
                        }
                        if (next == '/') {
                            LexerNextCharacter(lexer);
                            LexerNextCharacter(lexer);
                            SkipLineComment(lexer);
                            if (LexerPreviewCharacter(lexer) == '\n') {
                                LexerNextCharacter(lexer);
                                continue;
                            }
                            break;
                        }
                    }
                    if (tab) {
                        return 1;
                    }
                    if (character != ')' && indent <= lexer->LogIndent) {
                        return 1;
                    }
                    break;
                }
                continue;
            }
            if (lexer->HasToken == 0) {
                LexerNextCharacter(lexer);
                lexer->LineStart = 1;
                continue;
            }
            break;
        }
        if (character == '/') {
            size_t line = lexer->Line;
            size_t column = lexer->Column + 1;
            LexerNextCharacter(lexer);
            int next = LexerPreviewCharacter(lexer);
            if (next == '/') {
                LexerNextCharacter(lexer);
                SkipLineComment(lexer);
                continue;
            }

            if (next == '*') {
                LexerNextCharacter(lexer);
                int NewLine = 0;
                int result = SkipBlockComment(lexer, &NewLine);
                if (result != 0) {
                    return result;
                }
                if (NewLine) {
                    while (1) {
                        int character0 = LexerPreviewCharacter(lexer);
                        if (character0 == ' ' || character0 == '\t') {
                            LexerNextCharacter(lexer);
                            continue;
                        }
                        if (character0 != '\n' && character0 != EOF) {
                            return 1;
                        }
                        break;
                    }
                    if (LexerPreviewCharacter(lexer) == '\n') {
                        LexerNextCharacter(lexer);
                    }
                    lexer->LineStart = 1;
                    if (lexer->HasToken == 0) {
                        continue;
                    }
                    lexer->HasToken = 0;
                    return SetToken(token, TOKEN_EOL, "\n", line, column);
                }
                continue;
            }
            lexer->HasToken = 1;
            return SetToken(token, TOKEN_DIV, "/", line, column);
        }
        break;
    }
    size_t line = lexer->Line;
    size_t column = lexer->Column + 1;
    if (character != EOF) {
        lexer->HasToken = 1;
    }
    if (IdentifierStart(character)) {
        char *value = NULL;
        int result = ReadIdentifier(lexer, &value);
        if (result != 0) {
            return result;
        }
        if (strcmp(value, "s") == 0 && LexerPreviewCharacter(lexer) == '"') {
            free(value);
            return ReadString(lexer, token, TOKEN_INTERPOLATED, line, column);
        }
        TokenType type;
        if (strcmp(value, "_") == 0) {
            type = TOKEN_WILDCARD;
        } else {
            type = IdentifierType(value);
        }

        result = SetToken(token, type, value, line, column);
        free(value);
        return result;
    }
    if (character >= '0' && character <= '9') {
        return ReadNumber(lexer, token, line, column);
    }
    if (character == '.') {
        int next = fgetc(lexer->input);
        if (next != EOF) {
            if (ungetc(next, lexer->input) == EOF) {
                return 99;
            }
        } else if (ferror(lexer->input)) {
            return 99;
        }
        if (next >= '0' && next <= '9') {
            return ReadNumber(lexer, token, line, column);
        }
    }
    switch (character) {
    case '+':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_PLUS, "+", line, column);
    case '-':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_MINUS, "-", line, column);
    case '*':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_MUL, "*", line, column);
    case '(':
        LexerNextCharacter(lexer);
        lexer->Depth++;
        return SetToken(token, TOKEN_LPAREN, "(", line, column);
    case ')':
        LexerNextCharacter(lexer);
        if (lexer->Depth > 0) {
            lexer->Depth--;
        }
        return SetToken(token, TOKEN_RPAREN, ")", line, column);
    case '[':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_LBRACKET, "[", line, column);
    case ']':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_RBRACKET, "]", line, column);
    case ';':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_SEMICOLON, ";", line, column);
    case ':':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_COLON, ":", line, column);
    case '.':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_DOT, ".", line, column);
    case ',':
        LexerNextCharacter(lexer);
        return SetToken(token, TOKEN_COMMA, ",", line, column);
    case '=':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '=') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_EQ, "==", line, column);
        }
        if (LexerPreviewCharacter(lexer) == '>') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_ARROW, "=>", line, column);
        }
        return SetToken(token, TOKEN_ASSIGN, "=", line, column);
    case '<':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '=') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_LTE, "<=", line, column);
        }
        return SetToken(token, TOKEN_LT, "<", line, column);
    case '>':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '=') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_GTE, ">=", line, column);
        }
        return SetToken(token, TOKEN_GT, ">", line, column);
    case '!':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '=') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_NEQ, "!=", line, column);
        }
        return SetToken(token, TOKEN_NOT, "!", line, column);
    case '&':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '&') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_AND, "&&", line, column);
        }
        return 1;
    case '|':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) == '|') {
            LexerNextCharacter(lexer);
            return SetToken(token, TOKEN_OR, "||", line, column);
        }
        return 1;
    case '\n':
        LexerNextCharacter(lexer);
        lexer->LineStart = 1;
        lexer->HasToken = 0;
        return SetToken(token, TOKEN_EOL, "\n", line, column);
    case '"':
        return ReadString(lexer, token, TOKEN_STRING_LITERAL, line, column);
    case '@':
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) != 'm') {
            return 1;
        }
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) != 'a') {
            return 1;
        }
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) != 'i') {
            return 1;
        }
        LexerNextCharacter(lexer);
        if (LexerPreviewCharacter(lexer) != 'n') {
            return 1;
        }
        LexerNextCharacter(lexer);
        if (IdentifierCharacter(LexerPreviewCharacter(lexer))) {
            return 1;
        }
        return SetToken(token, TOKEN_MAIN, "@main", line, column);
    case EOF:
        if (ferror(lexer->input)) {
            return 99;
        }
        if (lexer->HasToken) {
            lexer->HasToken = 0;
            lexer->LineStart = 1;
            return SetToken(token, TOKEN_EOL, "\n", lexer->Line, lexer->Column + 1);
        }
        if (lexer->IndentCount > 1) {
            lexer->IndentCount--;
            return SetToken(token, TOKEN_DEDENT, "", line, column);
        }
        return SetToken(token, TOKEN_EOF, "", line, column);
    default:
        return 1;
    }
}
