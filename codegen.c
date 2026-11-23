/**
 * * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file codegen.c
 * @author Jonáš Bábela (xbabelj00)
 * @brief Implementacia funkcii na generaciu cieloveho kodu
 */

#define _POSIX_C_SOURCE 200809L // kvoli funkci strdup
#include "codegen.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*----------PRACA SO STRING BUFFEROM----------*/
/**
 * @param buff ukazovatel na datovu strukturu string bufferu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief inicializacia string bufferu
 */
int StrBufferInit(stringBuff *buff) {

    buff->length = 0;
    buff->capacity = INIT_SIZE;

    buff->data = malloc(sizeof(char) * buff->capacity);
    if (!buff->data)
        return 99;

    buff->data[0] = '\0';

    return 0;
}

/**
 * @param buff ukazovatel na datovu strukturu string bufferu
 * @brief uvolnenie pamata string bufferu
 */
void StrBufferFree(stringBuff *buff) {

    free(buff->data);

    buff->capacity = 0;
    buff->length = 0;
    buff->data = NULL;
}

/**
 * @param buff ukazovatel na datovu strukturu string bufferu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief dvojnasobne zvacsenie kapacity string bufferu
 */
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

/**
 * @param buff ukazovatel na datovu strukturu string bufferu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief pripojenie stringu na koniec string bufferu
 */
int StrBufferAppend(stringBuff *buff, const char *cmd) {
    size_t cmdLen = strlen(cmd);

    while (buff->length + cmdLen + 1 > buff->capacity) {

        if (StrBufferResize(buff) != 0)
            return 99;
    }

    memcpy(buff->data + buff->length, cmd, cmdLen + 1);
    buff->length += cmdLen;

    return 0;
}

/*-----------Cyklus Generatora----------*/

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief inicializacia struktury generatora kodu
 */
int CodeGenInit(codeGen *cg) {

    if (!cg)
        return 99;

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

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @brief uvolnenie pamate struktury generatora
 */
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

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 99
 * @brief helper funkcia - uvolnenie generatora kodu s navratovou hodnotou 99
 */
int Return99CodeGen(codeGen *cg) {
    if (cg)
        CodeGenFree(cg);
    return 99;
}

/* ---------- Program a funkcie ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia uvodu v cielovom kode s deklarovanim niekolkych pomocnych globalnych premennych
 */
int GenProlog(codeGen *cg) {
    if (!cg)
        return 99;

    if (StrBufferAppend(&cg->out, ".IFJcode26\n") != 0)
        return Return99CodeGen(cg);

    // Pomocne globalne premenne na niektore vypocty
    if (StrBufferAppend(&cg->out, "DEFVAR GF@%discard\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp1\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp2\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp3\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp4\n") != 0 ||
        StrBufferAppend(&cg->out, "DEFVAR GF@%tmp5\n") != 0) {
        return Return99CodeGen(cg);
    }
    if (StrBufferAppend(&cg->out, "JUMP $$main\n\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param funcName nazov spracovavanej funkcie
 * @param isMain urcuje ci je spracovavana funkcia main
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia cieloveho kodu pre hlavicku funkcie
 */
int GenFunStart(codeGen *cg, const char *funcName, bool isMain) {
    if (!cg)
        return 99;
    if (!funcName)
        return Return99CodeGen(cg);

    cg->currentFunc = funcName;

    // Vycistenie priestoru pre deklarovanie premennych
    for (int i = 0; i < cg->varCount; i++) {
        free(cg->vars[i].name);
        free(cg->vars[i].unique);
    }
    cg->varCount = 0;

    // Vynulovanie urovne vnorenia blokov, cyklov a vetvenia
    cg->scopeDepth = 0;
    cg->labelStackTop = 0;

    // Vytvorenie noveho unikatneho ID bloku a ulozenie otvoreneho bloku medzi aktivne
    cg->nextScopeId++;
    cg->activeScopes[cg->scopeDepth] = cg->nextScopeId;
    cg->scopeDepth++; // Nastavenie zakladneho rozsahu funkcie (vnorenie 1)

    // Generacia cieloveho kodu
    if (isMain) {
        if (StrBufferAppend(&cg->funHeader, "LABEL $$main\n") != 0)
            return Return99CodeGen(cg);
    } else {
        if (StrBufferAppend(&cg->funHeader, "LABEL $") != 0)
            return Return99CodeGen(cg);
        if (StrBufferAppend(&cg->funHeader, funcName) != 0)
            return Return99CodeGen(cg);
        if (StrBufferAppend(&cg->funHeader, "\n") != 0)
            return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funHeader, "CREATEFRAME\nPUSHFRAME\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param isMain je aktualna funkcia main?
 * @param bodyLeavesValue ma aktualna funkcia vracat nejaku hodnotu?
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia ukoncenia funkcie v cielovom kode
 */
int GenFunEnd(codeGen *cg, bool isMain, bool bodyLeavesValue) {
    if (!cg)
        return 99;

    if (isMain) {
        if (StrBufferAppend(&cg->funBody, "EXIT int@0\n") != 0)
            return Return99CodeGen(cg);
    } else {

        // Ak funkcia nevracia navratovu hodnotu
        if (!bodyLeavesValue) {
            if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0)
                return Return99CodeGen(cg);
        }

        if (StrBufferAppend(&cg->funBody, "POPFRAME\nRETURN\n") != 0)
            return Return99CodeGen(cg);
    }

    // Pripojenie bufferu hlavicky funkcie k bufferu cieloveho vystupu
    if (cg->funHeader.length > 0) {
        if (StrBufferAppend(&cg->out, cg->funHeader.data) != 0)
            return Return99CodeGen(cg);
    }

    // Pripojenie bufferu tela funkcie k bufferu cieloveho vystupu
    if (cg->funBody.length > 0) {
        if (StrBufferAppend(&cg->out, cg->funBody.data) != 0)
            return Return99CodeGen(cg);
    }

    // Vynulovanie bufferov hlavicky a tela funkcie
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

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param params ukazovatel na pole stringov-parametrov
 * @param paramCount pocet parametrov
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia uvodu v cielovom kode s deklarovanim niekolkych pomocnych globalnych premennych
 */
int GenFuncParams(codeGen *cg, const char **params, int paramCount) {
    if (cg == NULL || (paramCount > 0 && params == NULL)) {
        return Return99CodeGen(cg);
    }

    if (paramCount == 0) {
        return 0;
    }

    /*Parametre najskor zaregistrujeme v normalnom poradi (deklaracia DEFVAR a zapis do cg->vars).
    Najskor deklarovanie premennych, do ktorych sa parametre budu ukladat*/
    for (int i = 0; i < paramCount; i++) {
        if (GenDefVar(cg, params[i]) != 0)
            return 99;
    }

    // Vyberat ich ale budeme naopak, lebo na zasobnik su napushovane zlava
    for (int i = paramCount - 1; i >= 0; i--) {
        const char *unique =
            FindUniqueVar(cg, params[i]); // Najdenie pred chvilou vytvorenej premennej

        if (unique == NULL)
            return Return99CodeGen(cg);

        // Presunutie najvrchnejsieho (praveho) parametra zo zasobnika do prislusnej premennej
        if (StrBufferAppend(&cg->funBody, "POPS LF@") != 0 ||
            StrBufferAppend(&cg->funBody, unique) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0)
            return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param funcName nazov funkcie
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre volanie funkcie
 */
int GenCall(codeGen *cg, const char *funcName) {
    if (cg == NULL || funcName == NULL) {
        return 99;
    }

    if (StrBufferAppend(&cg->funBody, "CALL $") != 0 ||
        StrBufferAppend(&cg->funBody, funcName) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param hasReturnValue ma funkcia navratovu hodnotu?
 * @param isMain je funkcia main?
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre return funkcie
 */
int GenReturn(codeGen *cg, bool hasReturnValue, bool isMain) {
    if (cg == NULL) {
        return 99;
    }

    if (isMain) {
        if (StrBufferAppend(&cg->funBody, "EXIT int@0\n") != 0)
            return Return99CodeGen(cg);

        return 0;
    }

    if (!hasReturnValue) {
        if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0)
            return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funBody, "POPFRAME\nRETURN\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/* ---------- Pushovanie konstant ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param value int na pushnutie
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre pushnutie int
 */
int GenPushInt(codeGen *cg, long long value) {
    if (cg == NULL) {
        return 99;
    }

    // konverzia hodnoty na string
    char numBuf[INIT_SIZE];
    int written = snprintf(numBuf, sizeof(numBuf), "%lld", value);
    if (written < 0 || (size_t)written >= sizeof(numBuf))
        return Return99CodeGen(cg);

    if (StrBufferAppend(&cg->funBody, "PUSHS int@") != 0 ||
        StrBufferAppend(&cg->funBody, numBuf) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param value float na pushnutie
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre pushnutie float
 */
int GenPushFloat(codeGen *cg, double value) {
    if (cg == NULL)
        return 99;

    // konverzia float na int
    char numBuf[INIT_SIZE];
    int written = snprintf(numBuf, sizeof(numBuf), "%a", value);
    if (written < 0 || (size_t)written >= sizeof(numBuf)) {
        return Return99CodeGen(cg);
    }

    if (StrBufferAppend(&cg->funBody, "PUSHS float@") != 0 ||
        StrBufferAppend(&cg->funBody, numBuf) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre pushnutie ()
 */
int GenPushUnit(codeGen *cg) {
    if (cg == NULL)
        return 99;

    if (StrBufferAppend(&cg->funBody, "PUSHS nil@nil\n") != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param value true/false na pushnutie
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre pushnutie bool hodnoty
 */
int GenPushBool(codeGen *cg, bool value) {
    if (cg == NULL)
        return 99;

    const char *instr = value ? "PUSHS bool@true\n" : "PUSHS bool@false\n";
    if (StrBufferAppend(&cg->funBody, instr) != 0) {
        return Return99CodeGen(cg);
    }

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param str string na pushnutie
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu na pushnutie string
 */
int GenPushString(codeGen *cg, const char *str) {
    if (cg == NULL || str == NULL) {
        return 99;
    }

    if (StrBufferAppend(&cg->funBody, "PUSHS string@") != 0) {
        return Return99CodeGen(cg);
    }

    // Hladanie znakov so specialnym zapisom
    char esc[8];
    for (size_t i = 0; str[i] != '\0'; i++) {
        unsigned char c = (unsigned char)str[i]; // Unsigned, aby nebol problem so znamienkom

        // Ak ASCII kod mensi ako 32, # alebo '\\', zapis pomocou specialneho formatu
        if (c <= 32 || c == '#' || c == '\\') {

            snprintf(esc, sizeof(esc), "\\%03u", c); // Specialny format: '\\0xx'

            if (StrBufferAppend(&cg->funBody, esc) != 0)
                return Return99CodeGen(cg);

            // Inak zapis znak klasicky
        } else {
            char ch[2] = {c, '\0'}; // Musime pridat '\0', pretoze to bude posledny znak v bufferi
            if (StrBufferAppend(&cg->funBody, ch) != 0)
                return Return99CodeGen(cg);
        }
    }

    if (StrBufferAppend(&cg->funBody, "\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/* ---------- Premenne a bloky ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param varName nazov premennej
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre deklarovanie lokalnej premennej
 */
int GenDefVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL || cg->scopeDepth <= 0) {
        return Return99CodeGen(cg);
    }

    int currentScopeId = cg->activeScopes[cg->scopeDepth - 1];

    // Zvacsenie pola premennych
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

    // Ak chyba tak vycisti
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

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param varName nazov hladanej premennej
 * @return NULL ak sa premenna nenasla, unikatny nazov premennej ak sa nasla
 * @brief vyhladanie najvnutornejsej aktivnej premennej s danym menom
 */
const char *FindUniqueVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL)
        return NULL;

    // Prehladavanie aktivnych bloky zvnutra von
    for (int d = cg->scopeDepth - 1; d >= 0; d--) {
        int scopeId = cg->activeScopes[d];

        // Prehladavanie aktivnych premennych od konca
        for (int i = cg->varCount - 1; i >= 0; i--) {

            // Ak zhoda meno a ID prehladavaneho bloku, tak return jej unikatneho mena
            if (cg->vars[i].scopeID == scopeId && strcmp(cg->vars[i].name, varName) == 0) {
                return cg->vars[i].unique;
            }
        }
    }
    return NULL;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param varName nazov premennej
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre pushnutie premennej
 */
int GenPushVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL)
        return 99;

    // Najde najvnorenejsiu premennu s rovnakym menom
    const char *unique = FindUniqueVar(cg, varName);
    if (unique == NULL)
        return Return99CodeGen(cg);

    if (StrBufferAppend(&cg->funBody, "PUSHS LF@") != 0 ||
        StrBufferAppend(&cg->funBody, unique) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre vybratia vrcholu zasobnika do premennej
 */
int GenPopToVar(codeGen *cg, const char *varName) {
    if (cg == NULL || varName == NULL)
        return 99;

    const char *unique = FindUniqueVar(cg, varName);
    if (unique == NULL)
        return Return99CodeGen(cg);

    if (StrBufferAppend(&cg->funBody, "POPS LF@") != 0 ||
        StrBufferAppend(&cg->funBody, unique) != 0 || StrBufferAppend(&cg->funBody, "\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu na odpratanie vrchnej hodnoty zasobnika
 */
int GenPopDiscard(codeGen *cg) {
    if (cg == NULL)
        return 99;

    // Pouziva sa pomocna globalna premenna deklarovana vo funkcii GenProlog
    if (StrBufferAppend(&cg->funBody, "POPS GF@%discard\n") != 0)
        return Return99CodeGen(cg);

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief Registracia vchodu do noveho bloku
 */
int GenScopeEnter(codeGen *cg) {
    if (cg == NULL)
        return 99;

    if (cg->scopeDepth >= INIT_SIZE)
        return Return99CodeGen(cg);

    cg->nextScopeId++;                                  // Vytvorenie noveho ID bloku
    cg->activeScopes[cg->scopeDepth] = cg->nextScopeId; // Vlozenie bloku medzi aktivne bloky
    cg->scopeDepth++;                                   // Inkrementacia hlbky vnorenia

    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief registracia opustenia bloku
 */
int GenScopeExit(codeGen *cg) {
    if (cg == NULL || cg->scopeDepth <= 1)
        return Return99CodeGen(cg);

    // Dekrementacia hlbky vnorenia - zaroven odpojenie od aktivnych blokov (iteruje sa podla
    // scopeDepth)
    cg->scopeDepth--;
    return 0;
}

/* ---------- Vyrazy ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu na pretypovanie vrcholu zasobnika z int na float
 */
int GenInt2FloatTop(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "INT2FLOATS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}
/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief Pretypuje druhy prvok od vrchu zasobnika z int na float (lavy operand pri zmiesanom
 * vyraze)
 */
int GenInt2FloatSecond(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n"
                                      "INT2FLOATS\n"
                                      "PUSHS GF@%tmp1\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param isFloat je hodnota float?
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre vytvorenie unarneho minusu (potrebne urobit pomocou 0 - hodnota)
 */
int GenUnaryMinus(codeGen *cg, bool isFloat) {
    if (cg == NULL)
        return 99;

    // Ak float tak sa pushne float 0, ak int tak sa pushne int 0
    const char *zero = isFloat ? "PUSHS float@0x0p+0\n" : "PUSHS int@0\n";
    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n") != 0 ||
        StrBufferAppend(&cg->funBody, zero) != 0 ||
        StrBufferAppend(&cg->funBody, "PUSHS GF@%tmp1\nSUBS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre negaciu hodnoty
 */
int GenNot(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "NOTS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param op operand: '+', '-', '*', '/', '<', 'L' (<=), '>', 'G' (>=), '=' (==), '!' (!=), '&'
 * (&&), '|' (||)
 * @param isString ide o string operaciu?
 * @param isIntDiv ide o delenie int cisel?
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre binarne operacie
 */
int GenBinaryOp(codeGen *cg, char op, bool isString, bool isIntDiv) {
    if (cg == NULL)
        return 99;

    const char *code = NULL;

    switch (op) {
    case '+':

        // Konkatenacia (spajanie) stringov ak ide o stringy
        if (isString) {
            code = "POPS GF@%tmp2\n"
                   "POPS GF@%tmp1\n"
                   "CONCAT GF@%tmp1 GF@%tmp1 GF@%tmp2\n"
                   "PUSHS GF@%tmp1\n";
        } else {

            // Klasicke scitanie ak ide o cisla
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

        // Celociselne delenie : delenie s nejakym floatom
        code = isIntDiv ? "IDIVS\n" : "DIVS\n";
        break;
    case '<':
        code = "LTS\n";
        break;
    case 'L': // <=

        // NIE VACSIE
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

/* ---------- Riadenie toku programu ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre zaciatok konstrukcie if (zavolane po vyhodnoteni podmienky vo vyraze)
 */
int GenIfStart(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop >= INIT_SIZE)
        return Return99CodeGen(cg);

    // Zvysenie indexu vnorenia konstrukcii IF, WHILE
    int id = ++cg->labelCounter;
    cg->labelStack[cg->labelStackTop++] = id;

    // Kontrola splnenie podmienky vo vyraze
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "PUSHS bool@false\n"
             "JUMPIFEQS $else$%d\n",
             id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre koniec vetvy THEN, vytvorenie navestidla pre vetvu ELSE
 */
int GenIfElse(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0)
        return Return99CodeGen(cg);

    // Zistenie ID aktualnej konstrukcie IF
    int id = cg->labelStack[cg->labelStackTop - 1];

    // Vytvorenie navestidla pre vetvu else
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "JUMP $endif$%d\n"
             "LABEL $else$%d\n",
             id, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre koniec konstrukcie IF
 */
int GenIfEnd(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0)
        return Return99CodeGen(cg);

    // Zistenie ID aktualnej konstrukcie IF
    int id = cg->labelStack[--cg->labelStackTop];

    // Generacia navestidla na ukoncenia IF
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf), "LABEL $endif$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre zaciatok cyklu WHILE
 */
int GenWhileStart(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop >= INIT_SIZE)
        return Return99CodeGen(cg);

    // Zvysenie urovne vnorenia konstrukcii IF, WHILE
    int id = ++cg->labelCounter;
    cg->labelStack[cg->labelStackTop++] = id;

    // Vytvorenie navestidla pre zaciatok WHILE
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf), "LABEL $while_start$%d\n", id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre overenie podmienky cyklenia
 */
int GenWhileCond(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0)
        return Return99CodeGen(cg);

    // Zistenie ID aktualnej konstrukcie WHILE
    int id = cg->labelStack[cg->labelStackTop - 1];

    // Kontrola podmienky
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "PUSHS bool@false\n"
             "JUMPIFEQS $while_end$%d\n",
             id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre koniec cyklu WHILE
 */
int GenWhileEnd(codeGen *cg) {
    if (cg == NULL || cg->labelStackTop <= 0)
        return Return99CodeGen(cg);

    // Zistenie ID aktualnej konstrukcie WHILE
    int id = cg->labelStack[--cg->labelStackTop];

    // Generacia navestidla na ukoncenie cyklu a skoku naspat na zaciatok
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "JUMP $while_start$%d\n"
             "LABEL $while_end$%d\n",
             id, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}


/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre zaciatok porovnavania AND
 */ 
int GenAndStart(codeGen *cg) {
    if (!cg) return 99;
    if (cg->labelStackTop >= INIT_SIZE) return Return99CodeGen(cg);

    int id = cg->labelCounter++;
    cg->labelStack[cg->labelStackTop++] = id;

    char buf[INIT_SIZE*2];
    int written = snprintf(buf, sizeof(buf),
                    "PUSHS bool@false\n"
                    "JUMPIFEQS $and_false$%d\n", id);

    if (written < 0 || (size_t)written >= sizeof(buf)) return Return99CodeGen(cg);                         
    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre koniec porovnavania AND
 */ 
int GenAndEnd(codeGen *cg) {
    if (!cg) return 99;
    if (cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[--cg->labelStackTop];

    char buf[INIT_SIZE*2];
    int written = snprintf(buf, sizeof(buf),
                            "JUMP $and_end$%d\n"
                            "LABEL $and_false$%d\n"
                            "PUSHS bool@false\n"
                            "LABEL $and_end$%d\n",
                            id, id, id);

    if (written < 0 || (size_t)written >= sizeof(buf)) return Return99CodeGen(cg);                         
    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}


/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre zaciatok porovnavania OR
 */ 
int GenOrStart(codeGen *cg) {
    if (!cg) return 99;
    if (cg->labelStackTop >= INIT_SIZE) return Return99CodeGen(cg);

    int id = cg->labelCounter++;
    cg->labelStack[cg->labelStackTop++] = id;

    char buf[INIT_SIZE*2];
    int written = snprintf(buf, sizeof(buf),
                    "PUSHS bool@true\n"
                    "JUMPIFEQS $or_true$%d\n", id);

    if (written < 0 || (size_t)written >= sizeof(buf)) return Return99CodeGen(cg);                         
    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre koniec porovnavania OR
 */ 
int GenOrEnd(codeGen *cg) {
    if (!cg) return 99;
    if (cg->labelStackTop <= 0) return Return99CodeGen(cg);

    int id = cg->labelStack[--cg->labelStackTop];

    char buf[INIT_SIZE*2];
    int written = snprintf(buf, sizeof(buf),
                            "JUMP $or_end$%d\n"
                            "LABEL $or_true$%d\n"
                            "PUSHS bool@true\n"
                            "LABEL $or_end$%d\n",
                            id, id, id);

    if (written < 0 || (size_t)written >= sizeof(buf)) return Return99CodeGen(cg);                         
    if (StrBufferAppend(&cg->funBody, buf) != 0) return Return99CodeGen(cg);
    return 0;
}

/* ---------- Built-in funkcie ---------- */

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param type typ, ktory sa bude citat
 * @param defVal prislusna nulova hodnota k nacitavanemu typu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief pomocna funkcia pre generaciu kodu built-in funkcii nacitavania
 */
static int GenBuiltinRead(codeGen *cg, const char *type, const char *defVal) {
    if (cg == NULL)
        return 99;

    int id = ++cg->labelCounter;
    char buf[INIT_SIZE*2];

    // Vygenerovanie navestidla - pokial je nacitana hodnota NULL, tak je ulozena prislusna nulova
    // hodnota
    snprintf(buf, sizeof(buf),
             "READ GF@%%tmp1 %s\n"
             "JUMPIFNEQ $rd_ok$%d GF@%%tmp1 nil@nil\n"
             "MOVE GF@%%tmp1 %s\n"
             "LABEL $rd_ok$%d\n"
             "PUSHS GF@%%tmp1\n",
             type, id, defVal, id);

    if (StrBufferAppend(&cg->funBody, buf) != 0)
        return Return99CodeGen(cg);
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre nacitanie int
 */
int GenBuiltinReadInt(codeGen *cg) { return GenBuiltinRead(cg, "int", "int@0"); }

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre nacitanie float
 */
int GenBuiltinReadDouble(codeGen *cg) { return GenBuiltinRead(cg, "float", "float@0x0p+0"); }

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre nacitanie string
 */
int GenBuiltinReadLine(codeGen *cg) { return GenBuiltinRead(cg, "string", "string@"); }

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre nacitanie string
 */ 
int GenBuiltinReadBool(codeGen *cg) { return GenBuiltinRead(cg, "bool", "bool@false"); }


/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param isUnit funckia bola zavolana bez parametrov
 * @param pushUnitResult je potrebne pushnut na zasobnik unit
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre vypisanie vrchnej hodnoty na zasobniku
 */
int GenBuiltinPrint(codeGen *cg, bool isUnit, bool pushUnitResult) {
    if (cg == NULL)
        return 99;

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

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @param kind 'i' = Int, 'f' = Double, 's' = String, 'u' = Unit, 'b' = Bool
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre prevod int/float do string
 */
int GenBuiltinToStr(codeGen *cg, char kind) {
    if (cg == NULL)
        return 99;

    if (kind == 's') {
        // Na vrchole zasobnika je retazec, ktory netreba dalej prevadzat
        return 0;
    }

    if (kind == 'b') {
        int id = ++cg->labelCounter;
        char buff[INIT_SIZE*2];

        //Pokial je na vrchole zasobnika true - pushni string "true", inak pushni string "false"
        snprintf(buff, sizeof(buff),
                "PUSHS bool@true\n"
                "JUMPIFEQS $toStr_true$%d\n"
                "PUSHS string@false\n"
                "JUMP $toStr_bool_end$%d\n"
                "LABEL $toStr_true$%d\n"
                "PUSHS string@true\n"
                "LABEL $toStr_bool_end$%d\n", id, id, id, id);

        if (StrBufferAppend(&cg->funBody, buff) != 0) return Return99CodeGen(cg);
        return 0;
    }

    //Ak ide o Unit
    if (kind == 'u') {
        if (StrBufferAppend(&cg->funBody, "POPS GF@%discard\n"
                                          "PUSHS string@()\n") != 0) {
            return Return99CodeGen(cg);
        }
        return 0;
    }

    const char *conv = (kind == 'i') ? "INT2STR" : "FLOAT2STR";
    char buf[INIT_SIZE];
    snprintf(buf, sizeof(buf),
             "POPS GF@%%tmp1\n"
             "%s GF@%%tmp1 GF@%%tmp1\n"
             "PUSHS GF@%%tmp1\n",
             conv);

    if (StrBufferAppend(&cg->funBody, buf) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre prevod float do int
 */
int GenBuiltinToInt(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "FLOAT2INTS\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre zistenie dlzky retazca
 */
int GenBuiltinStrLength(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n"
                                      "STRLEN GF@%tmp2 GF@%tmp1\n"
                                      "PUSHS GF@%tmp2\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre prevod int do char
 */
int GenBuiltinChr(codeGen *cg) {
    if (cg == NULL)
        return 99;
    if (StrBufferAppend(&cg->funBody, "POPS GF@%tmp1\n"
                                      "INT2CHAR GF@%tmp2 GF@%tmp1\n"
                                      "PUSHS GF@%tmp2\n") != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre built-in funkciu ord
 */
int GenBuiltinOrd(codeGen *cg) {
    if (!cg)
        return 99;

    int id = ++cg->labelCounter;
    char buf[INIT_SIZE * 8];
    int written = snprintf(buf, sizeof(buf),
                           "POPS GF@%%tmp1\n" // tmp1 = index
                           "POPS GF@%%tmp2\n" // tmp2 = value

                           "LT GF@%%tmp3 GF@%%tmp1 int@0\n" // index < 0
                           "JUMPIFEQ $ord_err$%d GF@%%tmp3 bool@true\n"

                           "STRLEN GF@%%tmp3 GF@%%tmp2\n"       // tmp3 = len
                           "LT GF@%%tmp3 GF@%%tmp1 GF@%%tmp3\n" // index < len ?
                           "JUMPIFEQ $ord_err$%d GF@%%tmp3 bool@false\n"

                           "STRI2INT GF@%%tmp3 GF@%%tmp2 GF@%%tmp1\n"
                           "PUSHS GF@%%tmp3\n"
                           "JUMP $ord_end$%d\n"

                           "LABEL $ord_err$%d\n"
                           "PUSHS int@-1\n"
                           "LABEL $ord_end$%d\n",
                           id, id, id, id, id);

    if (written < 0 || (size_t)written >= sizeof(buf)) {
        return Return99CodeGen(cg);
    }
    if (StrBufferAppend(&cg->funBody, buf) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

/**
 * @param cg ukazovatel na datovu strukturu generatora kodu
 * @return 0 ak nedoslo k chybe, 99 ak doslo k chybe pri praci s pamatou
 * @brief generacia kodu pre built-in funkciu subStr
 */
int GenBuiltinSubStr(codeGen *cg) {
    if (cg == NULL)
        return 99;

    int id = ++cg->labelCounter;
    char buf[INIT_SIZE * 9];
    int written = snprintf(buf, sizeof(buf),
                           "POPS GF@%%tmp1\n"             // tmp1 = end
                           "POPS GF@%%tmp2\n"             // tmp2 = start
                           "POPS GF@%%tmp3\n"             // tmp3 = value
                           "STRLEN GF@%%tmp4 GF@%%tmp3\n" // tmp4 = len

                           "LT GF@%%tmp5 GF@%%tmp2 int@0\n" // start < 0
                           "JUMPIFEQ $ss_err$%d GF@%%tmp5 bool@true\n"
                           "GT GF@%%tmp5 GF@%%tmp2 GF@%%tmp1\n" // start > end
                           "JUMPIFEQ $ss_err$%d GF@%%tmp5 bool@true\n"
                           "GT GF@%%tmp5 GF@%%tmp1 GF@%%tmp4\n" // end > len
                           "JUMPIFEQ $ss_err$%d GF@%%tmp5 bool@true\n"
                           "JUMPIFEQ $ss_err$%d GF@%%tmp2 GF@%%tmp4\n" // start >= len

                           "MOVE GF@%%tmp5 string@\n" // vysledok
                           "LABEL $ss_loop$%d\n"
                           "JUMPIFEQ $ss_done$%d GF@%%tmp2 GF@%%tmp1\n" // start == end
                           "GETCHAR GF@%%tmp4 GF@%%tmp3 GF@%%tmp2\n"
                           "CONCAT GF@%%tmp5 GF@%%tmp5 GF@%%tmp4\n"
                           "ADD GF@%%tmp2 GF@%%tmp2 int@1\n"
                           "JUMP $ss_loop$%d\n"

                           "LABEL $ss_err$%d\n"
                           "EXIT int@40\n"

                           "LABEL $ss_done$%d\n"
                           "PUSHS GF@%%tmp5\n",
                           id, id, id, id, id, id, id, id, id);

    if (written < 0 || (size_t)written >= sizeof(buf)) {
        return Return99CodeGen(cg);
    }
    if (StrBufferAppend(&cg->funBody, buf) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}

int GenBuiltinFloor(codeGen *cg) {
    if (cg == NULL) return 99;

    int id = ++cg->labelCounter;
    char buf[INIT_SIZE*8];
    int written = snprintf(buf, sizeof(buf),
            "POPS GF@%%tmp2\n"                          //tmp2=povodny float
            "FLOAT2INT GF@%%tmp1 GF@%%tmp2\n"           //oseknutie k nule
            "INT2FLOAT GF@%%tmp1 GF@%%tmp1\n"           //spat na float
            "PUSHS GF@%%tmp2\n"
            "PUSHS GF@%%tmp1\n"
            "LTS\n"                                     //povodny < oseknuty
            "PUSHS bool@true\n"
            "JUMPIFEQS $floor_dec$%d\n"
            "JUMP $floor_end$%d\n"
            "LABEL $floor_dec$%d\n"
            "SUB GF@%%tmp1 GF@%%tmp1 float@0x1p+0\n"    //-1 pre zaporne desatinne cisla
            "LABEL $floor_end$%d\n"
            "PUSHS GF@%%tmp1\n",
            id, id, id, id);

    if (written < 0 || (size_t)written >= sizeof(buf) ||
        StrBufferAppend(&cg->funBody, buf) != 0) {
        return Return99CodeGen(cg);
    }
    return 0;
}