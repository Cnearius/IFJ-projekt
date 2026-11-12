#ifndef SYMTABLE_H
#define SYMTABLE_H
/**
@file symtable.h
@author Michal Caban (xcabanm00)
@brief Definicia struktur pre hashtable, protypy funkcii
*/
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TYPE_UNDEFINED,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOLEAN,
    TYPE_UNIT
} itemType;

typedef struct tSymbolData {
    itemType type;
    bool isFunction; // if false, it is a variable
    int paramCount;
    bool isDefined;
    bool isConst;
} symbolData;

typedef struct tSymTable symTable;

// prototypy funkcii
symTable *InitTable(size_t initCapacity);
bool InsertItem(symTable *table, const char *key, symbolData data);
bool DeleteItem(symTable *table, const char *key);
bool SearchItem(symTable *table, const char *key, symbolData *out);
// scope pre hashtable
void FreeAllTables(symTable *table);
bool PushScope(symTable *table);
bool PopScope(symTable *table);
bool InsertLocal(symTable *table, const char *key, symbolData data);
bool LookupVar(symTable *table, const char *key, symbolData *out);
bool InsertFunction(symTable *table, const char *key, symbolData data);
bool LookupFunction(symTable *table, const char *key, symbolData *out);
#endif
