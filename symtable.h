#ifndef SYMTABLE_H
#define SYMTABLE_H
/**
 *Projekt: Implementace překladače imperativního jazyka IFJ26
 *@file symtable.h
 *@author Michal Caban (xcabanm00)
 *@brief Definicia struktur pre hashtable, protypy funkcii
 */
#include "types.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct tSymbolData {
    typeId type;
    bool isFunction;
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
bool IsRootScope(symTable *table);
bool InsertLocal(symTable *table, const char *key, symbolData data);
bool LookupVar(symTable *table, const char *key, symbolData *out);
bool InsertFunction(symTable *table, const char *key, symbolData data);
bool LookupFunction(symTable *table, const char *key, symbolData *out);
#endif
