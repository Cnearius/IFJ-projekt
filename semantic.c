/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file semantic.c
 * @author Michal Caban (xcabanm00)
 * @brief Implementacia semantickej analyzy
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "symtable.h"
#include "types.h"

#define TYPE_ID_BASE 100

typedef struct sFuncType {
    int paramCount;
    typeId *idParams;
    typeId returnId;
} funcType;

typedef struct sTypeStore {
    funcType *functionTypes;
    int filledCount;
    int capacity;
} typeStore;

static typeStore types;
static symTable *rootTable;

/**
 * @param types  ukazovatel na struct types
 * @brief funkcia na alokaciu a realokaciu pola functionTypes
 * pri neuspechu ostane "types" bez zmeny
 * @return false pri zlyhani, true pri uspechu
 */
static bool IncreaseTypeStoreSize(typeStore *types) {
    int newCapacity = types->capacity == 0 ? 8 : types->capacity * 2;
    funcType *newTypes = realloc(types->functionTypes, (size_t)newCapacity * sizeof(funcType));
    if (newTypes == NULL)
        return false;
    types->functionTypes = newTypes;
    types->capacity = newCapacity;
    return true;
}

/**
 * @param params Pole parametrov ak je 0 parametrov, moze byt null
 * @param paramCount pocet parametrov
 * @param returnType navratovy typ poskladaneho typu funkcie
 * @param outId Tu sa potom zapise vysledne id typu
 * @brief Funkcia porovna, ci uz dany typ existuje v "types", ak ano, do outId zapise kod tohto
 * typu z "types", cize TYPE_ID_BASE co je 100 + poradie riadku v ktorom
 * sa tento typ nachadza. Ak nie, vytvori nove typove id a zapise ho do outId.
 * @return 0 pri uspechu 99 pri neuspechu
 */
int FunctionType(const typeId *params, int paramCount, typeId returnType, typeId *outId) {
    if (paramCount < 0 || outId == NULL || (paramCount > 0 && params == NULL)) {
        return 99;
    }

    for (int i = 0; i < types.filledCount; i++) {
        if (types.functionTypes[i].paramCount != paramCount) {
            continue;
        }

        bool same = true;
        for (int j = 0; j < paramCount; j++) {
            if (types.functionTypes[i].idParams[j] != params[j]) {
                same = false;
                break;
            }
        }

        if (same && types.functionTypes[i].returnId == returnType) {
            *outId = TYPE_ID_BASE + i;
            return 0;
        }
    }

    if (types.filledCount == types.capacity) {
        if (!IncreaseTypeStoreSize(&types)) {
            return 99;
        }
    }

    typeId *copy = NULL;
    if (paramCount > 0) {
        copy = malloc((size_t)paramCount * sizeof(typeId));
        if (copy == NULL) {
            return 99;
        }
        memcpy(copy, params, (size_t)paramCount * sizeof(typeId));
    }

    types.functionTypes[types.filledCount].paramCount = paramCount;
    types.functionTypes[types.filledCount].idParams = copy;
    types.functionTypes[types.filledCount].returnId = returnType;

    *outId = TYPE_ID_BASE + types.filledCount;
    types.filledCount++;
    return 0;
}

/**
 * @param name meno build-in funkcie, taktiez sluzi ako kluc pri ukladani do symtable
 * @param params pole parametrov ktore funkcia berie, moze byt aj NULL alebo jeden alebo viacero
 * @param paramCount pocet paremetrov ktore funkcia berie, ak je params NULL paramCount je 0
 * @param returnType navratovy typ built-in funkcie
 * @return Vracia Int, 0 v pripade uspechu, 99 v pripade neuspechu
 * @brief Helper funkcia pre vkladanie built-in funkcii do root tabulky, tato funkcia sa pouziva iba
 * v SemAnalysisInit, nech SemAnalysisInit nie je priliz dlha
 */
static int InsertBuiltIn(const char *name, const typeId *params, int paramCount,
                         typeId returnType) {
    int success;
    typeId funType;
    success = FunctionType(params, paramCount, returnType, &funType);
    if (success == 99)
        return 99;
    symbolData data;
    data.isConst = false;
    data.isDefined = true;
    data.isFunction = true;
    data.type = funType;
    data.paramCount = paramCount;
    bool insertSuccess;
    insertSuccess = InsertFunction(rootTable, name, data);
    if (!insertSuccess)
        return 99;
    return 0;
}

/**
 * @return 0 v pripade uspechu, 99 v pripade neuspechu
 * @brief inicializuje semanticku analyzu, vytvori root uroven symtable, naplni ju built in
 * funkciami
 */
int SemAnalysisInit(void) {
    rootTable = InitTable(23);
    if (rootTable == NULL)
        return 99;
    int insertSuccess;
    // print funkcia
    typeId params = {TYPE_ANY};
    insertSuccess = InsertBuiltIn("print", &params, 1, TYPE_UNIT);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // readLine funkcia
    insertSuccess = InsertBuiltIn("readLine", NULL, 0, TYPE_STRING);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }

    // readInt funkcia
    insertSuccess = InsertBuiltIn("readInt", NULL, 0, TYPE_INT);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // readDouble funkcia
    insertSuccess = InsertBuiltIn("readDouble", NULL, 0, TYPE_DOUBLE);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // toStr funkcia
    insertSuccess = InsertBuiltIn("toStr", &params, 1, TYPE_STRING);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // toInt funkcia
    params = TYPE_DOUBLE;
    insertSuccess = InsertBuiltIn("toInt", &params, 1, TYPE_INT);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // strLength funkcia
    params = TYPE_STRING;
    insertSuccess = InsertBuiltIn("strLength", &params, 1, TYPE_INT);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // subStr funkcia
    typeId subStrParams[3] = {TYPE_STRING, TYPE_INT, TYPE_INT};
    insertSuccess = InsertBuiltIn("subStr", subStrParams, 3, TYPE_STRING);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // ord funkcia
    typeId ordParams[2] = {TYPE_STRING, TYPE_INT};
    insertSuccess = InsertBuiltIn("ord", ordParams, 2, TYPE_INT);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // chr funkcia
    typeId chrParams = {TYPE_INT};
    insertSuccess = InsertBuiltIn("chr", &chrParams, 1, TYPE_STRING);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }
    // readBoolean funkcia
    insertSuccess = InsertBuiltIn("readBoolean", NULL, 0, TYPE_BOOLEAN);
    if (insertSuccess == 99) {
        SemAnalysisFree();
        return 99;
    }

    return 0;
}

/**
 * @brief uvolni pamat na konci semantickej analyzi
 */
void SemAnalysisFree(void) {
    FreeAllTables(rootTable);
    rootTable = NULL;
    for (int i = 0; i < types.filledCount; i++) {
        free(types.functionTypes[i].idParams);
    }
    free(types.functionTypes);
    types.functionTypes = NULL;
    types.filledCount = 0;
    types.capacity = 0;
}

int FunctionHeader(const char *name, bool hasReturnType, typeId returnType,
                   const typeId *paramTypes, int paramCount, bool isMain) {
    (void)name;
    (void)hasReturnType;
    (void)returnType;
    (void)paramTypes;
    (void)paramCount;
    (void)isMain;
    return 0;
}

// zatial len stub funkcie, nech sa prelozi program a nech sa mozu vyuzivat v syntaktickej analyze
int PutFuncParamAside(const char *name, typeId type) {
    (void)name;
    (void)type;
    return 0;
}

int FuncBodyStart(void) { return 0; }

int FuncBodyEnd(void) { return 0; }

int EnterBlock(void) { return 0; }

int LeaveBlock(void) { return 0; }

int DeclareVar(const char *name, bool hasType, typeId type, bool isConst, typeId initType) {
    (void)name;
    (void)hasType;
    (void)type;
    (void)isConst;
    (void)initType;
    return 0;
}

int GetVarType(const char *name, typeId *outType) {
    (void)name;
    *outType = TYPE_UNDEFINED;
    return 0;
}

int Call(const char *name, int argCount, const typeId *argTypes, typeId *outType) {
    (void)name;
    (void)argCount;
    (void)argTypes;
    *outType = TYPE_UNDEFINED;
    return 0;
}

int AssignValueToVar(const char *name, typeId valueType) {
    (void)name;
    (void)valueType;
    return 0;
}

int Operator(typeId left, typeId right, int op, typeId *outType) {
    (void)left;
    (void)right;
    (void)op;
    *outType = TYPE_UNDEFINED;
    return 0;
}

int ValmemMember(typeId objType, const char *member, typeId *outType) {
    (void)objType;
    (void)member;
    *outType = TYPE_UNDEFINED;
    return 0;
}

int ConditionType(typeId conditionType) {
    (void)conditionType;
    return 0;
}

int Return(bool hasValue, typeId valueType) {
    (void)hasValue;
    (void)valueType;
    return 0;
}

int StatementValueType(typeId valueType) {
    (void)valueType;
    return 0;
}

int CheckMain(void) { return 0; }
