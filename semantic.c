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

#include "lexer/token.h"
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

typedef struct {
    char *name;
    typeId declaredReturn;
    bool hasExplicitReturn;
    bool isMain;
    bool sawReturn;
} currentFunction;

typedef struct {
    char *name;
    bool hasExplicitReturn;
} functionInfo;

static typeStore types;
static symTable *rootTable;
static bool mainAlreadyDefined;
static currentFunction currentFn;
static bool analysisInferMode;
static int bodyBlockDepth;
static int blockDepth;
static functionInfo *functionList;
static int functionCount;
static int functionCapacity;

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
 * @return Vracia 0 v pripade uspechu, 99 v pripade neuspechu
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
 * @param enabled
 * @brief funkcia na prepinamie modu semantickej analyzy medzi inferencnym modom a modom na ostro
 */
void SemInferMode(bool enabled) { analysisInferMode = enabled; }

/**
 * @brief prerusi telo v inferencnom rezime a zavrie vsetky jeho scopes
 */
int AbortFuncBody(void) {
    if (currentFn.name == NULL)
        return 0;
    int result = 0;
    while (blockDepth > bodyBlockDepth) {
        if (!PopScope(rootTable)) {
            result = 99;
            break;
        }
        blockDepth--;
    }
    if (result == 0 && !PopScope(rootTable))
        result = 99;
    free(currentFn.name);
    currentFn.name = NULL;
    currentFn.sawReturn = false;
    return result;
}

/**
 * @param name meno funkcie
 * @return ukazatel na zaznam alebo NULL
 * @brief najde zaznam funkcie v pomocnom zozname
 */
static functionInfo *FindFunctionInfo(const char *name) {
    if (name == NULL)
        return NULL;
    for (int i = 0; i < functionCount; i++) {
        if (strcmp(functionList[i].name, name) == 0)
            return &functionList[i];
    }
    return NULL;
}

/**
 * @param name meno funkcie
 * @param hasExplicitReturn ci mal typ v zdroji
 * @return 0 pri uspechu, 99 pri zlyhani alokacie
 * @brief zapise alebo aktualizuje zaznam funkcie v pomocnom zozname
 */
static int RegisterFunctionInfo(const char *name, bool hasExplicitReturn) {
    if (name == NULL)
        return 99;
    functionInfo *existing;
    existing = FindFunctionInfo(name);
    if (existing != NULL) {
        existing->hasExplicitReturn = hasExplicitReturn;
        return 0;
    }
    if (functionCount == functionCapacity) {
        int newCapacity = functionCapacity == 0 ? 16 : functionCapacity * 2;
        functionInfo *newList = realloc(functionList, (size_t)newCapacity * sizeof(functionInfo));
        if (newList == NULL)
            return 99;
        functionList = newList;
        functionCapacity = newCapacity;
    }
    size_t length = strlen(name);
    char *copy = malloc(length + 1);
    if (copy == NULL)
        return 99;
    memcpy(copy, name, length + 1);
    functionList[functionCount].name = copy;
    functionList[functionCount].hasExplicitReturn = hasExplicitReturn;
    functionCount++;
    return 0;
}

/**
 * @param name meno funkcie
 * @return true ak je navratovy typ funkcie uz znamy
 * @brief zisti, ci ma funkcia uz odvodeny alebo uvedeny navratovy typ
 */
bool FunctionHasType(const char *name) {
    if (name == NULL || rootTable == NULL)
        return false;
    symbolData data;
    if (!LookupFunction(rootTable, name, &data))
        return false;
    int index = data.type - TYPE_ID_BASE;
    if (index < 0 || index >= types.filledCount)
        return false;
    return types.functionTypes[index].returnId != TYPE_UNDEFINED;
}

/**
 * @return pocet funkcii so znamym navratovym typom
 * @brief pouziva sa na zistenie pokroku medzi inferencnymi prechodmi
 */
int DefinedFunctionCount(void) {
    int count = 0;
    for (int i = 0; i < functionCount; i++) {
        if (FunctionHasType(functionList[i].name))
            count++;
    }
    return count;
}

/**
 * @return 0 v pripade uspechu
 * @return 99 v pripade internej chyby
 * @brief zresetuje semanticku analyzu a pripravi ju na poriadny prechod
 */
int ResetSemAnalysisForFinalAnalysis(void) {
    SemInferMode(false);
    if (rootTable == NULL)
        return 0;
    if (currentFn.name != NULL) {
        free(currentFn.name);
        currentFn.name = NULL;
        currentFn.sawReturn = false;
    }
    while (!IsRootScope(rootTable)) {
        if (!PopScope(rootTable))
            return 99;
    }
    blockDepth = 0;
    bodyBlockDepth = 0;

    for (int i = 0; i < functionCount; i++) {
        symbolData data;
        if (!LookupFunction(rootTable, functionList[i].name, &data))
            return 99;
        if (!data.isDefined)
            continue;
        data.isDefined = false;
        if (!DeleteItem(rootTable, functionList[i].name))
            return 99;
        if (!InsertFunction(rootTable, functionList[i].name, data))
            return 99;
    }
    return 0;
}

/**
 * @param has parameter ktory mame
 * @param expected parameter s ktorym porovnavame
 * @return true ak su typovo kompatibilne
 * @return false ak su typovo nekompatibilne
 * @brief Helper funkcia na zistenie kompatibility dvoch typov
 */
static bool CheckTypeCompatibility(typeId has, typeId expected) {
    if (has == TYPE_UNDEFINED || expected == TYPE_UNDEFINED)
        return false;
    if (has == expected)
        return true;
    if (expected == TYPE_ANY)
        return true;
    if (has == TYPE_INT && expected == TYPE_DOUBLE)
        return true;
    return false;
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
    mainAlreadyDefined = false;
    blockDepth = 0;
    bodyBlockDepth = 0;
    return 0;
}

/**
 * @brief uvolni pamat na konci semantickej analyzi
 */
void SemAnalysisFree(void) {
    FreeAllTables(rootTable);
    rootTable = NULL;
    free(currentFn.name);
    currentFn.name = NULL;
    for (int i = 0; i < types.filledCount; i++) {
        free(types.functionTypes[i].idParams);
    }
    free(types.functionTypes);
    types.functionTypes = NULL;
    types.filledCount = 0;
    types.capacity = 0;
    SemInferMode(false);
    blockDepth = 0;
    bodyBlockDepth = 0;

    for (int i = 0; i < functionCount; i++)
        free(functionList[i].name);
    free(functionList);
    functionList = NULL;
    functionCount = 0;
    functionCapacity = 0;
}

/**
 * @return 0 pri uspechu, 99 pri neuspechu
 * @brief Vytvori novy scope pre blok
 */
int EnterBlock(void) {
    bool success;
    success = PushScope(rootTable);
    if (!success)
        return 99;
    blockDepth += 1;
    return 0;
}

/**
 * @return 0 pri uspechu 99 pri neuspechu
 * @brief Zrusi vrchny scope na konci bloku
 */
int LeaveBlock(void) {
    bool success;
    success = PopScope(rootTable);
    if (!success)
        return 99;
    if (blockDepth > 0)
        blockDepth -= 1;
    return 0;
}

/**
 * @param name nazov premennej
 * @param hasType ci uzivatel zadal typ alebo nie
 * @param type typ ktory zadefinoval uzivatel pre premennu
 * @param isConst ci je premenna zadefinovana ako konstanta alebo nie
 * @param initType typ, ktory ma hodnota na pravej strane vyrazu
 * @return 99 v pripade neklasifikovanej internej chyby, v tomto pripade ak uzivatel nezada meno
 * premennej alebo sa premenna pushuje to root tabulky, do ktorej patria iba funkcie
 * @return 6 ak su type a initType nekompatibilne alebo ked initType sa neda zistit
 * @return 4 v pripade ze bola premenna zadefinovana viackrat v tom istom bloku
 * @return 0 v pripade uspechu
 * @brief Funkcia pre deklaraciu premennej, urobi overenia ci bolo zadane meno a nepokusame
 * pushnut do root tabulky, ci su kompatibilne datove typy, ak je vsetko v poriadku, zapise ju do
 * scope tabulky.
 */
int DeclareVar(const char *name, bool hasType, typeId type, bool isConst, typeId initType) {
    if (name == NULL)
        return 99;
    if (IsRootScope(rootTable))
        return 99;
    if (hasType) {
        if (!CheckTypeCompatibility(initType, type))
            return 6;
    } else {
        if (initType == TYPE_UNDEFINED)
            return 6;
        type = initType;
    }
    symbolData data;
    data.isConst = isConst;
    data.isDefined = true;
    data.isFunction = false;
    data.paramCount = 0;
    data.type = type;
    bool insertSuccess;
    insertSuccess = InsertLocal(rootTable, name, data);
    if (!insertSuccess)
        return 4;

    return 0;
}

/**
 * @param name meno premennej/funkcie ktoru hladame
 * @param outType tu sa zapise typ hladanej premennej / funkcie
 * @return 99 v pripade internej chyby
 * @return 3 v pripade ze sa nenasla ani funkcia ani premenna s tymto menom
 * @return 0 v pripade uspechu
 * @brief Funkcia ktora ziska typ premennej alebo funkcie, pokial existuje
 */
int GetVarType(const char *name, typeId *outType) {
    if (name == NULL)
        return 99;
    if (outType == NULL)
        return 99;
    symbolData data;
    bool found;
    found = LookupVar(rootTable, name, &data);
    if (!found) {
        found = LookupFunction(rootTable, name, &data);
    }
    if (!found) {
        return 3;
    }
    if (data.type == TYPE_UNDEFINED)
        return 99;
    *outType = data.type;

    return 0;
}

/**
 * @param name meno funkcie
 * @param hasReturnType ci ma funkcia navratovy typ alebo nie
 * @param returnType aky navratovy typ ma funkcia, ak ho ma, ak nie, zatial sa nastavi na
 * TYPE_UNDEFINED a neskor sa zmeni podla posledneho riadku v tele funkcii
 * @param paramTypes typy parammetrov funkcie
 * @param paramCount kolko parametrov ma funkcia
 * @param isMain ci je funkcia main alebo nie
 * @return 99 v pripade internej chyby
 * @return 5 v pripade ze isMain je true a funkcia obsahuje parametre
 * @return 6 ak je funkcia main a ma uvedeny navratovy typ a jej navratovy typ nie je typu UNIT
 * @return 4 ak je funkcia main, ale main funkcia uz bola definovana skor alebo ked deklarujeme
 * funkciu ktora uz existuje
 * @return 0 v pripade uspechu
 * @brief Funkcia ktora zapise hlavicku deklarovanej funkcie pred tym, ako sa zacne parsovat telo.
 * Toto tak musi byt preto ze specifikacia hovori ze funkcia moze byt volana aj pred tym ako je
 * deklarovana.
 */
int FunctionHeader(const char *name, bool hasReturnType, typeId returnType,
                   const typeId *paramTypes, int paramCount, bool isMain) {
    if (name == NULL)
        return 99;
    if (isMain && paramCount > 0)
        return 5;
    if (isMain && hasReturnType && returnType != TYPE_UNIT)
        return 6;
    if (isMain && mainAlreadyDefined)
        return 4;

    if (paramCount < 0 || (paramCount > 0 && paramTypes == NULL))
        return 99;
    if (!hasReturnType) {
        returnType = TYPE_UNDEFINED;
    }
    typeId funType;
    int success;
    success = FunctionType(paramTypes, paramCount, returnType, &funType);
    if (success == 99)
        return 99;
    symbolData data;
    data.isConst = false;
    data.isDefined = false;
    data.isFunction = true;
    data.paramCount = paramCount;
    data.type = funType;
    bool insertSuccess;
    insertSuccess = InsertFunction(rootTable, name, data);
    if (!insertSuccess)
        return 4;
    if (RegisterFunctionInfo(name, hasReturnType) != 0)
        return 99;
    if (isMain) {
        mainAlreadyDefined = true;
    }
    return 0;
}

/**
 * @param name Nazov premennej
 * @param valueType datovy typ hodnoty ktoru chceme priradit premennej
 * @return 99 v pripade internej chyby
 * @return 6 v pripade nekompatibility typov alebo ked je UNDEFINED typ
 * @return 7 ak sa snazime priradit hodnotu funkcii alebo ked sa snazime zmenit hodnotu konstantnej
 * premennej
 * @return 3 ak ani ani premenna ani funkcia s tymto menom neexistuje
 * @return 0 v pripade uspechu
 * @brief Funkcia ktora kontroluje podmienky pre priradovanie hodnoty ku premennej.
 */
int AssignValueToVar(const char *name, typeId valueType) {
    if (name == NULL)
        return 99;
    if (valueType == TYPE_UNDEFINED)
        return 6;
    bool found;
    symbolData data;
    found = LookupVar(rootTable, name, &data);
    if (!found) {
        found = LookupFunction(rootTable, name, &data);
        if (found)
            return 7;
    }
    if (!found)
        return 3;
    if (data.isConst)
        return 7;
    if (!CheckTypeCompatibility(valueType, data.type))
        return 6;
    return 0;
}

/**
 * @param funType id funkcneho typu v zozname types
 * @param argCount pocet argumentov volania
 * @param argTypes pole typov argumentov volania
 * @param outReturnType sem sa zapise navratovy typ
 * @return 99 pri NULL outReturnType alebo ked id neukazuje do zoznamu types
 * @return 5 ked pocet alebo typ argumentov nesedi so signaturou
 * @return 6 ked ma niektory argument typ TYPE_UNDEFINED alebo ked je returnId
 * TYPE_UNDEFINED
 * @return 0 pri uspechu
 * @brief overi argumenty volania voci signature a vrati jej navratovy typ
 */
static int GetReturnType(const typeId funType, int argCount, const typeId *argTypes,
                         typeId *outReturnType) {
    if (outReturnType == NULL)
        return 99;
    int index = funType - TYPE_ID_BASE;
    if (index < 0 || index >= types.filledCount)
        return 99;
    funcType *signature = &types.functionTypes[index];
    if (argCount != signature->paramCount)
        return 5;
    for (int i = 0; i < argCount; i++) {
        if (argTypes[i] == TYPE_UNDEFINED)
            return 6;
        if (!CheckTypeCompatibility(argTypes[i], signature->idParams[i]))
            return 5;
    }
    if (signature->returnId == TYPE_UNDEFINED)
        return analysisInferMode ? NOT_YET_KNOWN_RETURN_TYPE : 6;
    *outReturnType = signature->returnId;
    return 0;
}

/**
 * @param name meno funkcie alebo premennej
 * @param argCount pocet argumentov
 * @param argTypes pole typov argumentov
 * @param outType sem sa zapise navratovy typ
 * @return 99 pri NULL name alebo outType, zapornom argCount, NULL argTypes pri nenulovom pocte
 * alebo neplatnom id signatury
 * @return 3 ked meno neexistuje ani ako premenna ani ako funkcia
 * @return 5 ked pocet alebo typ argumentov nesedi so signaturou
 * @return 6 ked najdena premenna nie je funkcia, ked ma argument typ TYPE_UNDEFINED alebo ked sa
 * neda odvodit navratovy typ
 * @return 0 pri uspechu
 * @brief overi volanie funkcie alebo funkcie ulozenej v premennej a vrati jeho navratovy typ
 */
int Call(const char *name, int argCount, const typeId *argTypes, typeId *outType) {
    if (name == NULL)
        return 99;
    if (argCount > 0 && argTypes == NULL)
        return 99;
    if (argCount < 0)
        return 99;
    if (outType == NULL)
        return 99;
    symbolData data;
    bool success;
    success = LookupVar(rootTable, name, &data);
    if (!success) {
        success = LookupFunction(rootTable, name, &data);
        if (!success)
            return 3;
    }
    if (data.type < TYPE_ID_BASE)
        return 6;
    typeId returnType;
    int result = GetReturnType(data.type, argCount, argTypes, &returnType);
    if (result != 0)
        return result;
    *outType = returnType;

    return 0;
}

/**
 * @param name meno funkcie, ktorej telo sa zacina
 * @param paramNames mena parametrov
 * @param paramTypes typy parametrov
 * @param paramCount pocet parametrov
 * @param isMain ci ide o hlavnu funkciu
 * @return 99 pri internej chybe
 * @return 4 pri duplikate parametra
 * @return 0 pri uspechu
 * @brief vytvori scope tela funkcie, vlozi parametre ako konstanty a nastavi currentFn
 */
int FuncBodyStart(const char *name, const char **paramNames, const typeId *paramTypes,
                  int paramCount, bool isMain) {
    if (name == NULL || currentFn.name != NULL)
        return 99;
    if (paramCount < 0)
        return 99;
    if (paramCount > 0 && (paramNames == NULL || paramTypes == NULL))
        return 99;

    symbolData functionData;
    if (!LookupFunction(rootTable, name, &functionData))
        return 99;
    if (functionData.isDefined && !analysisInferMode)
        return 99;
    int index = functionData.type - TYPE_ID_BASE;
    if (index < 0 || index >= types.filledCount)
        return 99;
    funcType *signature = &types.functionTypes[index];
    if (paramCount != signature->paramCount)
        return 99;
    functionInfo *info = FindFunctionInfo(name);
    if (info == NULL)
        return 99;
    if (!PushScope(rootTable))
        return 99;

    symbolData paramData;
    paramData.isConst = true;
    paramData.isDefined = true;
    paramData.isFunction = false;
    paramData.paramCount = 0;
    for (int i = 0; i < paramCount; i++) {
        if (paramNames[i] == NULL || paramTypes[i] == TYPE_UNDEFINED ||
            paramTypes[i] != signature->idParams[i]) {
            PopScope(rootTable);
            return 99;
        }
        paramData.type = paramTypes[i];
        if (!InsertLocal(rootTable, paramNames[i], paramData)) {
            PopScope(rootTable);
            return 4;
        }
    }

    size_t nameLength = strlen(name);
    char *nameCopy = malloc(nameLength + 1);
    if (nameCopy == NULL) {
        PopScope(rootTable);
        return 99;
    }
    memcpy(nameCopy, name, nameLength + 1);

    currentFn.name = nameCopy;
    currentFn.declaredReturn = signature->returnId;
    currentFn.hasExplicitReturn = info->hasExplicitReturn;
    currentFn.isMain = isMain;
    currentFn.sawReturn = false;
    bodyBlockDepth = blockDepth;
    return 0;
}

/**
 * @param name meno funkcie v root tabulke
 * @return 0 pri uspechu
 * @return 99 pri internej chybe
 * @brief oznaci existujucu funkciu za definovanu
 */
static int MarkFunctionDefined(const char *name) {
    symbolData data;
    if (!LookupFunction(rootTable, name, &data))
        return 99;
    data.isDefined = true;
    if (!DeleteItem(rootTable, name))
        return 99;
    if (!InsertFunction(rootTable, name, data))
        return 99;
    return 0;
}

/**
 * @param name meno funkcie v root tabulke
 * @param returnType odvodeny navratovy typ
 * @return 0 pri uspechu
 * @return 99 pri internej chybe
 * @brief upravi navratovy typ funkcie
 */
static int UpdateFunctionReturnType(const char *name, typeId returnType) {
    symbolData data;
    if (!LookupFunction(rootTable, name, &data))
        return 99;
    int index = data.type - TYPE_ID_BASE;
    if (index < 0 || index >= types.filledCount)
        return 99;
    funcType *signature = &types.functionTypes[index];
    typeId newType;
    if (FunctionType(signature->idParams, signature->paramCount, returnType, &newType) != 0)
        return 99;
    data.type = newType;
    data.isDefined = true;
    if (!DeleteItem(rootTable, name))
        return 99;
    if (!InsertFunction(rootTable, name, data))
        return 99;
    return 0;
}
/**
 * @param hasResult ci telo funkcie vratilo hodnotu
 * @param resultType typ vysledku tela, pouziva sa iba ked hasResult je true
 * @return 99 pri internej chybe
 * @return 6 pri chybajucom alebo nekompatibilnom vysledku
 * @return 0 pri uspechu
 * @brief uzavrie scope tela, skontroluje vysledok a oznaci funkciu za definovanu
 */
int FuncBodyEnd(bool hasResult, typeId resultType) {
    if (currentFn.name == NULL)
        return 99;

    char *name = currentFn.name;
    typeId declaredReturn = currentFn.declaredReturn;
    bool hasExplicitReturn = currentFn.hasExplicitReturn;
    bool isMain = currentFn.isMain;
    bool sawReturn = currentFn.sawReturn;

    currentFn.name = NULL;
    if (!PopScope(rootTable)) {
        free(name);
        return 99;
    }

    int result = 0;
    if (hasResult && resultType == TYPE_UNDEFINED) {
        result = 6;
    } else if (!hasExplicitReturn) {
        if (sawReturn) {
            result = 6;
        } else if (hasResult) {
            if (isMain && resultType != TYPE_UNIT)
                result = 6;
            else
                result = UpdateFunctionReturnType(name, resultType);
        } else {
            result = UpdateFunctionReturnType(name, TYPE_UNIT);
        }
    } else if (declaredReturn == TYPE_UNIT) {
        result = MarkFunctionDefined(name);
    } else if (hasResult) {
        if (CheckTypeCompatibility(resultType, declaredReturn))
            result = MarkFunctionDefined(name);
        else
            result = 6;
    } else if (sawReturn) {
        result = MarkFunctionDefined(name);
    } else {
        result = 6;
    }

    free(name);
    return result;
}

/**
 * @param type kontrolovany typ
 * @return true ak je typ int alebo double, inak false
 * @brief zisti ci je typ numericky, pomocna funkcia pre overenie ze pri operaciach ktore beru len
 * numericke hodnoty su hodnoty naozaj numericke
 */
static bool IsNumeric(typeId type) { return type == TYPE_INT || type == TYPE_DOUBLE; }

/**
 * @param left typ laveho operandu
 * @param right typ praveho operandu
 * @return Double ak je aspon jeden operand Double, inak Int
 * @brief urci vysledny typ aritmetickej operacie, tiez pomocna funkcia
 */
static typeId NumericResult(typeId left, typeId right) {
    if (left == TYPE_DOUBLE || right == TYPE_DOUBLE)
        return TYPE_DOUBLE;
    return TYPE_INT;
}

/**
 * @param left typ laveho operanda
 * @param right typ praveho operanda, TYPE_UNDEFINED pri unarnom operatore
 * @param op token operatoru z lexeru
 * @param outType typ vysledku
 * @return 99 pri NULL outType, neznamom operatore alebo chybnom unarnom volani
 * @return 6 pri operandoch, ktore operator nepodporuje
 * @return 0 pri uspechu
 * @brief urci typ vysledku operacie laveho a praveho operanda
 */
int Operator(typeId left, typeId right, int op, typeId *outType) {
    if (outType == NULL)
        return 99;
    *outType = TYPE_UNDEFINED;
    if (left == TYPE_UNDEFINED)
        return 6;

    switch (op) {
    case TOKEN_NOT:
        if (right != TYPE_UNDEFINED)
            return 99;
        if (left != TYPE_BOOLEAN)
            return 6;
        *outType = TYPE_BOOLEAN;
        return 0;
    case TOKEN_MINUS:
    case TOKEN_PLUS:
        if (right == TYPE_UNDEFINED) {
            if (!IsNumeric(left))
                return 6;
            *outType = left;
            return 0;
        }
        break;
    case TOKEN_MUL:
    case TOKEN_DIV:
    case TOKEN_LT:
    case TOKEN_LTE:
    case TOKEN_GT:
    case TOKEN_GTE:
    case TOKEN_EQ:
    case TOKEN_NEQ:
    case TOKEN_AND:
    case TOKEN_OR:
        if (right == TYPE_UNDEFINED)
            return 6;
        break;
    default:
        return 99;
    }

    if (left == TYPE_ANY || right == TYPE_ANY)
        return 6;

    switch (op) {
    case TOKEN_PLUS:
        if (left == TYPE_STRING && right == TYPE_STRING) {
            *outType = TYPE_STRING;
            return 0;
        }
        if (IsNumeric(left) && IsNumeric(right)) {
            *outType = NumericResult(left, right);
            return 0;
        }
        return 6;
    case TOKEN_MINUS:
    case TOKEN_MUL:
    case TOKEN_DIV:
        if (!IsNumeric(left) || !IsNumeric(right))
            return 6;
        *outType = NumericResult(left, right);
        return 0;
    case TOKEN_LT:
    case TOKEN_LTE:
    case TOKEN_GT:
    case TOKEN_GTE:
        if ((IsNumeric(left) && IsNumeric(right)) ||
            (left == TYPE_STRING && right == TYPE_STRING)) {
            *outType = TYPE_BOOLEAN;
            return 0;
        }
        return 6;
    case TOKEN_EQ:
    case TOKEN_NEQ:
        if (left == right || (IsNumeric(left) && IsNumeric(right))) {
            *outType = TYPE_BOOLEAN;
            return 0;
        }
        return 6;
    case TOKEN_AND:
    case TOKEN_OR:
        if (left != TYPE_BOOLEAN || right != TYPE_BOOLEAN)
            return 6;
        *outType = TYPE_BOOLEAN;
        return 0;
    default:
        return 99;
    }
}

/**
 * @param objType typ pred bodkou
 * @param member meno clena valmem
 * @param outType tu sa zapisuje vysledok
 * @return 99 ked je member NULL alebo outtype je null
 * @return 6 pri neexistujucom valmem clene alebo nepodporovanom type pred bodkou
 * @return 0 pri uspechu
 * @brief overuje spravne pouzitie valmem rozsirenia, overi valmem clena a vrati jeho typ
 */
int ValmemMember(typeId objType, const char *member, typeId *outType) {
    if (outType == NULL)
        return 99;
    *outType = TYPE_UNDEFINED;
    if (member == NULL)
        return 99;
    if (objType == TYPE_UNDEFINED || objType == TYPE_ANY)
        return 6;

    if (strcmp(member, "toString") == 0) {
        if (objType < TYPE_INT || objType > TYPE_UNIT)
            return 6;
        *outType = TYPE_STRING;
        return 0;
    }
    if (strcmp(member, "toInt") == 0) {
        if (objType != TYPE_DOUBLE)
            return 6;
        *outType = TYPE_INT;
        return 0;
    }
    if (strcmp(member, "floor") == 0) {
        if (objType != TYPE_DOUBLE)
            return 6;
        *outType = TYPE_DOUBLE;
        return 0;
    }
    if (strcmp(member, "length") == 0) {
        if (objType != TYPE_STRING)
            return 6;
        *outType = TYPE_INT;
        return 0;
    }
    return 6;
}

/**
 * @param hasValue ci ma return nejaku hodnotu
 * @param valueType typ hodnoty returnu
 * @return 99 pri internej chybe
 * @return 10 ked return bez hodnoty nie je vo funkcii s TYPE_UNIT
 * @return 6 ked je typ UNDEFINED, nema explicitne return a return nie je typovo kompatibilny s
 * typom value
 * @return 0 v pripade uspechu
 * @brief Overenie zvysku pravidiel pre return, ktore este neboli pokryte v FuncBodyEnd()
 */
int Return(bool hasValue, typeId valueType) {
    if (currentFn.name == NULL)
        return 99;
    currentFn.sawReturn = true;
    if (!hasValue) {
        if (currentFn.hasExplicitReturn && currentFn.declaredReturn == TYPE_UNIT)
            return 0;
        return 10;
    }
    if (valueType == TYPE_UNDEFINED)
        return 6;
    if (!currentFn.hasExplicitReturn)
        return 6;
    if (currentFn.declaredReturn == TYPE_UNIT)
        return 0;
    if (!CheckTypeCompatibility(valueType, currentFn.declaredReturn))
        return 6;

    return 0;
}

/**
 * @param conditionType datovy typ podmienky
 * @return 6 ked je typ iny ako BOOLEAN
 * @return 0 ked je typ boolean
 * @brief Funkcia na kontrolu, ci je podmienka naozaj typu boolean, ineho typu nesmie byt
 */
int ConditionType(typeId conditionType) {
    if (conditionType == TYPE_BOOLEAN)
        return 0;
    else
        return 6;
}

/**
 * @param valueType typ vyrazu ktory je ako samostatny prikaz
 * @return 99 ked je typ UNDEFINED
 * @return 0 ked ma vyraz akykolvek typ okrem UNDEFINED
 * @brief overuje ze vyraz v pozicii prikazu ma znamy typ
 */
int StatementValueType(typeId valueType) {
    if (valueType == TYPE_UNDEFINED)
        return 99;
    return 0;
}

/**
 * @return 3 ked este nebol zadefinovany main
 * @return 0 ak uz bol zadefinovany main
 * @brief overuje ci program obsahuje definiciu main
 */
int CheckMain(void) { return mainAlreadyDefined ? 0 : 3; }

int GetFunctionSignature(const char *name, typeId *typeArr, const int arrSize, int *capacity,
                         typeId *returnType) {
    if (name == NULL || capacity == NULL || returnType == NULL)
        return 99;
    symbolData data;
    if (!(LookupVar(rootTable, name, &data))) {
        if (!(LookupFunction(rootTable, name, &data))) {
            return 3;
        }
    }
    if (data.type < TYPE_ID_BASE)
        return 6;
    if (data.type >= TYPE_ID_BASE + types.filledCount)
        return 99;
    int index = data.type - TYPE_ID_BASE;
    funcType *signature = &types.functionTypes[index];
    if (signature->paramCount > arrSize)
        return 99;
    if (signature->paramCount > 0 && typeArr == NULL)
        return 99;
    *capacity = signature->paramCount;
    typeId retType = signature->returnId;

    for (int i = 0; i < signature->paramCount; i++) {
        typeArr[i] = signature->idParams[i];
    }

    switch (retType) {
    case 0:
        return 6;
    case 1:
        *returnType = TYPE_INT;
        break;
    case 2:
        *returnType = TYPE_DOUBLE;
        break;
    case 3:
        *returnType = TYPE_STRING;
        break;
    case 4:
        *returnType = TYPE_BOOLEAN;
        break;
    case 5:
        *returnType = TYPE_UNIT;
        break;
    case 6:
        *returnType = TYPE_ANY;
        break;
    default:
        *returnType = retType;
        break;
    }
    return 0;
}
