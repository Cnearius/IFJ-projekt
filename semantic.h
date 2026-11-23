/**
 * * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file semantic.h
 * @author Michal Caban (xcabanm00)
 * @brief Rozhranie semantickej analyzy, akcie volane parserom a typova kontrola
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "types.h"
#include <stdbool.h>

#define NOT_YET_KNOWN_RETURN_TYPE 98

int SemAnalysisInit(void);
void SemAnalysisFree(void);
int FunctionType(const typeId *params, int paramCount, typeId returnType, typeId *outId);
int FunctionHeader(const char *name, bool hasReturnType, typeId returnType,
                   const typeId *paramTypes, int paramCount, bool isMain);

int FuncBodyStart(const char *name, const char **paramNames, const typeId *paramTypes,
                  int paramCount,
                  bool isMain); // vytvori novy scope pre funkciu a vlozi donho parametre
int FuncBodyEnd(bool hasResult,
                typeId resultType); // uzavrie scope, overi vysledok a oznaci funkciu za definovanu
int EnterBlock(void);               // vytvori scope pre blok (napriklad if)
int LeaveBlock(void);               // zrusi scope pre blok
int DeclareVar(const char *name, bool hasType, typeId type, bool isConst,
               typeId initType);                   // inicializacia novej premennej / konstanty
int GetVarType(const char *name, typeId *outType); // najde premennu/funkciu a vrati jej type
int Call(const char *name, int argCount, const typeId *argTypes,
         typeId *outType); // overi volanie a vrati navratovy typ
int AssignValueToVar(const char *name,
                     typeId valueType); // overi zapis do premennej a ci sedia typy
int Operator(typeId left, typeId right, int op, typeId *outType);
int ValmemMember(typeId objType, const char *member, typeId *outType); // toto su valmem funkcie
int ConditionType(typeId conditionType);     // overenie lebo podmienka musi mat vzdy typ boolean
int Return(bool hasValue, typeId valueType); // pravidla pre return
int StatementValueType(typeId valueType);
int CheckMain(void); // overi ze program obsahuje definiciu main
void SemInferMode(bool enabled);
int AbortFuncBody(void);
bool FunctionHasType(const char *name);
int DefinedFunctionCount(void);
int ResetSemAnalysisForFinalAnalysis(void);
int GetFunctionSignature(const char *name, typeId *typeArr, const int arrSize, int *capacity,
                         typeId *returnType);
void SetVarInitInfo(const char *initName, bool isBareIdent);
int LambdaBodyStart(const char **paramNames, const typeId *paramTypes, int paramCount);
int LambdaBodyEnd(bool hasResult, typeId resultType, typeId *outType);
#endif
