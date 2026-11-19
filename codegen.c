/**
 * * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file codegen.c
 * @author Jonáš Bábela (xbabelj00)
 * @brief Implementacia funkcii na generaciu cieloveho kodu 
 */

#define _POSIX_C_SOURCE 200809L //kvoli funkci strdup
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INIT_SIZE 128

int StrBufferInit(stringBuff *buff) {

    buff->length = 0;
    buff->capacity = INIT_SIZE;
    
    buff->data = malloc(sizeof(char) * buff->capacity);
    if (!buff->data) return 99;
    
    buff->data[0] = '\0';

    return 0;
}

void StrBufferFree(stringBuff *buff) {

    free(buff->data);

    buff->capacity = 0;
    buff->length = 0;
    buff->data = NULL;
}

int StrBufferResize(stringBuff *buff) {

    size_t newCapacity = buff->capacity * 2;
    char *temp;

    temp = realloc(buff->data, sizeof(char) * newCapacity);

    if (!temp) {
        StrBufferFree(buff);
        return 99;
    } 

    buff->data = temp;
    buff->capacity = newCapacity;

    return 0;
}

int StrBufferAppend(stringBuff *buff, const char *cmd) {
    size_t cmdLen = strlen(cmd);

    while (buff->length + cmdLen + 1 > buff->capacity) {

        if (StrBufferResize(buff) != 0) return 99;
    }

    memcpy(buff->data + buff->length, cmd, cmdLen + 1);
    buff->length += cmdLen;

    return 0;
}

int CodeGenInit(codeGen *cg) {
    
    if (!cg) return 99;

    int mem = 0;
    mem += StrBufferInit(&cg->out);
    mem += StrBufferInit(&cg->funBody);
    mem += StrBufferInit(&cg->funHeader);

    if (mem != 0) {
        StrBufferFree(&cg->out);
        StrBufferFree(&cg->funBody);
        StrBufferFree(&cg->funHeader);
        return 99;
    }

    cg->labelCounter = 0;
    cg->nextScopeId = 0;

    for (int i = 0; i < INIT_SIZE; i++) {
        cg->labelStack[i] = 0;
        cg->activeScopes[i] = 0;
    }
    cg->labelStackTop = 0;
    cg->scopeDepth = 0;

    cg->vars = NULL;
    cg->varCount = 0;
    cg->varCapacity = 0;

    cg->currentFunc = NULL;

    return 0;
}

void CodeGenFree(codeGen *cg) {
    StrBufferFree(&cg->out);
    StrBufferFree(&cg->funBody);
    StrBufferFree(&cg->funHeader);

    cg->labelCounter = 0;
    cg->nextScopeId = 0;

    for (int i = 0; i < INIT_SIZE; i++) {
        cg->labelStack[i] = 0;
        cg->activeScopes[i] = 0;
    }
    cg->labelStackTop = 0;
    cg->scopeDepth = 0;

    for (int i = 0; i < cg->varCount; i++) {
        free(cg->vars[i].name);
        free(cg->vars[i].unique);
    }

    free(cg->vars);
    cg->vars = NULL;
    cg->varCount = 0;
    cg->varCapacity = 0;

    cg->currentFunc = NULL;
}

int Return99CodeGen(codeGen *cg) {
    if (cg) CodeGenFree(cg);
    return 99;
}

/**
 * @param cg datova struktura generatoru kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generuje hlavicku programu v cielovom kode
 */
int GenProlog(codeGen *cg) {
    if (!cg) return 99;

    if (StrBufferAppend(&cg->out, ".IFJcode26\n") != 0) return Return99CodeGen(cg);
    
    //Pomocne globalne premenne na niektore vypocty
    if (StrBufferAppend(&cg->out, "DEFVAR GF@%discard\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp1\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp2\n") != 0) {
        return Return99CodeGen(cg);
    }
    if (StrBufferAppend(&cg->out, "JUMP $$main\n\n") != 0) return Return99CodeGen(cg);


    return 0;
}

/**
 * @param cg datova struktura generatoru kodu
 * @param funcName meno spracovavanej funkcie
 * @param isMain urcuje ci je spracovavana funkcia main
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generuje cielovy kod pre hlavicku funkcie
 */
int GenFunStart(codeGen *cg, const char *funcName, bool isMain) {
    if (!cg) return 99;
    if (!funcName) return Return99CodeGen(cg);

    cg->currentFunc = funcName;

    for (int i = 0; i < cg->varCount; i++) {
        free(cg->vars[i].name);
        free(cg->vars[i].unique);
    }
    cg->varCount = 0;
    
    cg->scopeDepth = 0;
    cg->labelStackTop = 0;

    cg->nextScopeId++;
    cg->activeScopes[cg->scopeDepth] = cg->nextScopeId;
    cg->scopeDepth++;

    if (isMain) {
        if (StrBufferAppend(&cg->funHeader, "LABEL $$main\n") != 0) return Return99CodeGen(cg);
    } else {
        if (StrBufferAppend(&cg->funHeader, "LABEL $") != 0) return Return99CodeGen(cg);
        if (StrBufferAppend(&cg->funHeader, funcName) != 0) return Return99CodeGen(cg);
        if (StrBufferAppend(&cg->funHeader, "\n") != 0) return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funHeader, "CREATEFRAME\nPUSHFRAME\n") != 0) return Return99CodeGen(cg);

    return 0;
}

int GenFunEnd(codeGen *cg, bool isMain, bool bodyLeavesValue) {
    if (!cg) return 99;

    if (isMain) {
        if (StrBufferAppend(&cg->funBody, "EXIT int@0\n") != 0) return Return99CodeGen(cg);
    } else {
        if (!bodyLeavesValue) {
            if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0) return Return99CodeGen(cg);
        }
        if (StrBufferAppend(&cg->funBody, "POPFRAME\nRETURN\n") != 0) return Return99CodeGen(cg);
    }

    if (cg->funHeader.length > 0) {
        if (StrBufferAppend(&cg->out, cg->funHeader.data) != 0) return Return99CodeGen(cg);
    }

    if (cg->funBody.length > 0) {
        if (StrBufferAppend(&cg->out, cg->funBody.data) != 0) return Return99CodeGen(cg);
    }
    

    cg->funHeader.length = 0;
    if (cg->funHeader.data != NULL) {
        cg->funHeader.data[0] = '\0';
    }

    cg->funBody.length = 0;
    if (cg->funBody.data != NULL) {
        cg->funBody.data[0] = '\0';
    }

    for (int i = 0; i < cg->varCount; i++) {
        free(cg->vars[i].name);
        free(cg->vars[i].unique);
    }

    cg->scopeDepth = 0;
    cg->labelStackTop = 0;
    cg->varCount = 0;
    cg->currentFunc = NULL;

    return 0;
}

int GenCall(codeGen *cg, const char *funcName) {
    if (cg == NULL || funcName == NULL) {
        return 99;
    }

    if (StrBufferAppend(&cg->funBody, "CALL $") != 0 ||
        StrBufferAppend(&cg->funBody, funcName) != 0 ||
        StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}


int GenPushInt(codeGen *cg, long long value) {
    if (cg == NULL) {
        return 99;
    }

    char numBuf[INIT_SIZE];
    int written = snprintf(numBuf, sizeof(numBuf), "%lld", value);
    if (written < 0 || (size_t)written >= sizeof(numBuf)) {
        return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funBody, "PUSHS int@") != 0 ||
        StrBufferAppend(&cg->funBody, numBuf) != 0 ||
        StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

int GenPushFloat(codeGen *cg, double value) {
    if (cg == NULL) return 99;

    char numBuf[INIT_SIZE];
    int written = snprintf(numBuf, sizeof(numBuf), "%a", value);
    if (written < 0 || (size_t)written >= sizeof(numBuf)) {
        return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funBody, "PUSHS float@") != 0 ||
        StrBufferAppend(&cg->funBody, numBuf) != 0 ||
        StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

int GenPushUnit(codeGen *cg) {
    if (cg == NULL) return 99;

    if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

int GenPushBool(codeGen *cg, bool value) {
    if (cg == NULL) return 99;

    const char *instr = value ? "PUSHS bool@true\n" : "PUSHS bool@false\n";
    if (StrBufferAppend(&cg->funBody, instr) != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

int GenPushString(codeGen *cg, const char *str) {
    if (cg == NULL || str == NULL) {
        return 99;
    }

    if (StrBufferAppend(&cg->funBody, "PUSHS string@") != 0) {
        return Return99CodeGen(cg);
    }

    char esc[8];
    for (size_t i = 0; str[i] != '\0'; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c <= 32 || c == '#' || c == '\\') {
            snprintf(esc, sizeof(esc), "\\%03u", c);
            if (StrBufferAppend(&cg->funBody, esc) != 0) {
                return Return99CodeGen(cg);
            }
        } else {
            char ch[2] = {c, '\0'};
            if (StrBufferAppend(&cg->funBody, ch) != 0) {
                return Return99CodeGen(cg);
            }
        }
    }

    if (StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}



int GenDefVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL || cg->scopeDepth <= 0) {
        return Return99CodeGen(cg);
    }

    int currentScopeId = cg->activeScopes[cg->scopeDepth - 1];

    //Zvacsenie pola premennych
    if (cg->varCount >= cg->varCapacity) {
        int newCap = (cg->varCapacity == 0) ? INIT_SIZE : cg->varCapacity * 2;
        varScope *newVars = realloc(cg->vars, newCap * sizeof(varScope));
        if (newVars == NULL) {
            return Return99CodeGen(cg);
        }
        cg->vars = newVars;
        cg->varCapacity = newCap;
    }

    // Vytvorenie unikatneho nazvu premennej v cielovom jazyku (nazov$cislo_bloku)
    char unique[INIT_SIZE];
    int written = snprintf(unique, sizeof(unique), "%s$%d", varName, currentScopeId);
    if (written < 0 || (size_t)written >= sizeof(unique)) {
        return Return99CodeGen(cg);
    }

    // Ulozenie premennej do zoznamu premennych
    cg->vars[cg->varCount].name = strdup(varName);
    cg->vars[cg->varCount].unique = strdup(unique);
    cg->vars[cg->varCount].scopeID = currentScopeId;

    if (!cg->vars[cg->varCount].name || !cg->vars[cg->varCount].unique) {
        free(cg->vars[cg->varCount].name);
        free(cg->vars[cg->varCount].unique);
        return Return99CodeGen(cg);
    }

    cg->varCount++;

    // Vypisanie cieloveho kodu
    if (StrBufferAppend(&cg->funHeader, "DEFVAR LF@") != 0 ||
        StrBufferAppend(&cg->funHeader, unique) != 0 ||
        StrBufferAppend(&cg->funHeader, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

// Nájde najvnútornejšiu platnú verziu premennej (rieši shadowing)
const char *FindUniqueVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL) return NULL;

    // Prehladavame aktivne bloky zvnutra von
    for (int d = cg->scopeDepth - 1; d >= 0; d--) {
        int scopeId = cg->activeScopes[d];
        for (int i = cg->varCount - 1; i >= 0; i--) {
            if (cg->vars[i].scopeID == scopeId && strcmp(cg->vars[i].name, varName) == 0) {
                return cg->vars[i].unique;
            }
        }
    }
    return NULL;
}




// Vlozi hodnotu premennej na zasobnik
int GenPushVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL) return 99;

    const char *unique = FindUniqueVar(cg, varName);
    if (unique == NULL) return Return99CodeGen(cg);

    if (StrBufferAppend(&cg->funBody, "PUSHS LF@") != 0 ||
        StrBufferAppend(&cg->funBody, unique) != 0 ||
        StrBufferAppend(&cg->funBody, "\n") != 0) return Return99CodeGen(cg);

    return 0;
}

// Vyberie vrchol zasobnika a ulozi do premennej
int GenPopToVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL) return 99;

    const char *unique = FindUniqueVar(cg, varName);
    if (unique == NULL) return Return99CodeGen(cg);

    if (StrBufferAppend(&cg->funBody, "POPS LF@") != 0 ||
        StrBufferAppend(&cg->funBody, unique) != 0 ||
        StrBufferAppend(&cg->funBody, "\n") != 0) return Return99CodeGen(cg);

    return 0;
}

// Kos na odpratanie vrcholu zasobnika
int GenPopDiscard(codeGen *cg) {
    if (cg == NULL) return 99;

    if (StrBufferAppend(&cg->funBody, "POPS GF@%discard\n") != 0) return Return99CodeGen(cg);

    return 0;
}



int GenFuncParams(codeGen *cg, const char **params, int paramCount) {
    if (cg == NULL || (paramCount > 0 && params == NULL)) {
        return Return99CodeGen(cg);
    }

    if (paramCount == 0) {
        return 0;
    }

    // Parametre najskor zaregistrujeme v normalnom poradi (deklaracia DEFVAR a zapis do cg->vars)
    for (int i = 0; i < paramCount; i++) {
        if (GenDefVar(cg, params[i]) != 0) return 99;
    }

    // Vyberat ich ale budeme naopak
    for (int i = paramCount - 1; i >= 0; i--) {
        const char *unique = FindUniqueVar(cg, params[i]);
        
        if (unique == NULL) return Return99CodeGen(cg);

        if (StrBufferAppend(&cg->funBody, "POPS LF@") != 0 ||
            StrBufferAppend(&cg->funBody, unique) != 0 ||
            StrBufferAppend(&cg->funBody, "\n") != 0) return Return99CodeGen(cg);
    }

    return 0;

}


int GenReturn(codeGen *cg, bool hasReturnValue, bool isMain) {
    if (cg == NULL) {
        return 99;
    }

    if (isMain) {
        if (StrBufferAppend(&cg->funBody, "EXIT int@0\n") != 0) return Return99CodeGen(cg);
        
        return 0;
    }

    if (!hasReturnValue) {
        if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0) return Return99CodeGen(cg);
        
    }

    if (StrBufferAppend(&cg->funBody, "POPFRAME\nRETURN\n") != 0) return Return99CodeGen(cg);
    

    return 0;
}


int GenScopeEnter(codeGen *cg) {
    if (cg == NULL) return 99;

    if (cg->scopeDepth >= INIT_SIZE) return Return99CodeGen(cg);

    cg->nextScopeId++;
    cg->activeScopes[cg->scopeDepth] = cg->nextScopeId;
    cg->scopeDepth++;

    return 0;
}

int GenScopeExit(codeGen *cg) {
    if (cg == NULL || cg->scopeDepth <= 1) return Return99CodeGen(cg);

    cg->scopeDepth--;
    return 0;
}



// Pretypuje vrchol zásobníka z Int na Double
int GenInt2FloatTop(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody, "INT2FLOATS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

// Pretypuje druhý prvok od vrchu z Int na Double (ľavý operand pri zmiešanom výraze)
int GenInt2FloatSecond(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody,
            "POPS GF@%tmp1\n"
            "INT2FLOATS\n"
            "PUSHS GF@%tmp1\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}



int GenUnaryMinus(codeGen *cg, bool isFloat) {
    if (cg == NULL) return 99;

    const char *zero = isFloat ? "PUSHS float@0x0p+0\n" : "PUSHS int@0\n";
    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n") != 0 ||
        StrBufferAppend(&cg->funBody, zero) != 0 ||
        StrBufferAppend(&cg->funBody, "PUSHS GF@%tmp1\nSUBS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

int GenNot(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody, "NOTS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}




// op: '+', '-', '*', '/', '<', 'L' (<=), '>', 'G' (>=), '=' (==), '!' (!=), '&' (&&), '|' (||)
int GenBinaryOp(codeGen *cg, char op, bool isString, bool isIntDiv) {
    if (cg == NULL) return 99;

    const char *code = NULL;

    switch (op) {
        case '+':

            //Konkatenacia (spajanie) stringov ak ide o stringy
            if (isString) {
                code = "POPS GF@%tmp2\n"
                       "POPS GF@%tmp1\n"
                       "CONCAT GF@%tmp1 GF@%tmp1 GF@%tmp2\n"
                       "PUSHS GF@%tmp1\n";
            } else {

                //Klasicke scitanie ak ide o cisla
                code = "ADDS\n";
            }
            break;
        case '-':
            code = "SUBS\n";
            break;
        case '*':
            code = "MULS\n";
            break;
        case '/':

            //Celociselne delenie : delenie s nejakym floatom
            code = isIntDiv ? "IDIVS\n" : "DIVS\n";
            break;
        case '<':
            code = "LTS\n";
            break;
        case 'L': // <=

            //NIE VACSIE
            code = "GTS\nNOTS\n";
            break;
        case '>':
            code = "GTS\n";
            break;
        case 'G': // >=
            code = "LTS\nNOTS\n";
            break;
        case '=': // ==
            code = "EQS\n";
            break;
        case '!': // !=
            code = "EQS\nNOTS\n";
            break;
        case '&': // &&
            code = "ANDS\n";
            break;
        case '|': // ||
            code = "ORS\n";
            break;
        default:
            return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funBody, code) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}


// 1. Zavoláš hneď po vyhodnotení podmienky if (výsledok bool je na vrchole zásobníka)
int GenIfStart(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop >= INIT_SIZE) return Return99CodeGen(cg);

    int id = ++cg->labelCounter;
    cg->labelStack[cg->labelStackTop++] = id;

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "PUSHS bool@false\n"
             "JUMPIFEQS $else$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

// 2. Zavoláš na konci vetvy THEN (pred spracovaním prípadného ELSE)
int GenIfElse(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[cg->labelStackTop - 1];

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "JUMP $endif$%d\n"
             "LABEL $else$%d\n", id, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

// 3. Zavoláš na úplnom konci celého if-else príkazu
int GenIfEnd(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[--cg->labelStackTop];

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf), "LABEL $endif$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}


// 1. Zavoláš PRED vyhodnotením podmienky cyklu while
int GenWhileStart(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop >= INIT_SIZE) return Return99CodeGen(cg);

    int id = ++cg->labelCounter;
    cg->labelStack[cg->labelStackTop++] = id;

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf), "LABEL $while_start$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

// 2. Zavoláš HNEĎ PO vyhodnotení podmienky (výsledok bool je na vrchole zásobníka)
int GenWhileCond(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[cg->labelStackTop - 1];

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "PUSHS bool@false\n"
             "JUMPIFEQS $while_end$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

// 3. Zavoláš na KONCI tela cyklu while
int GenWhileEnd(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[--cg->labelStackTop];

    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "JUMP $while_start$%d\n"
             "LABEL $while_end$%d\n", id, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}


static int GenBuiltinRead(codeGen *cg, const char *type, const char *defVal) {
    if (cg == NULL) return 99;

    int id = ++cg->labelCounter;
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "READ GF@%%tmp1 %s\n"
             "JUMPIFNEQ $rd_ok$%d GF@%%tmp1 nil@nil\n"
             "MOVE GF@%%tmp1 %s\n"
             "LABEL $rd_ok$%d\n"
             "PUSHS GF@%%tmp1\n", type, id, defVal, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

int GenBuiltinReadInt(codeGen *cg)    { return GenBuiltinRead(cg, "int", "int@0"); }
int GenBuiltinReadDouble(codeGen *cg) { return GenBuiltinRead(cg, "float", "float@0x0p+0"); }
int GenBuiltinReadString(codeGen *cg) { return GenBuiltinRead(cg, "string", "string@"); }



// Vypíše hodnotu z vrcholu dátového zásobníka cez WRITE
// isUnit: true ak ide o hodnotu () typu Unit (vypíše "()")
// pushUnitResult: true ak má na zásobníku zostať nil@nil (návratová hodnota Unit)
int GenBuiltinPrint(codeGen *cg, bool isUnit, bool pushUnitResult) {
    if (cg == NULL) return 99;

    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n") != 0) {
        return Return99CodeGen(cg);
    }

    if (isUnit) {
        if (StrBufferAppend(&cg->funBody, "WRITE string@()\n") != 0) {
            return Return99CodeGen(cg);
        }
    } else {
        if (StrBufferAppend(&cg->funBody, "WRITE GF@%tmp1\n") != 0) {
            return Return99CodeGen(cg);
        }
    }

    if (pushUnitResult) {
        if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0) {
            return Return99CodeGen(cg);
        }
    }

    return 0;
}




// toStr pre Int / Double / String
// kind: 'i' = Int, 'f' = Double, 's' = String
int GenBuiltinToStr(codeGen *cg, char kind) {
    if (cg == NULL) return 99;

    if (kind == 's') {
        // Reťazec už je na vrchole zásobníka
        return 0;
    }

    const char *conv = (kind == 'i') ? "INT2STR" : "FLOAT2STR";
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "POPS GF@%%tmp1\n"
             "%s GF@%%tmp1 GF@%%tmp1\n"
             "PUSHS GF@%%tmp1\n", conv);

    if (StrBufferAppend(&cg->funBody, buf) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

// toInt(value: Double): Int
int GenBuiltinToInt(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody, "FLOAT2INTS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}



// strLength(s: String): Int
int GenBuiltinStrLength(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody,
            "POPS GF@%tmp1\n"
            "STRLEN GF@%tmp2 GF@%tmp1\n"
            "PUSHS GF@%tmp2\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

// chr(i: Int): String
int GenBuiltinChr(codeGen *cg) {
    if (cg == NULL) return 99;
    if (StrBufferAppend(&cg->funBody,
            "POPS GF@%tmp1\n"
            "INT2CHAR GF@%tmp2 GF@%tmp1\n"
            "PUSHS GF@%tmp2\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}



