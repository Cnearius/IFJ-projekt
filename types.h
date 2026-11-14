/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 * @file types.h
 * @author Michal Caban (xcabanm00)
 * @brief deklaracia typov s ktorymi musi pracovat viacero vrstiev 
 * */

#ifndef TYPES_H
#define TYPES_H

typedef enum {
    TYPE_UNDEFINED = 0,
    TYPE_INT = 1,
    TYPE_DOUBLE = 2,
    TYPE_STRING = 3,
    TYPE_BOOLEAN = 4,
    TYPE_UNIT = 5,
    TYPE_ANY = 6
} itemType;

typedef int typeId;

#endif
