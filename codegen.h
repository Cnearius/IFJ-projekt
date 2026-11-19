/**
 * * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file codegen.h
 * @author Jonáš Bábela (xbabelj00)
 * @brief Definicia funkcii a datovych struktur pre generaciu cieloveho kodu
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} stringBuff;

typedef struct {
    char *name;
    char *unique;
    int scopeID;
} varScope;

typedef struct {
    stringBuff out;
    stringBuff funHeader;
    stringBuff funBody;

    int labelCounter;       
    int nextScopeId;         

    int labelStack[128];
    int labelStackTop;

    varScope *vars;     
    int varCount;
    int varCapacity;
    int activeScopes[128];  
    int scopeDepth;          

    const char *currentFunc; 

} codeGen;

/* ---------- String buffer ---------- */
int  StrBufferInit(stringBuff *buff);
void StrBufferFree(stringBuff *buff);
int  StrBufferResize(stringBuff *buff);
int  StrBufferAppend(stringBuff *buff, const char *cmd);

/* ---------- Cyklus generatora ---------- */
int  CodeGenInit(codeGen *cg);
void CodeGenFree(codeGen *cg);
int  Return99CodeGen(codeGen *cg);

/* ---------- Program a funkcie ---------- */
int GenProlog(codeGen *cg);
int GenFunStart(codeGen *cg, const char *funcName, bool isMain);
int GenFunEnd(codeGen *cg, bool isMain, bool bodyLeavesValue);   /* if you add the flag: (codeGen *cg, bool isMain, bool bodyLeavesValue) */
int GenFuncParams(codeGen *cg, const char **params, int paramCount);
int GenCall(codeGen *cg, const char *funcName);
int GenReturn(codeGen *cg, bool hasReturnValue, bool isMain);

/* ---------- Pushovanie konstant ---------- */
int GenPushInt(codeGen *cg, long long value);
int GenPushFloat(codeGen *cg, double value);
int GenPushUnit(codeGen *cg);
int GenPushBool(codeGen *cg, bool value);
int GenPushString(codeGen *cg, const char *str);

/* ---------- Premenne a bloky ---------- */
int GenDefVar(codeGen *cg, const char *varName);
const char *FindUniqueVar(codeGen *cg, const char *varName);
int GenPushVar(codeGen *cg, const char *varName);
int GenPopToVar(codeGen *cg, const char *varName);
int GenPopDiscard(codeGen *cg);
int GenScopeEnter(codeGen *cg);
int GenScopeExit(codeGen *cg);

/* ---------- Vyrazy ---------- */
int GenInt2FloatTop(codeGen *cg);
int GenInt2FloatSecond(codeGen *cg);
int GenUnaryMinus(codeGen *cg, bool isFloat);
int GenNot(codeGen *cg);
int GenBinaryOp(codeGen *cg, char op, bool isString, bool isIntDiv);

/* ---------- Riadenie toku programu ---------- */
int GenIfStart(codeGen *cg);
int GenIfElse(codeGen *cg);
int GenIfEnd(codeGen *cg);
int GenWhileStart(codeGen *cg);
int GenWhileCond(codeGen *cg);
int GenWhileEnd(codeGen *cg);

/* ---------- Built-in funkcie ---------- */
int GenBuiltinReadInt(codeGen *cg);
int GenBuiltinReadDouble(codeGen *cg);
int GenBuiltinReadString(codeGen *cg);
int GenBuiltinPrint(codeGen *cg, bool isUnit, bool pushUnitResult);
int GenBuiltinToStr(codeGen *cg, char kind);
int GenBuiltinToInt(codeGen *cg);
int GenBuiltinStrLength(codeGen *cg);
int GenBuiltinChr(codeGen *cg);

#endif