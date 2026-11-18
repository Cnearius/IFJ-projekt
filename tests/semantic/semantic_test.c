// Testy semantiky vygenerovane AI (Deepseek).
// Prompt: "vytvor testy pre moju semanticku analyzu, snaz sa osetrit co najviac edgecases a urobit testy co najpodrobnejsie ako sa ti daf"

/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file semantic_test.c
 * @author Michal Caban (xcabanm00)
 * @brief API-only testy semantickej analyzy pre semantic.c
 */

#include <stdio.h>
#include "../../semantic.h"
#include "../../types.h"
#include "../../lexer/token.h"

static int checks = 0;
static int failures = 0;
static int sec_checks = 0;
static int sec_failures = 0;

static const char *typeName(typeId type) {
    switch (type) {
        case TYPE_UNDEFINED: return "Undefined";
        case TYPE_INT: return "Int";
        case TYPE_DOUBLE: return "Double";
        case TYPE_STRING: return "String";
        case TYPE_BOOLEAN: return "Boolean";
        case TYPE_UNIT: return "Unit";
        case TYPE_ANY: return "Any";
        default: return "FunctionType";
    }
}

static void check(const char *what, int got, int expected) {
    checks++;
    if (got != expected) {
        printf("  FAIL %-58s got %d, expected %d\n", what, got, expected);
        failures++;
    }
}

static void checkType(const char *what, typeId got, typeId expected) {
    checks++;
    if (got != expected) {
        printf("  FAIL %-58s type %s, expected %s\n", what, typeName(got),
               typeName(expected));
        failures++;
    }
}

static void beginSection(const char *name) {
    printf("\n== %s ==\n", name);
    sec_checks = checks;
    sec_failures = failures;
}

static void endSection(void) {
    int passed = (checks - sec_checks) - (failures - sec_failures);
    printf("   %d/%d\n", passed, checks - sec_checks);
}

static void fresh(void) {
    SemAnalysisFree();
    check("SemAnalysisInit", SemAnalysisInit(), 0);
}

static void checkCall(const char *what, const char *name, int argc, const typeId *args,
                      int expectedCode, typeId expectedType) {
    typeId out = 12345;
    int got = Call(name, argc, args, &out);
    check(what, got, expectedCode);
    if (got == 0 && expectedCode == 0)
        checkType(what, out, expectedType);
}

static void checkVar(const char *what, const char *name, int expectedCode,
                     typeId expectedType) {
    typeId out = 12345;
    int got = GetVarType(name, &out);
    check(what, got, expectedCode);
    if (got == 0 && expectedCode == 0)
        checkType(what, out, expectedType);
}

static void checkOp(const char *what, typeId left, typeId right, int op,
                    int expectedCode, typeId expectedType) {
    typeId out = 12345;
    int got = Operator(left, right, op, &out);
    check(what, got, expectedCode);
    if (got == 0 && expectedCode == 0)
        checkType(what, out, expectedType);
}

static void checkMember(const char *what, typeId objType, const char *member,
                        int expectedCode, typeId expectedType) {
    typeId out = 12345;
    int got = ValmemMember(objType, member, &out);
    check(what, got, expectedCode);
    if (got == 0 && expectedCode == 0)
        checkType(what, out, expectedType);
}

static void test_lifecycle_and_builtins(void) {
    beginSection("1. lifecycle a built-iny");
    SemAnalysisFree();
    SemAnalysisFree();
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    typeId oneDouble[1] = {TYPE_DOUBLE};
    typeId oneString[1] = {TYPE_STRING};
    typeId oneBoolean[1] = {TYPE_BOOLEAN};
    typeId oneUnit[1] = {TYPE_UNIT};
    typeId subStrArgs[3] = {TYPE_STRING, TYPE_INT, TYPE_INT};
    typeId ordArgs[2] = {TYPE_STRING, TYPE_INT};

    checkCall("print(Int)", "print", 1, oneInt, 0, TYPE_UNIT);
    checkCall("print(String)", "print", 1, oneString, 0, TYPE_UNIT);
    checkCall("print(Boolean)", "print", 1, oneBoolean, 0, TYPE_UNIT);
    checkCall("print(Unit)", "print", 1, oneUnit, 0, TYPE_UNIT);
    checkCall("print()", "print", 0, NULL, 5, TYPE_UNDEFINED);
    checkCall("readLine()", "readLine", 0, NULL, 0, TYPE_STRING);
    checkCall("readLine(Int)", "readLine", 1, oneInt, 5, TYPE_UNDEFINED);
    checkCall("readInt()", "readInt", 0, NULL, 0, TYPE_INT);
    checkCall("readInt(Int)", "readInt", 1, oneInt, 5, TYPE_UNDEFINED);
    checkCall("readDouble()", "readDouble", 0, NULL, 0, TYPE_DOUBLE);
    checkCall("readBoolean()", "readBoolean", 0, NULL, 0, TYPE_BOOLEAN);
    checkCall("toStr(Int)", "toStr", 1, oneInt, 0, TYPE_STRING);
    checkCall("toStr(Unit)", "toStr", 1, oneUnit, 0, TYPE_STRING);
    checkCall("toStr()", "toStr", 0, NULL, 5, TYPE_UNDEFINED);
    checkCall("toInt(Double)", "toInt", 1, oneDouble, 0, TYPE_INT);
    checkCall("toInt(Int) implicit", "toInt", 1, oneInt, 0, TYPE_INT);
    checkCall("toInt(String)", "toInt", 1, oneString, 5, TYPE_UNDEFINED);
    checkCall("strLength(String)", "strLength", 1, oneString, 0, TYPE_INT);
    checkCall("strLength(Int)", "strLength", 1, oneInt, 5, TYPE_UNDEFINED);
    checkCall("subStr(String,Int,Int)", "subStr", 3, subStrArgs, 0, TYPE_STRING);
    checkCall("subStr(String,Int)", "subStr", 2, subStrArgs, 5, TYPE_UNDEFINED);
    checkCall("ord(String,Int)", "ord", 2, ordArgs, 0, TYPE_INT);
    checkCall("ord(String)", "ord", 1, ordArgs, 5, TYPE_UNDEFINED);
    checkCall("chr(Int)", "chr", 1, oneInt, 0, TYPE_STRING);
    checkCall("chr(Double)", "chr", 1, oneDouble, 5, TYPE_UNDEFINED);
    checkCall("neznama funkcia", "neexistuje", 0, NULL, 3, TYPE_UNDEFINED);
    checkCall("Call NULL name", NULL, 0, NULL, 99, TYPE_UNDEFINED);
    checkCall("Call zaporny pocet", "print", -1, NULL, 99, TYPE_UNDEFINED);
    checkCall("Call NULL args", "print", 1, NULL, 99, TYPE_UNDEFINED);

    typeId out = 12345;
    check("Call NULL outType", Call("print", 1, oneInt, NULL), 99);
    check("Call s Undefined argumentom", Call("print", 1, (typeId[]){TYPE_UNDEFINED}, &out), 6);

    endSection();
}

static void test_function_type(void) {
    beginSection("2. FunctionType a interning");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    typeId twoIntDouble[2] = {TYPE_INT, TYPE_DOUBLE};
    typeId twoDoubleInt[2] = {TYPE_DOUBLE, TYPE_INT};

    typeId id1 = 0, id2 = 0, id3 = 0, id4 = 0, id5 = 0, id6 = 0, id7 = 0;
    check("FunctionType (Int)->Int", FunctionType(oneInt, 1, TYPE_INT, &id1), 0);
    check("id je >= 100", id1 >= 100, 1);
    check("FunctionType rovnaky", FunctionType(oneInt, 1, TYPE_INT, &id2), 0);
    check("interning vrati rovnake id", id2 == id1, 1);
    check("FunctionType (Int,Double)->String",
          FunctionType(twoIntDouble, 2, TYPE_STRING, &id3), 0);
    check("ine parametre = ine id", id3 != id1, 1);
    check("FunctionType (Double,Int)->String",
          FunctionType(twoDoubleInt, 2, TYPE_STRING, &id4), 0);
    check("poradie parametrov meni id", id4 != id3, 1);
    check("FunctionType (Int)->Double", FunctionType(oneInt, 1, TYPE_DOUBLE, &id5), 0);
    check("iny navrat = ine id", id5 != id1, 1);
    check("FunctionType ()->Int", FunctionType(NULL, 0, TYPE_INT, &id6), 0);
    check("FunctionType s funkcnym parametrom",
          FunctionType(&id1, 1, TYPE_INT, &id7), 0);
    check("funkcny parameter ma id >= 100", id7 >= 100, 1);

    typeId dummy;
    check("FunctionType NULL params s poctom",
          FunctionType(NULL, 1, TYPE_INT, &dummy), 99);
    check("FunctionType zaporny pocet",
          FunctionType(oneInt, -1, TYPE_INT, &dummy), 99);
    check("FunctionType NULL outId",
          FunctionType(oneInt, 1, TYPE_INT, NULL), 99);
    check("FunctionType 0 parametrov s NULL",
          FunctionType(NULL, 0, TYPE_INT, &dummy), 0);

    typeId prev = 0;
    check("FunctionType zaklad pre stress", FunctionType(NULL, 0, TYPE_INT, &prev), 0);
    int growthOk = 1;
    for (int i = 0; i < 300; i++) {
        typeId next = 0;
        if (FunctionType(&prev, 1, TYPE_INT, &next) != 0) {
            growthOk = 0;
            break;
        }
        prev = next;
    }
    check("300 unikatnych signatur a rast store", growthOk, 1);

    endSection();
}

static void test_function_header(void) {
    beginSection("3. FunctionHeader");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    check("Header f(Int): Int", FunctionHeader("f", true, TYPE_INT, oneInt, 1, false), 0);
    check("duplicitny f", FunctionHeader("f", true, TYPE_INT, oneInt, 1, false), 4);
    check("duplicitny f ina signatura", FunctionHeader("f", true, TYPE_STRING, NULL, 0, false), 4);
    check("f moze byt volane pred telom", Call("f", 1, oneInt, &(typeId){0}), 0);
    check("meno built-inu print", FunctionHeader("print", true, TYPE_UNIT, NULL, 0, false), 4);
    check("NULL meno", FunctionHeader(NULL, true, TYPE_INT, NULL, 0, false), 99);
    check("zaporny pocet parametrov",
          FunctionHeader("neg", true, TYPE_INT, NULL, -1, false), 99);
    check("NULL typy pri pocte 1",
          FunctionHeader("nullp", true, TYPE_INT, NULL, 1, false), 99);

    check("Header h bez typu", FunctionHeader("h", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Call h pred telom je 6", Call("h", 0, NULL, &(typeId){0}), 6);
    check("Start h", FuncBodyStart("h", NULL, NULL, 0, false), 0);
    check("End h(true, String)", FuncBodyEnd(true, TYPE_STRING), 0);
    check("Call h po tele", Call("h", 0, NULL, &(typeId){0}), 0);

    check("main s parametrom", FunctionHeader("main", true, TYPE_UNIT, oneInt, 1, true), 5);
    check("main s Int", FunctionHeader("main", true, TYPE_INT, NULL, 0, true), 6);
    check("main s Unit", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);
    check("druhy main", FunctionHeader("main2", true, TYPE_UNIT, NULL, 0, true), 4);
    check("CheckMain po main", CheckMain(), 0);

    fresh();
    check("main bez typu", FunctionHeader("main", false, TYPE_UNDEFINED, NULL, 0, true), 0);
    check("Start main", FuncBodyStart("main", NULL, NULL, 0, true), 0);
    check("End main bez resultu", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    check("CheckMain po inferovanom main", CheckMain(), 0);

    fresh();
    check("main s Int nezablokuje mainAlreadyDefined",
          FunctionHeader("main", true, TYPE_INT, NULL, 0, true), 6);
    check("potom platny main", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);

    endSection();
}

static void test_blocks_and_scope(void) {
    beginSection("4. bloky a scope");
    fresh();

    check("Header f(): Int", FunctionHeader("f", true, TYPE_INT, NULL, 0, false), 0);
    check("Start f", FuncBodyStart("f", NULL, NULL, 0, false), 0);
    check("EnterBlock", EnterBlock(), 0);
    check("DeclareVar x Int", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    checkVar("x vo vnutornom bloku", "x", 0, TYPE_INT);
    check("EnterBlock 2", EnterBlock(), 0);
    check("DeclareVar x String", DeclareVar("x", true, TYPE_STRING, false, TYPE_STRING), 0);
    checkVar("x v hlbsom bloku", "x", 0, TYPE_STRING);
    check("LeaveBlock 2", LeaveBlock(), 0);
    checkVar("x po vnutornom bloku", "x", 0, TYPE_INT);
    check("LeaveBlock", LeaveBlock(), 0);
    checkVar("x po bloku", "x", 3, TYPE_UNDEFINED);
    check("End f(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    check("LeaveBlock v roote", LeaveBlock(), 99);

    fresh();
    check("EnterBlock pre duplikat", EnterBlock(), 0);
    check("DeclareVar a", DeclareVar("a", true, TYPE_INT, false, TYPE_INT), 0);
    check("duplikat a v tom istom bloku", DeclareVar("a", true, TYPE_INT, false, TYPE_INT), 4);
    check("LeaveBlock po duplikate", LeaveBlock(), 0);

    check("EnterBlock brat 1", EnterBlock(), 0);
    check("DeclareVar i brat 1", DeclareVar("i", true, TYPE_INT, false, TYPE_INT), 0);
    check("LeaveBlock brat 1", LeaveBlock(), 0);
    check("EnterBlock brat 2", EnterBlock(), 0);
    check("DeclareVar i brat 2", DeclareVar("i", true, TYPE_INT, false, TYPE_INT), 0);
    check("LeaveBlock brat 2", LeaveBlock(), 0);

    fresh();
    check("Header g(): Int", FunctionHeader("g", true, TYPE_INT, NULL, 0, false), 0);
    check("Start g", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("lokalna g zatieni funkciu", DeclareVar("g", true, TYPE_STRING, false, TYPE_STRING), 0);
    checkVar("g je lokalna String", "g", 0, TYPE_STRING);
    check("Call g ako premenna", Call("g", 0, NULL, &(typeId){0}), 6);
    check("End g(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("EnterBlock v roote", EnterBlock(), 0);
    check("DeclareVar v bloku rootu", DeclareVar("rootvar", true, TYPE_INT, false, TYPE_INT), 0);
    checkVar("rootvar v bloku", "rootvar", 0, TYPE_INT);
    check("LeaveBlock rootu", LeaveBlock(), 0);
    checkVar("rootvar po bloku", "rootvar", 3, TYPE_UNDEFINED);
    check("DeclareVar priamo v roote",
          DeclareVar("bad", true, TYPE_INT, false, TYPE_INT), 99);
    check("GetVarType built-inu", GetVarType("print", &(typeId){0}), 0);
    check("GetVarType neznameho", GetVarType("nic", &(typeId){0}), 3);
    check("GetVarType NULL mena", GetVarType(NULL, &(typeId){0}), 99);
    check("GetVarType NULL out", GetVarType("print", NULL), 99);

    endSection();
}

static void test_declare_and_get(void) {
    beginSection("5. DeclareVar a GetVarType");
    fresh();

    check("Header f(): Int", FunctionHeader("f", true, TYPE_INT, NULL, 0, false), 0);
    check("Start f", FuncBodyStart("f", NULL, NULL, 0, false), 0);

    check("DeclareVar x Int", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    checkVar("GetVarType x", "x", 0, TYPE_INT);
    check("DeclareVar y Double z Int", DeclareVar("y", true, TYPE_DOUBLE, false, TYPE_INT), 0);
    checkVar("GetVarType y", "y", 0, TYPE_DOUBLE);
    check("DeclareVar z Int z Double", DeclareVar("z", true, TYPE_INT, false, TYPE_DOUBLE), 6);
    check("DeclareVar u Unit", DeclareVar("u", true, TYPE_UNIT, false, TYPE_UNIT), 0);
    checkVar("GetVarType u", "u", 0, TYPE_UNIT);
    check("DeclareVar v bez typu", DeclareVar("v", false, TYPE_UNDEFINED, false, TYPE_INT), 0);
    checkVar("GetVarType v", "v", 0, TYPE_INT);
    check("DeclareVar vu bez typu s Unit",
          DeclareVar("vu", false, TYPE_UNDEFINED, false, TYPE_UNIT), 0);
    checkVar("GetVarType vu", "vu", 0, TYPE_UNIT);
    check("DeclareVar badunit Unit z Int",
          DeclareVar("badunit", true, TYPE_UNIT, false, TYPE_INT), 6);
    check("DeclareVar w bez typu a Undefined",
          DeclareVar("w", false, TYPE_UNDEFINED, false, TYPE_UNDEFINED), 6);
    check("DeclareVar c const", DeclareVar("c", true, TYPE_INT, true, TYPE_INT), 0);
    check("zapis do const c", AssignValueToVar("c", TYPE_INT), 7);
    check("DeclareVar a", DeclareVar("a", true, TYPE_INT, false, TYPE_INT), 0);
    check("zapis do a", AssignValueToVar("a", TYPE_INT), 0);
    check("duplicitne x", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 4);
    check("DeclareVar NULL meno", DeclareVar(NULL, true, TYPE_INT, false, TYPE_INT), 99);
    check("DeclareVar r s Undefined init",
          DeclareVar("r", true, TYPE_INT, false, TYPE_UNDEFINED), 6);
    check("DeclareVar s String", DeclareVar("s", true, TYPE_STRING, false, TYPE_STRING), 0);
    checkVar("GetVarType s", "s", 0, TYPE_STRING);
    check("DeclareVar b Boolean", DeclareVar("b", true, TYPE_BOOLEAN, false, TYPE_BOOLEAN), 0);
    checkVar("GetVarType b", "b", 0, TYPE_BOOLEAN);
    check("DeclareVar bb Boolean z Int",
          DeclareVar("bb", true, TYPE_BOOLEAN, false, TYPE_INT), 6);
    checkVar("GetVarType neexistuje", "neexistuje", 3, TYPE_UNDEFINED);
    check("GetVarType NULL meno", GetVarType(NULL, &(typeId){0}), 99);
    check("GetVarType NULL out", GetVarType("x", NULL), 99);

    check("End f(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkVar("x po skonceni funkcie", "x", 3, TYPE_UNDEFINED);
    check("DeclareVar po skonceni v roote",
          DeclareVar("late", true, TYPE_INT, false, TYPE_INT), 99);

    endSection();
}

static void test_assign(void) {
    beginSection("6. AssignValueToVar");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    check("Header f(): Unit", FunctionHeader("f", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start f", FuncBodyStart("f", NULL, NULL, 0, false), 0);
    check("DeclareVar i Int", DeclareVar("i", true, TYPE_INT, false, TYPE_INT), 0);
    check("i = Int", AssignValueToVar("i", TYPE_INT), 0);
    check("i = Double", AssignValueToVar("i", TYPE_DOUBLE), 6);
    check("DeclareVar d Double", DeclareVar("d", true, TYPE_DOUBLE, false, TYPE_DOUBLE), 0);
    check("d = Int", AssignValueToVar("d", TYPE_INT), 0);
    check("d = Double", AssignValueToVar("d", TYPE_DOUBLE), 0);
    check("DeclareVar s String", DeclareVar("s", true, TYPE_STRING, false, TYPE_STRING), 0);
    check("s = String", AssignValueToVar("s", TYPE_STRING), 0);
    check("s = Int", AssignValueToVar("s", TYPE_INT), 6);
    check("DeclareVar b Boolean", DeclareVar("b", true, TYPE_BOOLEAN, false, TYPE_BOOLEAN), 0);
    check("b = Boolean", AssignValueToVar("b", TYPE_BOOLEAN), 0);
    check("b = Int", AssignValueToVar("b", TYPE_INT), 6);
    check("DeclareVar u Unit", DeclareVar("u", true, TYPE_UNIT, false, TYPE_UNIT), 0);
    check("u = Unit", AssignValueToVar("u", TYPE_UNIT), 0);
    check("u = Int", AssignValueToVar("u", TYPE_INT), 6);
    check("DeclareVar c const", DeclareVar("c", true, TYPE_INT, true, TYPE_INT), 0);
    check("c = Int", AssignValueToVar("c", TYPE_INT), 7);
    check("zapis do neznamej", AssignValueToVar("neexistuje", TYPE_INT), 3);
    check("zapis Undefined", AssignValueToVar("i", TYPE_UNDEFINED), 6);
    check("zapis NULL", AssignValueToVar(NULL, TYPE_INT), 99);
    check("zapis do funkcie f", AssignValueToVar("f", TYPE_INT), 7);
    check("DeclareVar lokalna f", DeclareVar("f", true, TYPE_INT, false, TYPE_INT), 0);
    check("zapis do lokalnej f", AssignValueToVar("f", TYPE_INT), 0);
    check("End f(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    check("Header g(a: Int): Unit", FunctionHeader("g", true, TYPE_UNIT, oneInt, 1, false), 0);
    check("Start g", FuncBodyStart("g", (const char *[]){"a"}, oneInt, 1, false), 0);
    check("zapis do parametra a", AssignValueToVar("a", TYPE_INT), 7);
    check("End g(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    check("zapis do f po skonceni", AssignValueToVar("f", TYPE_INT), 7);
    check("zapis do neexistujucej po skonceni",
          AssignValueToVar("neexistuje", TYPE_INT), 3);

    endSection();
}

static void test_call(void) {
    beginSection("7. Call");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    typeId oneDouble[1] = {TYPE_DOUBLE};
    typeId oneString[1] = {TYPE_STRING};
    typeId twoInts[2] = {TYPE_INT, TYPE_INT};

    check("Header f(Int): Int", FunctionHeader("f", true, TYPE_INT, oneInt, 1, false), 0);
    checkCall("f(Int) pred telom", "f", 1, oneInt, 0, TYPE_INT);
    check("Header g(Double): Double",
          FunctionHeader("g", true, TYPE_DOUBLE, oneDouble, 1, false), 0);
    checkCall("g(Int) implicit", "g", 1, oneInt, 0, TYPE_DOUBLE);
    checkCall("g(String)", "g", 1, oneString, 5, TYPE_UNDEFINED);
    checkCall("g()", "g", 0, NULL, 5, TYPE_UNDEFINED);
    checkCall("g(Int,Int)", "g", 2, twoInts, 5, TYPE_UNDEFINED);

    check("Header h bez typu",
          FunctionHeader("h", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    checkCall("h() pred telom", "h", 0, NULL, 6, TYPE_UNDEFINED);
    check("Start h", FuncBodyStart("h", NULL, NULL, 0, false), 0);
    check("End h(true, String)", FuncBodyEnd(true, TYPE_STRING), 0);
    checkCall("h() po tele", "h", 0, NULL, 0, TYPE_STRING);

    fresh();
    check("Header f(Int): Int", FunctionHeader("f", true, TYPE_INT, oneInt, 1, false), 0);
    check("Start f", FuncBodyStart("f", (const char *[]){"a"}, oneInt, 1, false), 0);
    check("DeclareVar x Int", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    checkCall("Call na premennu x", "x", 0, NULL, 6, TYPE_UNDEFINED);
    typeId sig = 0;
    check("FunctionType (Int)->Int", FunctionType(oneInt, 1, TYPE_INT, &sig), 0);
    check("DeclareVar fn s funkcnym typom",
          DeclareVar("fn", true, sig, false, sig), 0);
    checkCall("Call fn(Int)", "fn", 1, oneInt, 0, TYPE_INT);
    checkCall("Call fn(String)", "fn", 1, oneString, 5, TYPE_UNDEFINED);
    checkCall("Call fn()", "fn", 0, NULL, 5, TYPE_UNDEFINED);
    checkCall("Call f(Int) popri lokalnej fn", "f", 1, oneInt, 0, TYPE_INT);
    check("DeclareVar lokalna f", DeclareVar("f", true, TYPE_INT, false, TYPE_INT), 0);
    checkCall("Call f ako lokalna premenna", "f", 1, oneInt, 6, TYPE_UNDEFINED);
    check("End f(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("Call f po skonceni", "f", 1, oneInt, 0, TYPE_INT);

    fresh();
    checkCall("Call NULL meno", NULL, 0, NULL, 99, TYPE_UNDEFINED);
    checkCall("Call zaporny pocet", "print", -1, NULL, 99, TYPE_UNDEFINED);
    checkCall("Call NULL args pri pocte 1", "print", 1, NULL, 99, TYPE_UNDEFINED);
    check("Call NULL outType", Call("print", 1, oneInt, NULL), 99);
    checkCall("Call neznamy", "neexistuje", 0, NULL, 3, TYPE_UNDEFINED);
    checkCall("Call s Undefined argumentom", "print", 1,
              (typeId[]){TYPE_UNDEFINED}, 6, TYPE_UNDEFINED);
    checkCall("Call s 0 argumentmi a neNULL poli", "readInt", 0, oneInt, 0, TYPE_INT);

    endSection();
}

static void test_body_start(void) {
    beginSection("8. FuncBodyStart");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    check("Header f(Int): Int", FunctionHeader("f", true, TYPE_INT, oneInt, 1, false), 0);
    check("Start f",
          FuncBodyStart("f", (const char *[]){"a"}, oneInt, 1, false), 0);
    checkVar("parameter a je viditelny", "a", 0, TYPE_INT);
    check("zapis do parametra a", AssignValueToVar("a", TYPE_INT), 7);
    check("DeclareVar x v tele", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    checkVar("lokalna x je viditelna", "x", 0, TYPE_INT);
    check("druhy Start bez End",
          FuncBodyStart("f", (const char *[]){"a"}, oneInt, 1, false), 99);
    check("End f(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    check("Start po End",
          FuncBodyStart("f", (const char *[]){"a"}, oneInt, 1, false), 99);

    fresh();
    check("Header g(Int): Int", FunctionHeader("g", true, TYPE_INT, oneInt, 1, false), 0);
    check("Start g so zlym poctom", FuncBodyStart("g", NULL, NULL, 0, false), 99);
    check("Start g s NULL typmi",
          FuncBodyStart("g", (const char *[]){"b"}, NULL, 1, false), 99);
    check("Start g s NULL menami",
          FuncBodyStart("g", NULL, oneInt, 1, false), 99);
    check("Start g s Undefined typom parametra",
          FuncBodyStart("g", (const char *[]){"b"}, (typeId[]){TYPE_UNDEFINED}, 1, false),
          99);
    check("Start g s NULL menom parametra",
          FuncBodyStart("g", (const char *[]){NULL}, oneInt, 1, false), 99);
    check("Start g so zapornym poctom",
          FuncBodyStart("g", NULL, NULL, -1, false), 99);
    check("Start g s NULL menom funkcie",
          FuncBodyStart(NULL, NULL, NULL, 0, false), 99);
    check("Start g s viac parametrami ako signatura",
          FuncBodyStart("g", (const char *[]){"b", "c"}, (typeId[]){TYPE_INT, TYPE_INT}, 2,
                        false),
          99);
    check("Start g s nezhodnym typom parametra",
          FuncBodyStart("g", (const char *[]){"b"}, (typeId[]){TYPE_STRING}, 1, false), 99);
    check("Start g po chybach",
          FuncBodyStart("g", (const char *[]){"b"}, oneInt, 1, false), 0);
    check("End g(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    typeId twoInts[2] = {TYPE_INT, TYPE_INT};
    check("Header h(Int,Int): Int",
          FunctionHeader("h", true, TYPE_INT, twoInts, 2, false), 0);
    check("Start h s duplicitnym parametrom",
          FuncBodyStart("h", (const char *[]){"a", "a"}, twoInts, 2, false), 4);
    check("Start h s unikatnymi parametrami",
          FuncBodyStart("h", (const char *[]){"a", "b"}, twoInts, 2, false), 0);
    check("End h(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("Header k(Int) bez typu",
          FunctionHeader("k", false, TYPE_UNDEFINED, oneInt, 1, false), 0);
    check("Start k",
          FuncBodyStart("k", (const char *[]){"a"}, oneInt, 1, false), 0);
    check("End k(true, String)", FuncBodyEnd(true, TYPE_STRING), 0);
    checkCall("Call k(Int)", "k", 1, oneInt, 0, TYPE_STRING);

    fresh();
    check("Header main(): Unit", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);
    check("Start main", FuncBodyStart("main", NULL, NULL, 0, true), 0);
    check("druhy Start main", FuncBodyStart("main", NULL, NULL, 0, true), 99);
    check("End main(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    check("CheckMain", CheckMain(), 0);

    endSection();
}

static void test_body_end(void) {
    beginSection("9. FuncBodyEnd");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    check("Header f(): Int", FunctionHeader("f", true, TYPE_INT, NULL, 0, false), 0);
    check("Start f", FuncBodyStart("f", NULL, NULL, 0, false), 0);
    check("End f(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("Call f()", "f", 0, NULL, 0, TYPE_INT);
    check("Start f po End", FuncBodyStart("f", NULL, NULL, 0, false), 99);

    fresh();
    check("Header g(): Int", FunctionHeader("g", true, TYPE_INT, NULL, 0, false), 0);
    check("Start g", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("End g(true, String)", FuncBodyEnd(true, TYPE_STRING), 6);
    check("Start g po chybe", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("End g(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 6);

    fresh();
    check("Header h(): Int", FunctionHeader("h", true, TYPE_INT, NULL, 0, false), 0);
    check("Start h", FuncBodyStart("h", NULL, NULL, 0, false), 0);
    check("End h(false) bez returnu", FuncBodyEnd(false, TYPE_UNDEFINED), 6);

    fresh();
    check("Header r(): Int", FunctionHeader("r", true, TYPE_INT, NULL, 0, false), 0);
    check("Start r", FuncBodyStart("r", NULL, NULL, 0, false), 0);
    check("Return(true, Int)", Return(true, TYPE_INT), 0);
    check("End r(false) po returnе", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    checkCall("Call r()", "r", 0, NULL, 0, TYPE_INT);

    fresh();
    check("Header u(): Unit", FunctionHeader("u", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start u", FuncBodyStart("u", NULL, NULL, 0, false), 0);
    check("End u(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("Call u()", "u", 0, NULL, 0, TYPE_UNIT);

    fresh();
    check("Header v(): Unit", FunctionHeader("v", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start v", FuncBodyStart("v", NULL, NULL, 0, false), 0);
    check("Return(false)", Return(false, TYPE_UNDEFINED), 0);
    check("End v(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    fresh();
    check("Header g2(Int) bez typu",
          FunctionHeader("g2", false, TYPE_UNDEFINED, oneInt, 1, false), 0);
    check("Start g2",
          FuncBodyStart("g2", (const char *[]){"a"}, oneInt, 1, false), 0);
    check("End g2(true, String)", FuncBodyEnd(true, TYPE_STRING), 0);
    checkCall("Call g2(Int)", "g2", 1, oneInt, 0, TYPE_STRING);
    checkCall("Call g2(String)", "g2", 1, (typeId[]){TYPE_STRING}, 5, TYPE_UNDEFINED);

    fresh();
    check("Header h2() bez typu",
          FunctionHeader("h2", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Start h2", FuncBodyStart("h2", NULL, NULL, 0, false), 0);
    check("End h2(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    checkCall("Call h2()", "h2", 0, NULL, 0, TYPE_UNIT);

    fresh();
    check("Header i2() bez typu",
          FunctionHeader("i2", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Start i2", FuncBodyStart("i2", NULL, NULL, 0, false), 0);
    check("End i2(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("Call i2()", "i2", 0, NULL, 0, TYPE_INT);

    fresh();
    check("Header k() bez typu",
          FunctionHeader("k", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Start k", FuncBodyStart("k", NULL, NULL, 0, false), 0);
    check("Return(true, Int) bez typu", Return(true, TYPE_INT), 6);
    check("End k(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 6);

    fresh();
    check("Header main() bez typu",
          FunctionHeader("main", false, TYPE_UNDEFINED, NULL, 0, true), 0);
    check("Start main", FuncBodyStart("main", NULL, NULL, 0, true), 0);
    check("End main(true, Int)", FuncBodyEnd(true, TYPE_INT), 6);

    fresh();
    check("Header main2() bez typu",
          FunctionHeader("main2", false, TYPE_UNDEFINED, NULL, 0, true), 0);
    check("Start main2", FuncBodyStart("main2", NULL, NULL, 0, true), 0);
    check("End main2(true, Unit)", FuncBodyEnd(true, TYPE_UNIT), 0);
    check("CheckMain po main2", CheckMain(), 0);

    fresh();
    check("End bez Start", FuncBodyEnd(false, TYPE_UNDEFINED), 99);

    fresh();
    check("Header q(): Int", FunctionHeader("q", true, TYPE_INT, NULL, 0, false), 0);
    check("Start q", FuncBodyStart("q", NULL, NULL, 0, false), 0);
    check("End q(true, Undefined)", FuncBodyEnd(true, TYPE_UNDEFINED), 6);

    fresh();
    check("Header d(): Double", FunctionHeader("d", true, TYPE_DOUBLE, NULL, 0, false), 0);
    check("Start d", FuncBodyStart("d", NULL, NULL, 0, false), 0);
    check("End d(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("Call d()", "d", 0, NULL, 0, TYPE_DOUBLE);

    fresh();
    check("Header s(): String", FunctionHeader("s", true, TYPE_STRING, NULL, 0, false), 0);
    check("Start s", FuncBodyStart("s", NULL, NULL, 0, false), 0);
    check("End s(true, String)", FuncBodyEnd(true, TYPE_STRING), 0);
    checkCall("Call s()", "s", 0, NULL, 0, TYPE_STRING);

    fresh();
    check("Header b(): Boolean", FunctionHeader("b", true, TYPE_BOOLEAN, NULL, 0, false), 0);
    check("Start b", FuncBodyStart("b", NULL, NULL, 0, false), 0);
    check("End b(true, Boolean)", FuncBodyEnd(true, TYPE_BOOLEAN), 0);
    checkCall("Call b()", "b", 0, NULL, 0, TYPE_BOOLEAN);

    fresh();
    check("Header i4(): Int", FunctionHeader("i4", true, TYPE_INT, NULL, 0, false), 0);
    check("Start i4", FuncBodyStart("i4", NULL, NULL, 0, false), 0);
    check("End i4(true, Double)", FuncBodyEnd(true, TYPE_DOUBLE), 6);

    fresh();
    check("Header u3(): Unit", FunctionHeader("u3", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start u3", FuncBodyStart("u3", NULL, NULL, 0, false), 0);
    check("End u3(false, Int) ignoruje result", FuncBodyEnd(false, TYPE_INT), 0);

    fresh();
    check("Header u2(): Unit", FunctionHeader("u2", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start u2", FuncBodyStart("u2", NULL, NULL, 0, false), 0);
    check("End u2(true, Undefined)", FuncBodyEnd(true, TYPE_UNDEFINED), 6);

    fresh();
    check("Header f2(): Int", FunctionHeader("f2", true, TYPE_INT, NULL, 0, false), 0);
    check("Start f2", FuncBodyStart("f2", NULL, NULL, 0, false), 0);
    check("DeclareVar x pred End", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    check("End f2(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);
    checkVar("x po End", "x", 3, TYPE_UNDEFINED);

    endSection();
}

static void test_return(void) {
    beginSection("10. Return");
    fresh();
    check("Return bez tela", Return(false, TYPE_UNDEFINED), 99);

    fresh();
    check("Header v(): Unit", FunctionHeader("v", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start v", FuncBodyStart("v", NULL, NULL, 0, false), 0);
    check("holy return v Unit", Return(false, TYPE_UNDEFINED), 0);
    check("End v", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    fresh();
    check("Header r(): Int", FunctionHeader("r", true, TYPE_INT, NULL, 0, false), 0);
    check("Start r", FuncBodyStart("r", NULL, NULL, 0, false), 0);
    check("holy return v Int", Return(false, TYPE_UNDEFINED), 10);

    fresh();
    check("Header k() bez typu",
          FunctionHeader("k", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Start k", FuncBodyStart("k", NULL, NULL, 0, false), 0);
    check("holy return bez typu", Return(false, TYPE_UNDEFINED), 10);
    check("End k po chybe", FuncBodyEnd(false, TYPE_UNDEFINED), 6);

    fresh();
    check("Header k2() bez typu",
          FunctionHeader("k2", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("Start k2", FuncBodyStart("k2", NULL, NULL, 0, false), 0);
    check("return Int bez typu", Return(true, TYPE_INT), 6);

    fresh();
    check("Header i(): Int", FunctionHeader("i", true, TYPE_INT, NULL, 0, false), 0);
    check("Start i", FuncBodyStart("i", NULL, NULL, 0, false), 0);
    check("return Int v Int", Return(true, TYPE_INT), 0);
    check("End i(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    check("Return po End", Return(false, TYPE_UNDEFINED), 99);

    fresh();
    check("Header u2(): Unit", FunctionHeader("u2", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start u2", FuncBodyStart("u2", NULL, NULL, 0, false), 0);
    check("holy return s hodnotou v Unit", Return(false, TYPE_INT), 0);
    check("End u2", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    fresh();
    check("Header d(): Double", FunctionHeader("d", true, TYPE_DOUBLE, NULL, 0, false), 0);
    check("Start d", FuncBodyStart("d", NULL, NULL, 0, false), 0);
    check("return Int v Double", Return(true, TYPE_INT), 0);

    fresh();
    check("Header i2(): Int", FunctionHeader("i2", true, TYPE_INT, NULL, 0, false), 0);
    check("Start i2", FuncBodyStart("i2", NULL, NULL, 0, false), 0);
    check("return Double v Int", Return(true, TYPE_DOUBLE), 6);

    fresh();
    check("Header i3(): Int", FunctionHeader("i3", true, TYPE_INT, NULL, 0, false), 0);
    check("Start i3", FuncBodyStart("i3", NULL, NULL, 0, false), 0);
    check("return String v Int", Return(true, TYPE_STRING), 6);

    fresh();
    check("Header u(): Unit", FunctionHeader("u", true, TYPE_UNIT, NULL, 0, false), 0);
    check("Start u", FuncBodyStart("u", NULL, NULL, 0, false), 0);
    check("return Int v Unit", Return(true, TYPE_INT), 0);
    check("End u(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("Header b(): Boolean", FunctionHeader("b", true, TYPE_BOOLEAN, NULL, 0, false), 0);
    check("Start b", FuncBodyStart("b", NULL, NULL, 0, false), 0);
    check("return Boolean v Boolean", Return(true, TYPE_BOOLEAN), 0);

    fresh();
    check("Header z(): Int", FunctionHeader("z", true, TYPE_INT, NULL, 0, false), 0);
    check("Start z", FuncBodyStart("z", NULL, NULL, 0, false), 0);
    check("return Undefined", Return(true, TYPE_UNDEFINED), 6);

    fresh();
    check("Header w(): Int", FunctionHeader("w", true, TYPE_INT, NULL, 0, false), 0);
    check("Start w", FuncBodyStart("w", NULL, NULL, 0, false), 0);
    check("prvy return Int", Return(true, TYPE_INT), 0);
    check("druhy holy return v Int", Return(false, TYPE_UNDEFINED), 10);

    endSection();
}

typedef struct {
    const char *what;
    typeId left;
    typeId right;
    TokenType op;
    int code;
    typeId type;
} OpCase;

static void test_operator(void) {
    beginSection("11. Operator");
    fresh();

    static const OpCase cases[] = {
        {"unary - Int", TYPE_INT, TYPE_UNDEFINED, TOKEN_MINUS, 0, TYPE_INT},
        {"unary - Double", TYPE_DOUBLE, TYPE_UNDEFINED, TOKEN_MINUS, 0, TYPE_DOUBLE},
        {"unary + Int", TYPE_INT, TYPE_UNDEFINED, TOKEN_PLUS, 0, TYPE_INT},
        {"unary + Double", TYPE_DOUBLE, TYPE_UNDEFINED, TOKEN_PLUS, 0, TYPE_DOUBLE},
        {"unary ! Boolean", TYPE_BOOLEAN, TYPE_UNDEFINED, TOKEN_NOT, 0, TYPE_BOOLEAN},
        {"unary ! Int", TYPE_INT, TYPE_UNDEFINED, TOKEN_NOT, 6, TYPE_UNDEFINED},
        {"unary + String", TYPE_STRING, TYPE_UNDEFINED, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"unary - Boolean", TYPE_BOOLEAN, TYPE_UNDEFINED, TOKEN_MINUS, 6, TYPE_UNDEFINED},
        {"unary ! s pravym", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_NOT, 99, TYPE_UNDEFINED},
        {"Int + Int", TYPE_INT, TYPE_INT, TOKEN_PLUS, 0, TYPE_INT},
        {"Int + Double", TYPE_INT, TYPE_DOUBLE, TOKEN_PLUS, 0, TYPE_DOUBLE},
        {"Double + Int", TYPE_DOUBLE, TYPE_INT, TOKEN_PLUS, 0, TYPE_DOUBLE},
        {"Double + Double", TYPE_DOUBLE, TYPE_DOUBLE, TOKEN_PLUS, 0, TYPE_DOUBLE},
        {"String + String", TYPE_STRING, TYPE_STRING, TOKEN_PLUS, 0, TYPE_STRING},
        {"String + Int", TYPE_STRING, TYPE_INT, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"Int + String", TYPE_INT, TYPE_STRING, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"String + Double", TYPE_STRING, TYPE_DOUBLE, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"Unit + Unit", TYPE_UNIT, TYPE_UNIT, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"Int - Int", TYPE_INT, TYPE_INT, TOKEN_MINUS, 0, TYPE_INT},
        {"Double - Double", TYPE_DOUBLE, TYPE_DOUBLE, TOKEN_MINUS, 0, TYPE_DOUBLE},
        {"Int - Double", TYPE_INT, TYPE_DOUBLE, TOKEN_MINUS, 0, TYPE_DOUBLE},
        {"Int * Int", TYPE_INT, TYPE_INT, TOKEN_MUL, 0, TYPE_INT},
        {"Double * Double", TYPE_DOUBLE, TYPE_DOUBLE, TOKEN_MUL, 0, TYPE_DOUBLE},
        {"Int / Int", TYPE_INT, TYPE_INT, TOKEN_DIV, 0, TYPE_INT},
        {"Double / Int", TYPE_DOUBLE, TYPE_INT, TOKEN_DIV, 0, TYPE_DOUBLE},
        {"Double / Double", TYPE_DOUBLE, TYPE_DOUBLE, TOKEN_DIV, 0, TYPE_DOUBLE},
        {"Int < Int", TYPE_INT, TYPE_INT, TOKEN_LT, 0, TYPE_BOOLEAN},
        {"Double < Int", TYPE_DOUBLE, TYPE_INT, TOKEN_LT, 0, TYPE_BOOLEAN},
        {"String < String", TYPE_STRING, TYPE_STRING, TOKEN_LT, 0, TYPE_BOOLEAN},
        {"Int < String", TYPE_INT, TYPE_STRING, TOKEN_LT, 6, TYPE_UNDEFINED},
        {"Boolean < Boolean", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_LT, 6, TYPE_UNDEFINED},
        {"Int <= Double", TYPE_INT, TYPE_DOUBLE, TOKEN_LTE, 0, TYPE_BOOLEAN},
        {"String <= String", TYPE_STRING, TYPE_STRING, TOKEN_LTE, 0, TYPE_BOOLEAN},
        {"Int > Int", TYPE_INT, TYPE_INT, TOKEN_GT, 0, TYPE_BOOLEAN},
        {"Double >= Int", TYPE_DOUBLE, TYPE_INT, TOKEN_GTE, 0, TYPE_BOOLEAN},
        {"Int == Int", TYPE_INT, TYPE_INT, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"Int == Double", TYPE_INT, TYPE_DOUBLE, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"Double == Int", TYPE_DOUBLE, TYPE_INT, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"String == String", TYPE_STRING, TYPE_STRING, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"Boolean == Boolean", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"Unit == Unit", TYPE_UNIT, TYPE_UNIT, TOKEN_EQ, 0, TYPE_BOOLEAN},
        {"String == Int", TYPE_STRING, TYPE_INT, TOKEN_EQ, 6, TYPE_UNDEFINED},
        {"String == Boolean", TYPE_STRING, TYPE_BOOLEAN, TOKEN_EQ, 6, TYPE_UNDEFINED},
        {"Int != Int", TYPE_INT, TYPE_INT, TOKEN_NEQ, 0, TYPE_BOOLEAN},
        {"Int != Double", TYPE_INT, TYPE_DOUBLE, TOKEN_NEQ, 0, TYPE_BOOLEAN},
        {"Boolean != Boolean", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_NEQ, 0, TYPE_BOOLEAN},
        {"Unit != Unit", TYPE_UNIT, TYPE_UNIT, TOKEN_NEQ, 0, TYPE_BOOLEAN},
        {"Int && Int", TYPE_INT, TYPE_INT, TOKEN_AND, 6, TYPE_UNDEFINED},
        {"Boolean && Boolean", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_AND, 0, TYPE_BOOLEAN},
        {"Boolean || Boolean", TYPE_BOOLEAN, TYPE_BOOLEAN, TOKEN_OR, 0, TYPE_BOOLEAN},
        {"Int || Boolean", TYPE_INT, TYPE_BOOLEAN, TOKEN_OR, 6, TYPE_UNDEFINED},
        {"Boolean && Unit", TYPE_BOOLEAN, TYPE_UNIT, TOKEN_AND, 6, TYPE_UNDEFINED},
        {"Int + Any", TYPE_INT, TYPE_ANY, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"Any + Int", TYPE_ANY, TYPE_INT, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"unary ! Any", TYPE_ANY, TYPE_UNDEFINED, TOKEN_NOT, 6, TYPE_UNDEFINED},
        {"Any == Any", TYPE_ANY, TYPE_ANY, TOKEN_EQ, 6, TYPE_UNDEFINED},
        {"Undefined + Int", TYPE_UNDEFINED, TYPE_INT, TOKEN_PLUS, 6, TYPE_UNDEFINED},
        {"unary ! Undefined", TYPE_UNDEFINED, TYPE_UNDEFINED, TOKEN_NOT, 6, TYPE_UNDEFINED},
        {"neznamy operator", TYPE_INT, TYPE_INT, (TokenType)9999, 99, TYPE_UNDEFINED},
        {"neznamy operator s Undefined", TYPE_INT, TYPE_UNDEFINED, (TokenType)9999, 99,
         TYPE_UNDEFINED},
        {"binarne * bez praveho", TYPE_INT, TYPE_UNDEFINED, TOKEN_MUL, 6, TYPE_UNDEFINED},
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        checkOp(cases[i].what, cases[i].left, cases[i].right, cases[i].op,
                cases[i].code, cases[i].type);

    typeId out = 12345;
    check("Operator NULL outType", Operator(TYPE_INT, TYPE_INT, TOKEN_PLUS, NULL), 99);
    check("Operator error nastavi Undefined",
          Operator(TYPE_INT, TYPE_STRING, TOKEN_PLUS, &out), 6);
    checkType("Operator error out", out, TYPE_UNDEFINED);

    endSection();
}

typedef struct {
    const char *what;
    typeId objType;
    const char *member;
    int code;
    typeId type;
} MemberCase;

static void test_valmem(void) {
    beginSection("12. ValmemMember");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    typeId sig = 0;
    check("FunctionType pre ValmemMember", FunctionType(oneInt, 1, TYPE_INT, &sig), 0);

    static const MemberCase cases[] = {
        {"Int.toString", TYPE_INT, "toString", 0, TYPE_STRING},
        {"Double.toString", TYPE_DOUBLE, "toString", 0, TYPE_STRING},
        {"String.toString", TYPE_STRING, "toString", 0, TYPE_STRING},
        {"Boolean.toString", TYPE_BOOLEAN, "toString", 0, TYPE_STRING},
        {"Unit.toString", TYPE_UNIT, "toString", 0, TYPE_STRING},
        {"Undefined.toString", TYPE_UNDEFINED, "toString", 6, TYPE_UNDEFINED},
        {"Any.toString", TYPE_ANY, "toString", 6, TYPE_UNDEFINED},
        {"Double.toInt", TYPE_DOUBLE, "toInt", 0, TYPE_INT},
        {"Int.toInt", TYPE_INT, "toInt", 6, TYPE_UNDEFINED},
        {"String.toInt", TYPE_STRING, "toInt", 6, TYPE_UNDEFINED},
        {"Boolean.toInt", TYPE_BOOLEAN, "toInt", 6, TYPE_UNDEFINED},
        {"Unit.toInt", TYPE_UNIT, "toInt", 6, TYPE_UNDEFINED},
        {"Double.floor", TYPE_DOUBLE, "floor", 0, TYPE_DOUBLE},
        {"Int.floor", TYPE_INT, "floor", 6, TYPE_UNDEFINED},
        {"String.floor", TYPE_STRING, "floor", 6, TYPE_UNDEFINED},
        {"Unit.floor", TYPE_UNIT, "floor", 6, TYPE_UNDEFINED},
        {"String.length", TYPE_STRING, "length", 0, TYPE_INT},
        {"Int.length", TYPE_INT, "length", 6, TYPE_UNDEFINED},
        {"Double.length", TYPE_DOUBLE, "length", 6, TYPE_UNDEFINED},
        {"Boolean.length", TYPE_BOOLEAN, "length", 6, TYPE_UNDEFINED},
        {"Unit.length", TYPE_UNIT, "length", 6, TYPE_UNDEFINED},
        {"Any.toInt", TYPE_ANY, "toInt", 6, TYPE_UNDEFINED},
        {"Any.length", TYPE_ANY, "length", 6, TYPE_UNDEFINED},
        {"Double.toString.length", TYPE_STRING, "length", 0, TYPE_INT},
        {"neznamy clen", TYPE_INT, "neexistuje", 6, TYPE_UNDEFINED},
        {"prazdny clen", TYPE_INT, "", 6, TYPE_UNDEFINED},
        {"velke ToString", TYPE_INT, "ToString", 6, TYPE_UNDEFINED},
        {"male tostring", TYPE_INT, "tostring", 6, TYPE_UNDEFINED},
        {"medzera v clene", TYPE_INT, "toString ", 6, TYPE_UNDEFINED},
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        checkMember(cases[i].what, cases[i].objType, cases[i].member,
                    cases[i].code, cases[i].type);

    checkMember("Int.toString.toString", TYPE_INT, "toString", 0, TYPE_STRING);
    checkMember("funkcny typ.toString", sig, "toString", 6, TYPE_UNDEFINED);
    checkMember("String.toInt", TYPE_STRING, "toInt", 6, TYPE_UNDEFINED);
    checkMember("String.length po toString", TYPE_STRING, "length", 0, TYPE_INT);
    checkMember("Double.toInt po floor", TYPE_DOUBLE, "toInt", 0, TYPE_INT);

    check("ValmemMember NULL member", ValmemMember(TYPE_INT, NULL, &(typeId){0}), 99);
    check("ValmemMember NULL outType", ValmemMember(TYPE_INT, "toString", NULL), 99);

    typeId out = 12345;
    check("ValmemMember error", ValmemMember(TYPE_INT, "neexistuje", &out), 6);
    checkType("ValmemMember error out", out, TYPE_UNDEFINED);

    endSection();
}

static void test_condition(void) {
    beginSection("13. ConditionType");
    fresh();

    check("Boolean", ConditionType(TYPE_BOOLEAN), 0);
    check("Int", ConditionType(TYPE_INT), 6);
    check("Double", ConditionType(TYPE_DOUBLE), 6);
    check("String", ConditionType(TYPE_STRING), 6);
    check("Unit", ConditionType(TYPE_UNIT), 6);
    check("Any", ConditionType(TYPE_ANY), 6);
    check("Undefined", ConditionType(TYPE_UNDEFINED), 6);

    endSection();
}

static void test_statement(void) {
    beginSection("14. StatementValueType");
    fresh();

    check("Int", StatementValueType(TYPE_INT), 0);
    check("Double", StatementValueType(TYPE_DOUBLE), 0);
    check("String", StatementValueType(TYPE_STRING), 0);
    check("Boolean", StatementValueType(TYPE_BOOLEAN), 0);
    check("Unit", StatementValueType(TYPE_UNIT), 0);
    check("Any", StatementValueType(TYPE_ANY), 0);
    check("Undefined", StatementValueType(TYPE_UNDEFINED), 99);

    endSection();
}

static void test_checkmain(void) {
    beginSection("15. CheckMain");
    fresh();
    check("bez main", CheckMain(), 3);
    check("Header main", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);
    check("po hlavicke main", CheckMain(), 0);
    check("druhy main", FunctionHeader("main2", true, TYPE_UNIT, NULL, 0, true), 4);
    check("po druhom main", CheckMain(), 0);

    fresh();
    check("bez main po resete", CheckMain(), 3);
    check("Header bez main", FunctionHeader("f", true, TYPE_INT, NULL, 0, false), 0);
    check("po beznej funkcii", CheckMain(), 3);

    fresh();
    check("main s parametrom", FunctionHeader("main", true, TYPE_UNIT,
                                               (typeId[]){TYPE_INT}, 1, true), 5);
    check("po neplatnom main", CheckMain(), 3);

    fresh();
    check("main s Int", FunctionHeader("main", true, TYPE_INT, NULL, 0, true), 6);
    check("po dalsom neplatnom main", CheckMain(), 3);

    fresh();
    check("main bez typu", FunctionHeader("main", false, TYPE_UNDEFINED, NULL, 0, true), 0);
    check("po main bez typu", CheckMain(), 0);

    endSection();
}

static void test_integration(void) {
    beginSection("16. integracne sekvencie");
    fresh();

    typeId oneInt[1] = {TYPE_INT};
    typeId oneString[1] = {TYPE_STRING};
    check("add header", FunctionHeader("add", true, TYPE_INT, oneInt, 1, false), 0);
    check("add start", FuncBodyStart("add", (const char *[]){"a"}, oneInt, 1, false), 0);
    checkVar("add parameter a", "a", 0, TYPE_INT);
    check("add DeclareVar one", DeclareVar("one", true, TYPE_INT, false, TYPE_INT), 0);
    checkOp("add a + one", TYPE_INT, TYPE_INT, TOKEN_PLUS, 0, TYPE_INT);
    check("add End", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("add(Int)", "add", 1, oneInt, 0, TYPE_INT);
    check("add bez main", CheckMain(), 3);

    fresh();
    check("greet header", FunctionHeader("greet", false, TYPE_UNDEFINED, oneInt, 1, false), 0);
    check("greet start",
          FuncBodyStart("greet", (const char *[]){"a"}, oneInt, 1, false), 0);
    checkCall("greet toStr(a)", "toStr", 1, oneInt, 0, TYPE_STRING);
    checkOp("greet String + String", TYPE_STRING, TYPE_STRING, TOKEN_PLUS, 0, TYPE_STRING);
    check("greet End", FuncBodyEnd(true, TYPE_STRING), 0);
    checkCall("greet(Int)", "greet", 1, oneInt, 0, TYPE_STRING);

    fresh();
    check("main header", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);
    check("main start", FuncBodyStart("main", NULL, NULL, 0, true), 0);
    checkCall("main print(1)", "print", 1, oneInt, 0, TYPE_UNIT);
    check("main statement print", StatementValueType(TYPE_UNIT), 0);
    check("main End", FuncBodyEnd(true, TYPE_UNIT), 0);
    check("main CheckMain", CheckMain(), 0);

    fresh();
    check("shadow header", FunctionHeader("shadow", true, TYPE_INT, NULL, 0, false), 0);
    check("shadow start", FuncBodyStart("shadow", NULL, NULL, 0, false), 0);
    check("shadow x Int", DeclareVar("x", true, TYPE_INT, false, TYPE_INT), 0);
    check("shadow EnterBlock", EnterBlock(), 0);
    check("shadow x String", DeclareVar("x", true, TYPE_STRING, false, TYPE_STRING), 0);
    checkVar("shadow x String", "x", 0, TYPE_STRING);
    check("shadow LeaveBlock", LeaveBlock(), 0);
    checkVar("shadow x Int", "x", 0, TYPE_INT);
    check("shadow End", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("forward a header", FunctionHeader("a", true, TYPE_INT, NULL, 0, false), 0);
    check("forward b header", FunctionHeader("b", true, TYPE_INT, NULL, 0, false), 0);
    check("forward b start", FuncBodyStart("b", NULL, NULL, 0, false), 0);
    checkCall("b vola a pred jeho telom", "a", 0, NULL, 0, TYPE_INT);
    check("forward b End", FuncBodyEnd(true, TYPE_INT), 0);
    check("forward a start", FuncBodyStart("a", NULL, NULL, 0, false), 0);
    check("forward a End", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("forward a()", "a", 0, NULL, 0, TYPE_INT);

    fresh();
    check("fn header", FunctionHeader("hfn", true, TYPE_INT, NULL, 0, false), 0);
    check("fn start", FuncBodyStart("hfn", NULL, NULL, 0, false), 0);
    typeId sig = 0;
    check("fn FunctionType", FunctionType(oneInt, 1, TYPE_INT, &sig), 0);
    check("fn DeclareVar", DeclareVar("fn", true, sig, false, sig), 0);
    checkCall("fn(Int)", "fn", 1, oneInt, 0, TYPE_INT);
    checkCall("fn(String)", "fn", 1, oneString, 5, TYPE_UNDEFINED);
    check("fn End", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("while header", FunctionHeader("w", true, TYPE_UNIT, NULL, 0, false), 0);
    check("while start", FuncBodyStart("w", NULL, NULL, 0, false), 0);
    check("while block 1", EnterBlock(), 0);
    check("while i 1", DeclareVar("i", true, TYPE_INT, false, TYPE_INT), 0);
    check("while leave 1", LeaveBlock(), 0);
    check("while block 2", EnterBlock(), 0);
    check("while i 2", DeclareVar("i", true, TYPE_INT, false, TYPE_INT), 0);
    check("while leave 2", LeaveBlock(), 0);
    check("while End", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    endSection();
}

static void test_api_edges_and_stress(void) {
    beginSection("17. API hrany a stres");
    fresh();

    typeId oneInt[1] = {TYPE_INT};

    check("Header bez typu ignoruje returnType",
          FunctionHeader("f", false, TYPE_INT, NULL, 0, false), 0);
    checkCall("Call f pred telom po ignorovani", "f", 0, NULL, 6, TYPE_UNDEFINED);
    check("Start f", FuncBodyStart("f", NULL, NULL, 0, false), 0);
    check("End f(true, Unit)", FuncBodyEnd(true, TYPE_UNIT), 0);
    checkCall("Call f po tele", "f", 0, NULL, 0, TYPE_UNIT);

    fresh();
    check("Header g(Int): Int", FunctionHeader("g", true, TYPE_INT, oneInt, 1, false), 0);
    check("Start g", FuncBodyStart("g", (const char *[]){"a"}, oneInt, 1, false), 0);
    typeId gType = 0;
    check("GetVarType vrati id signatury", GetVarType("g", &gType), 0);
    check("id signatury je >= 100", gType >= 100, 1);
    check("End g(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("Header host(): Int", FunctionHeader("host", true, TYPE_INT, NULL, 0, false), 0);
    check("Start host", FuncBodyStart("host", NULL, NULL, 0, false), 0);
    typeId bogus = 1099;
    check("DeclareVar s neplatnym funkcnym typom",
          DeclareVar("bad", true, bogus, false, bogus), 0);
    checkCall("Call na neplatnu signaturu", "bad", 0, NULL, 99, TYPE_UNDEFINED);
    check("End host", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("Header shadow(): Int", FunctionHeader("shadow", true, TYPE_INT, NULL, 0, false), 0);
    check("Start shadow", FuncBodyStart("shadow", NULL, NULL, 0, false), 0);
    check("lokalna premenna print zatieni built-in",
          DeclareVar("print", true, TYPE_INT, false, TYPE_INT), 0);
    checkVar("print je lokalna Int", "print", 0, TYPE_INT);
    checkCall("Call print ako premenna", "print", 1, oneInt, 6, TYPE_UNDEFINED);
    check("End shadow", FuncBodyEnd(true, TYPE_INT), 0);
    checkCall("print sa po skonceni vrati", "print", 1, oneInt, 0, TYPE_UNIT);

    fresh();
    check("Header ret(): Int", FunctionHeader("ret", true, TYPE_INT, NULL, 0, false), 0);
    check("Start ret", FuncBodyStart("ret", NULL, NULL, 0, false), 0);
    check("return Unit v Int", Return(true, TYPE_UNIT), 6);
    check("return Int v Int", Return(true, TYPE_INT), 0);
    check("End ret(false)", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    fresh();
    typeId sig = 0;
    check("FunctionType pre hrany", FunctionType(NULL, 0, TYPE_INT, &sig), 0);
    check("ConditionType s funkcnym typom", ConditionType(sig), 6);
    check("StatementValueType s funkcnym typom", StatementValueType(sig), 0);
    checkOp("Operator s funkcnym typom", sig, TYPE_INT, TOKEN_PLUS, 6, TYPE_UNDEFINED);
    checkOp("String - Int", TYPE_STRING, TYPE_INT, TOKEN_MINUS, 6, TYPE_UNDEFINED);
    checkOp("Int * Boolean", TYPE_INT, TYPE_BOOLEAN, TOKEN_MUL, 6, TYPE_UNDEFINED);
    checkOp("Unit / Int", TYPE_UNIT, TYPE_INT, TOKEN_DIV, 6, TYPE_UNDEFINED);
    checkOp("String * String", TYPE_STRING, TYPE_STRING, TOKEN_MUL, 6, TYPE_UNDEFINED);
    checkOp("Int - Any", TYPE_INT, TYPE_ANY, TOKEN_MINUS, 6, TYPE_UNDEFINED);

    fresh();
    typeId fnSig = 0;
    check("FunctionType pre parameter h", FunctionType(NULL, 0, TYPE_INT, &fnSig), 0);
    check("Header h(funkcia): Int",
          FunctionHeader("h", true, TYPE_INT, &fnSig, 1, false), 0);
    check("Start h s funkcnym parametrom",
          FuncBodyStart("h", (const char *[]){"fn"}, &fnSig, 1, false), 0);
    checkVar("funkcny parameter fn", "fn", 0, fnSig);
    check("End h(true, Int)", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    check("Start neznamy", FuncBodyStart("neexistuje", NULL, NULL, 0, false), 99);
    check("Header zero parametrov s neNULL polom",
          FunctionHeader("zero", true, TYPE_INT, oneInt, 0, false), 0);

    fresh();
    check("Header stress(): Int", FunctionHeader("stress", true, TYPE_INT, NULL, 0, false), 0);
    check("Start stress", FuncBodyStart("stress", NULL, NULL, 0, false), 0);
    int stressOk = 1;
    for (int i = 0; i < 500 && stressOk; i++) {
        if (EnterBlock() != 0 ||
            DeclareVar("x", true, TYPE_INT, false, TYPE_INT) != 0)
            stressOk = 0;
    }
    check("500 vnorenych blokov", stressOk, 1);
    for (int i = 0; i < 500 && stressOk; i++) {
        if (LeaveBlock() != 0)
            stressOk = 0;
    }
    check("500 LeaveBlock", stressOk, 1);
    checkVar("x po stresovych blokoch", "x", 3, TYPE_UNDEFINED);
    check("End stress", FuncBodyEnd(true, TYPE_INT), 0);

    fresh();
    int functionsOk = 1;
    char name[32];
    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof(name), "sf%d", i);
        if (FunctionHeader(name, true, TYPE_INT, NULL, 0, false) != 0) {
            functionsOk = 0;
            break;
        }
    }
    check("100 funkcii v roote", functionsOk, 1);
    int callsOk = 1;
    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof(name), "sf%d", i);
        typeId out = 0;
        if (Call(name, 0, NULL, &out) != 0 || out != TYPE_INT) {
            callsOk = 0;
            break;
        }
    }
    check("100 volani pred telom", callsOk, 1);

    fresh();
    check("Header locals(): Int", FunctionHeader("locals", true, TYPE_INT, NULL, 0, false), 0);
    check("Start locals", FuncBodyStart("locals", NULL, NULL, 0, false), 0);
    int localsOk = 1;
    for (int i = 0; i < 200; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        if (DeclareVar(name, true, TYPE_INT, false, TYPE_INT) != 0) {
            localsOk = 0;
            break;
        }
    }
    check("200 lokalnych premennych", localsOk, 1);
    int lookupsOk = 1;
    for (int i = 0; i < 200; i++) {
        snprintf(name, sizeof(name), "v%d", i);
        typeId out = 0;
        if (GetVarType(name, &out) != 0 || out != TYPE_INT) {
            lookupsOk = 0;
            break;
        }
    }
    check("200 lookupov lokalnych premennych", lookupsOk, 1);
    check("duplikat v199", DeclareVar("v199", true, TYPE_INT, false, TYPE_INT), 4);
    check("End locals", FuncBodyEnd(true, TYPE_INT), 0);

    endSection();
}

static int fuzzCodeOk(int code) {
    switch (code) {
        case 0:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 10:
        case 99:
            return 1;
        default:
            return 0;
    }
}

static void test_fuzz_api(void) {
    beginSection("18. deterministicky fuzz API");
    fresh();

    static const typeId tvals[8] = {TYPE_UNDEFINED, TYPE_INT,       TYPE_DOUBLE, TYPE_STRING,
                                    TYPE_BOOLEAN,   TYPE_UNIT,      TYPE_ANY,    1099};
    static const char *names[8] = {"a", "b", "c", "f", "g", "main", "print", "x"};
    const char *pnames[2] = {"p", "q"};
    unsigned state = 0x13579BDFu;
    int noCrash = 1;
    int codesOk = 1;

    for (int i = 0; i < 5000 && noCrash; i++) {
        if (i % 100 == 0) {
            SemAnalysisFree();
            if (SemAnalysisInit() != 0) {
                noCrash = 0;
                break;
            }
        }

        state = state * 1664525u + 1013904223u;
        typeId args[2] = {tvals[state % 8], tvals[(state >> 3) % 8]};
        typeId out = 0;
        const char *fn = names[(state >> 6) % 8];
        int code = 0;

        switch ((state >> 9) % 16) {
            case 0:
                code = FunctionType(args, (int)((state >> 12) % 3),
                                    tvals[(state >> 14) % 8], &out);
                break;
            case 1:
                code = FunctionHeader(fn, ((state >> 1) & 1) != 0,
                                      tvals[(state >> 2) % 8], args, (int)((state >> 12) % 3),
                                      ((state >> 4) & 1) != 0);
                break;
            case 2:
                code = FuncBodyStart(fn, pnames, args, (int)((state >> 14) % 3),
                                     ((state >> 5) & 1) != 0);
                break;
            case 3:
                code = FuncBodyEnd(((state >> 1) & 1) != 0, tvals[(state >> 2) % 8]);
                break;
            case 4:
                code = EnterBlock();
                break;
            case 5:
                code = LeaveBlock();
                break;
            case 6:
                code = DeclareVar(fn, ((state >> 1) & 1) != 0, tvals[(state >> 2) % 8],
                                  ((state >> 3) & 1) != 0, tvals[(state >> 4) % 8]);
                break;
            case 7:
                code = GetVarType(fn, &out);
                break;
            case 8:
                code = Call(fn, (int)((state >> 16) % 3), args, &out);
                break;
            case 9:
                code = AssignValueToVar(fn, tvals[(state >> 1) % 8]);
                break;
            case 10:
                code = Operator(tvals[(state >> 2) % 8], tvals[(state >> 3) % 8],
                                (TokenType)((state >> 20) % 100), &out);
                break;
            case 11:
                code = ValmemMember(tvals[(state >> 2) % 8],
                                    ((state >> 22) & 1) != 0 ? "toString" : "neznami", &out);
                break;
            case 12:
                code = ConditionType(tvals[(state >> 2) % 8]);
                break;
            case 13:
                code = Return(((state >> 1) & 1) != 0, tvals[(state >> 2) % 8]);
                break;
            case 14:
                code = StatementValueType(tvals[(state >> 1) % 8]);
                break;
            case 15:
                code = CheckMain();
                break;
        }

        if (!fuzzCodeOk(code))
            codesOk = 0;
    }

    check("fuzz 5000 akcii bez padu", noCrash, 1);
    check("fuzz vsetky kody v povolenom rozsahu", codesOk, 1);
    SemAnalysisFree();
    endSection();
}

/* ================================================================== */
/* 19. Dopredne volania a inferencne prechody                          */
/*                                                                     */
/* Modeluje presne to, co bude robit parser po napojeni:               */
/*   1. hlavicky vsetkych funkcii (FunctionHeader)                     */
/*   2. SemInferMode(true) a opakovane prechody, kym rastie            */
/*      DefinedFunctionCount; telo, ktore vrati 98, sa zahodi           */
/*   3. ResetSemAnalysisForFinalAnalysis()                             */
/*   4. ostry prechod, v ktorom je 98 uz chyba 6                       */
/*                                                                     */
/* Sekcia vygenerovana AI (zadanie, sekcia 4).                         */
/* Prompt: "napíš testy, ktoré modelujú viacprechodové inferenčné      */
/* správanie parsera nad semantickým API vrátane abortu, resetu a      */
/* ostrého prechodu"                                                   */
/* ================================================================== */

enum SimBodyKind {
    BODY_INT_LITERAL,
    BODY_DOUBLE_LITERAL,
    BODY_BOOL_LITERAL,
    BODY_CALL,
    BODY_CALL_INT_ARG,
    BODY_CALL_PLUS_INT,
    BODY_CALL_PLUS_DOUBLE,
    BODY_CALL_AS_COND,
    BODY_CALL_ARG,
    BODY_RETURN_INT,
    BODY_BLOCK_CALL,
    BODY_MAIN_PRINT
};

typedef struct {
    bool hasResult;
    typeId resultType;
} SimResult;

typedef struct {
    const char *name;
    const char *callee;
    int kind;
    const char **paramNames;
    const typeId *paramTypes;
    int paramCount;
    bool isMain;
} SimFunc;

static int simPasses;

static int SimRunBody(const SimFunc *func, SimResult *out) {
    typeId t = TYPE_UNDEFINED;
    typeId arg = TYPE_UNDEFINED;
    typeId intArg = TYPE_INT;
    int r;

    out->hasResult = false;
    out->resultType = TYPE_UNDEFINED;

    switch (func->kind) {
        case BODY_INT_LITERAL:
            out->hasResult = true;
            out->resultType = TYPE_INT;
            return 0;
        case BODY_DOUBLE_LITERAL:
            out->hasResult = true;
            out->resultType = TYPE_DOUBLE;
            return 0;
        case BODY_BOOL_LITERAL:
            out->hasResult = true;
            out->resultType = TYPE_BOOLEAN;
            return 0;
        case BODY_CALL:
            r = Call(func->callee, 0, NULL, &t);
            break;
        case BODY_CALL_INT_ARG:
            r = Call(func->callee, 1, &intArg, &t);
            break;
        case BODY_CALL_PLUS_INT:
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            r = Operator(t, TYPE_INT, TOKEN_PLUS, &t);
            break;
        case BODY_CALL_PLUS_DOUBLE:
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            r = Operator(t, TYPE_DOUBLE, TOKEN_PLUS, &t);
            break;
        case BODY_CALL_AS_COND:
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            return ConditionType(t);
        case BODY_CALL_ARG:
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            arg = t;
            r = Call("g", 1, &arg, &t);
            break;
        case BODY_RETURN_INT:
            return Return(true, TYPE_INT);
        case BODY_BLOCK_CALL:
            r = EnterBlock();
            if (r != 0)
                return r;
            r = DeclareVar("tmp", true, TYPE_INT, false, TYPE_INT);
            if (r != 0)
                return r;
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            r = LeaveBlock();
            break;
        case BODY_MAIN_PRINT:
            r = Call(func->callee, 0, NULL, &t);
            if (r != 0)
                return r;
            arg = t;
            r = Call("toStr", 1, &arg, &t);
            if (r != 0)
                return r;
            arg = t;
            r = Call("print", 1, &arg, &t);
            break;
        default:
            return 99;
    }
    if (r != 0)
        return r;
    out->hasResult = true;
    out->resultType = t;
    return 0;
}

/* Simuluje presne poradie volani parsera: inferencne prechody, kym
   DefinedFunctionCount rastie, potom reset a ostry prechod. */
static int SimRun(const SimFunc *funcs, int count) {
    int r;

    SemInferMode(true);
    simPasses = 0;
    for (;;) {
        int before = DefinedFunctionCount();
        simPasses++;
        for (int i = 0; i < count; i++) {
            const SimFunc *f = &funcs[i];
            if (FunctionHasType(f->name))
                continue;
            r = FuncBodyStart(f->name, f->paramNames, f->paramTypes, f->paramCount, f->isMain);
            if (r != 0)
                return r;
            SimResult out;
            r = SimRunBody(f, &out);
            if (r != 0) {
                r = AbortFuncBody();
                if (r != 0)
                    return r;
                continue;
            }
            r = FuncBodyEnd(out.hasResult, out.resultType);
            if (r != 0)
                continue; /* inferencny prechod chybu tela nenahlasuje */
        }
        if (DefinedFunctionCount() == before)
            break;
    }

    r = ResetSemAnalysisForFinalAnalysis();
    if (r != 0)
        return r;

    for (int i = 0; i < count; i++) {
        const SimFunc *f = &funcs[i];
        r = FuncBodyStart(f->name, f->paramNames, f->paramTypes, f->paramCount, f->isMain);
        if (r != 0)
            return r;
        SimResult out;
        r = SimRunBody(f, &out);
        if (r != 0) {
            AbortFuncBody();
            return r;
        }
        r = FuncBodyEnd(out.hasResult, out.resultType);
        if (r != 0) {
            AbortFuncBody();
            return r;
        }
    }
    return 0;
}

static void test_forward_inference(void) {
    static const char *paramN[] = {"n"};
    static const typeId paramInt[] = {TYPE_INT};

    beginSection("19. dopredne volania a inferencne prechody");

    /* S0: bez SemInferMode je neznamy navratovy typ hned chyba 6 */
    fresh();
    check("S0 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S0 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S0 start a", FuncBodyStart("a", NULL, NULL, 0, false), 0);
    checkCall("S0 call b bez inferencie", "b", 0, NULL, 6, TYPE_UNDEFINED);
    check("S0 abort", AbortFuncBody(), 0);

    /* S1: a -> b, b = 5; tri prechody (posledny uz bez pokroku) */
    fresh();
    check("S1 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S1 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S1 count na zaciatku", DefinedFunctionCount(), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S1 vysledok", SimRun(prog, 2), 0);
        check("S1 pocet prechodov", simPasses, 3);
    }
    check("S1 a ma typ", FunctionHasType("a"), true);
    checkCall("S1 typ a", "a", 0, NULL, 0, TYPE_INT);
    check("S1 reset je idempotentny", ResetSemAnalysisForFinalAnalysis(), 0);
    check("S1 count po druhom resete", DefinedFunctionCount(), 2);

    /* S2: retaz a -> b -> c; styri prechody */
    fresh();
    check("S2 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S2 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S2 header c", FunctionHeader("c", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL, NULL, NULL, 0, false},
            {"b", "c", BODY_CALL, NULL, NULL, 0, false},
            {"c", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S2 vysledok", SimRun(prog, 3), 0);
        check("S2 pocet prechodov", simPasses, 4);
    }
    checkCall("S2 typ a", "a", 0, NULL, 0, TYPE_INT);
    checkCall("S2 typ b", "b", 0, NULL, 0, TYPE_INT);

    /* S3: dopredny call v aritmetike, Int a Double */
    fresh();
    check("S3 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S3 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_PLUS_INT, NULL, NULL, 0, false},
            {"b", NULL, BODY_DOUBLE_LITERAL, NULL, NULL, 0, false},
        };
        check("S3 vysledok", SimRun(prog, 2), 0);
    }
    checkCall("S3 a zvladne Int + Double", "a", 0, NULL, 0, TYPE_DOUBLE);

    /* S4: dopredny call ako podmienka; platny aj neplatny pripad */
    fresh();
    check("S4 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S4 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_AS_COND, NULL, NULL, 0, false},
            {"b", NULL, BODY_BOOL_LITERAL, NULL, NULL, 0, false},
        };
        check("S4 podmienka z dopredneho callu", SimRun(prog, 2), 0);
    }
    checkCall("S4 a je Unit", "a", 0, NULL, 0, TYPE_UNIT);

    fresh();
    check("S4b header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S4b header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_AS_COND, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S4b Int v podmienke = 6", SimRun(prog, 2), 6);
    }

    /* S5: dopredny call ako argument; g(Int) je explicitna */
    fresh();
    check("S5 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S5 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S5 header g", FunctionHeader("g", true, TYPE_INT, paramInt, 1, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_ARG, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
            {"g", NULL, BODY_INT_LITERAL, paramN, paramInt, 1, false},
        };
        check("S5 vysledok", SimRun(prog, 3), 0);
    }
    checkCall("S5 a ma Int", "a", 0, NULL, 0, TYPE_INT);

    /* S6: rekurzia bez typu nikdy neziska typ -> 6 */
    fresh();
    check("S6 header f", FunctionHeader("f", false, TYPE_UNDEFINED, paramInt, 1, false), 0);
    {
        const SimFunc prog[] = {
            {"f", "f", BODY_CALL_INT_ARG, paramN, paramInt, 1, false},
        };
        check("S6 rekurzia = 6", SimRun(prog, 1), 6);
        check("S6 iba jeden prechod", simPasses, 1);
    }
    check("S6 f nema typ", FunctionHasType("f"), false);

    /* S7: vzajomna rekurzia bez typov -> 6 */
    fresh();
    check("S7 header x", FunctionHeader("x", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S7 header y", FunctionHeader("y", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"x", "y", BODY_CALL, NULL, NULL, 0, false},
            {"y", "x", BODY_CALL, NULL, NULL, 0, false},
        };
        check("S7 vzajomna rekurzia = 6", SimRun(prog, 2), 6);
        check("S7 iba jeden prechod", simPasses, 1);
    }

    /* S8: rekurzia s explicitnym typom je v poriadku */
    fresh();
    check("S8 header f", FunctionHeader("f", true, TYPE_INT, paramInt, 1, false), 0);
    {
        const SimFunc prog[] = {
            {"f", "f", BODY_CALL_INT_ARG, paramN, paramInt, 1, false},
        };
        check("S8 explicitna rekurzia = 0", SimRun(prog, 1), 0);
    }
    checkCall("S8 typ f", "f", 1, paramInt, 0, TYPE_INT);

    /* S9: explicitna funkcia vola neskor definovanu inferovanu */
    fresh();
    check("S9 header a", FunctionHeader("a", true, TYPE_INT, NULL, 0, false), 0);
    check("S9 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S9 a ma typ uz z hlavicky", FunctionHasType("a"), true);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S9 vysledok", SimRun(prog, 2), 0);
        check("S9 dva prechody", simPasses, 2);
    }

    /* S10: dopredny call na neexistujucu funkciu */
    fresh();
    check("S10 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "nope", BODY_CALL, NULL, NULL, 0, false},
        };
        check("S10 neexistujuca funkcia = 3", SimRun(prog, 1), 3);
    }

    /* S11: dopredny call so zlym poctom argumentov */
    fresh();
    check("S11 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S11 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_INT_ARG, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S11 zly pocet argumentov = 5", SimRun(prog, 2), 5);
    }

    /* S12: return v inferovanej funkcii nema co overovat -> 6 */
    fresh();
    check("S12 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", NULL, BODY_RETURN_INT, NULL, NULL, 0, false},
        };
        check("S12 return v inferovanej = 6", SimRun(prog, 1), 6);
    }

    /* S13: return v explicitnej funkcii je v poriadku */
    fresh();
    check("S13 header a", FunctionHeader("a", true, TYPE_INT, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", NULL, BODY_RETURN_INT, NULL, NULL, 0, false},
        };
        check("S13 return v explicitnej = 0", SimRun(prog, 1), 0);
    }

    /* S14: main tlaci vysledok neskor definovanej funkcie */
    fresh();
    check("S14 header main", FunctionHeader("main", true, TYPE_UNIT, NULL, 0, true), 0);
    check("S14 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"main", "a", BODY_MAIN_PRINT, NULL, NULL, 0, true},
            {"a", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S14 main s doprednym callom", SimRun(prog, 2), 0);
    }
    check("S14 CheckMain", CheckMain(), 0);

    /* S15: abort musi zavriet aj vnoreny blok, nie len telo */
    fresh();
    check("S15 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S15 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_BLOCK_CALL, NULL, NULL, 0, false},
            {"b", NULL, BODY_INT_LITERAL, NULL, NULL, 0, false},
        };
        check("S15 blok v inferovanom tele", SimRun(prog, 2), 0);
        check("S15 tri prechody", simPasses, 3);
    }
    checkCall("S15 typ a", "a", 0, NULL, 0, TYPE_INT);

    /* S16: abort s dvoma otvorenymi blokmi a premennou vnutri */
    fresh();
    check("S16 header g", FunctionHeader("g", true, TYPE_UNIT, NULL, 0, false), 0);
    SemInferMode(true);
    check("S16 start g", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("S16 blok 1", EnterBlock(), 0);
    check("S16 blok 2", EnterBlock(), 0);
    check("S16 premenna tmp", DeclareVar("tmp", true, TYPE_INT, false, TYPE_INT), 0);
    check("S16 abort", AbortFuncBody(), 0);
    checkVar("S16 tmp po aborte neexistuje", "tmp", 3, TYPE_UNDEFINED);
    check("S16 start po aborte", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("S16 end po druhom starte", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    SemInferMode(false);

    /* S17: abort bez otvoreneho tela je bezpecny */
    fresh();
    check("S17 abort bez tela", AbortFuncBody(), 0);

    /* S18: to iste telo sa v inferenci smie spustit znova, v ostrom nie */
    fresh();
    check("S18 header g", FunctionHeader("g", true, TYPE_UNIT, NULL, 0, false), 0);
    SemInferMode(true);
    check("S18 start 1", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("S18 end 1", FuncBodyEnd(false, TYPE_UNDEFINED), 0);
    check("S18 start 2 v inferenci", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("S18 abort", AbortFuncBody(), 0);
    SemInferMode(false);
    check("S18 start v ostrom bez resetu = 99", FuncBodyStart("g", NULL, NULL, 0, false), 99);
    check("S18 reset", ResetSemAnalysisForFinalAnalysis(), 0);
    check("S18 start po resete", FuncBodyStart("g", NULL, NULL, 0, false), 0);
    check("S18 end po resete", FuncBodyEnd(false, TYPE_UNDEFINED), 0);

    /* S19: FunctionHasType pre neznamu funkciu */
    fresh();
    check("S19 neznama nema typ", FunctionHasType("nic"), false);
    check("S19 header h", FunctionHeader("h", true, TYPE_INT, NULL, 0, false), 0);
    check("S19 h ma typ", FunctionHasType("h"), true);

    /* S20: Double sa pri inferenci neskomoli na Int */
    fresh();
    check("S20 header a", FunctionHeader("a", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    check("S20 header b", FunctionHeader("b", false, TYPE_UNDEFINED, NULL, 0, false), 0);
    {
        const SimFunc prog[] = {
            {"a", "b", BODY_CALL_PLUS_DOUBLE, NULL, NULL, 0, false},
            {"b", NULL, BODY_DOUBLE_LITERAL, NULL, NULL, 0, false},
        };
        check("S20 Double + Double", SimRun(prog, 2), 0);
    }
    checkCall("S20 typ a", "a", 0, NULL, 0, TYPE_DOUBLE);

    SemAnalysisFree();
    endSection();
}

int main(void) {
    test_lifecycle_and_builtins();
    test_function_type();
    test_function_header();
    test_blocks_and_scope();
    test_declare_and_get();
    test_assign();
    test_call();
    test_body_start();
    test_body_end();
    test_return();
    test_operator();
    test_valmem();
    test_condition();
    test_statement();
    test_checkmain();
    test_integration();
    test_api_edges_and_stress();
    test_fuzz_api();
    test_forward_inference();

    SemAnalysisFree();

    printf("\n==============================================\n");
    printf("spolu: %d/%d\n", checks - failures, checks);
    if (failures == 0)
        printf("vsetko preslo\n");
    else
        printf("chyby: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
