/**
@file symtable.c
@author Michal Caban (xcabanm00)
@brief ADT implementacia hashovacej tabulky s implicitnym zretazenim za vyuzitia hashovacieho
algoritmu DJB + scope pre hashtable, zasobnik tabuliek kde deklaracia
ide do vrchnej tabulky a hladanie ide po parentoch
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtable.h"

#define STEP 1

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED, SLOT_DELETED } memSlotState;

typedef struct tSymTableItem {
    char *key;
    memSlotState state;
    symbolData data;
} symTableItem;

struct tSymTable {
    symTableItem *items;
    size_t capacity;
    size_t count;
    size_t tombstoneCount;
    struct tSymTable *parent;
    symTable *top;
};

// pomocna funkcia pre zistovanie prvocisla
static bool IsPrime(unsigned long n) {
    if (n <= 1)
        return false;
    if (n <= 3)
        return true;
    // kazde prvocislo vacsie ako 3 ma tvar 6k +- 1
    if (n % 2 == 0 || n % 3 == 0)
        return false;

    // pre kaze cislo, ak nenajdeme delitela pre toto cislo v prvych sqrt(cislo) cislach, je to
    // prvocislo zaciname na prvom prvocisle vacsom ako 3, cize 6k +-1 je aplikovatelne, potom
    // skaceme po 6tkach
    for (size_t i = 5; n >= i * i; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

// pomocna funkcia pre vypocitanie najblizsieho prvocisla
static size_t GetNextPrime(size_t n) {
    if (n <= 2)
        return 2;
    // ak je cislo parne, posuvame sa dalej
    if (n % 2 == 0)
        n++;

    while (!IsPrime(n)) {
        n += 2;
    }
    return n;
}

// hashovacia funkcia
static unsigned long DJBHash(const char *key) {
    unsigned long hash = 5381;
    int c;

    while ((c = *key++) != '\0') {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

// ked je tabulka uz velmi zaplnena, v tomto pripade > 0.65, je potrebne spravit resize 
// aby sme neprisli o konstantnu casovu zlozitost pri vyhladavani a vkladani
static bool ResizeTable(symTable *table) {
    size_t oldCapacity = table->capacity;
    symTableItem *oldItems = table->items;
    // nova kapacita sa vypocita ako dvojnasobok starej, potom sa vyhlada najblizsie prvocislo
    size_t newCapacity = GetNextPrime(oldCapacity * 2);

    symTableItem *newItems = calloc(newCapacity, sizeof(symTableItem));
    if (newItems == NULL) {
        fprintf(stderr, "Error while reallocating memory for table items\n");
        return false;
    }
    table->items = newItems;
    table->capacity = newCapacity;
    table->count = 0;
    table->tombstoneCount = 0;

    for (size_t i = 0; i < oldCapacity; i++) {
        if (oldItems[i].state == SLOT_OCCUPIED) {
            size_t idx = DJBHash(oldItems[i].key) % newCapacity;
            while (table->items[idx].state == SLOT_OCCUPIED) {
                idx = (idx + 1) % newCapacity;
            }

            table->items[idx].key = oldItems[i].key;
            table->items[idx].data = oldItems[i].data;
            table->items[idx].state = SLOT_OCCUPIED;
            table->count++;
        }
    }
    free(oldItems);
    return true;
}

// pomocna funkcia, skopiruje kluc do novej pamate, lebo kluc ma vlastnit tabulka, nei uzivatel
static char *CopyKey(const char *key) {
    size_t len = strlen(key);
    char *copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, key, len + 1);
    return copy;
}

// funkcia pre inicializaciu tabulky, zoberie kapacitu na ktoru ju uzivatel chce nastavit
// a vypocita z nej najblizsie prvocislo, to je nova kapacita
//tabulka je sama sebe vrcholom zasobnika
symTable *InitTable(size_t initCapacity) {
    size_t primeCapacity = GetNextPrime(initCapacity);
    symTable *table = malloc(sizeof(symTable));
    if (table == NULL) {
        fprintf(stderr, "Error while creating lookup table\n");
        return NULL;
    }
    table->capacity = primeCapacity;
    table->count = 0;
    table->tombstoneCount = 0;
    table->parent = NULL;
    table->top = table;

    table->items = calloc(primeCapacity, sizeof(symTableItem));
    if (table->items == NULL) {
        fprintf(stderr, "Error while allocating memory for table items\n");
        free(table);
        return NULL;
    }

    return table;
}

//najde konkretny prvok v tabulke podla kluca
//musi byt static, lebo vracia ukazatel na prvok tabulky
//tne musi byt skryty pred uzivatelom
static symTableItem *FindSlot(symTable *table, const char *targetKey) {
    unsigned long hashedKey = DJBHash(targetKey);
    size_t index = hashedKey % table->capacity;
    size_t next = index;

    for (size_t i = 0; i < table->capacity; i++) {
        next = (index + STEP * i) % table->capacity;

        if (table->items[next].state == SLOT_EMPTY)
            return NULL;

        if (table->items[next].state == SLOT_OCCUPIED &&
            strcmp(table->items[next].key, targetKey) == 0) {
            return &table->items[next];
        }
    }

    return NULL;
}

// najde kluc v konkretnej tabulke a vracia true/false 
//a robi kopiu hodnoty do *out ukazovatela
bool SearchItem(symTable *table, const char *key, symbolData *out) {
    symTableItem *slot = FindSlot(table, key);
    if (slot == NULL)
        return false;
    if (out != NULL)
        *out = slot->data;
    return true;
}

// vlozi prvok do konkretnej tabulkym vracia true ak sa operacia podarila, inak false
bool InsertItem(symTable *table, const char *key, symbolData data) {
    //ak uz item v tabulke existuje, len prepise jeho hodnotu
    symTableItem *existing = FindSlot(table, key);
    if (existing != NULL) {
        existing->data = data;
        return true;
    }
    //kontrola zaplnenosti tabulky, 
    //ak je zaplnena na viac ako 0.65, je potrebny resize
    //pocitaju sa aj zaslepene indexy, nech sa co najviac udrzi
    //rychlost vyhladavania
    bool isFull =
        (((double)table->count + (double)table->tombstoneCount) / ((double)table->capacity - 1) >
         0.65);
    bool resizeSuccess = true;

    if (isFull) {
        resizeSuccess = ResizeTable(table);
    }
    if (resizeSuccess == false) {
        fprintf(stderr, "Error while resizing table.");
        return false;
    }
    //vypocita sa index, nesmieme zabudnut na modulo
    size_t index = DJBHash(key) % table->capacity;
    size_t targetIndex = table->capacity;

    for (size_t i = 0; i < table->capacity; i++) {
        size_t next = (index + STEP * i) % table->capacity;
        if (table->items[next].state == SLOT_EMPTY || table->items[next].state == SLOT_DELETED) {
            targetIndex = next;
            break;
        }
    }
    // kluc by idealne mala vlastnit tabulka nie uzivatel
    char *copy = CopyKey(key);
    if (copy == NULL)
        return false;

    if (targetIndex != table->capacity) {
        table->items[targetIndex].key = copy;
        table->items[targetIndex].data = data;
        if (table->items[targetIndex].state == SLOT_DELETED) {
            table->tombstoneCount--;
        }
        table->items[targetIndex].state = SLOT_OCCUPIED;
        table->count++;
    } else {
        fprintf(stderr, "Couldnt find unoccupied space.");
        return false;
    }
    return true;
}

//odstranenie prvku z konkretnej tabulky
//kluc sa uvolni a slot sa nastavi na DELETED
bool DeleteItem(symTable *table, const char *key) {
    symTableItem *item = FindSlot(table, key);
    if (item == NULL) {
        return false;
    }
    free(item->key);
    item->key = NULL;
    item->state = SLOT_DELETED;
    table->count--;
    table->tombstoneCount++;
    return true;
}

//scope vrstva pre hashtable
//je to zasobnik tabuliek
//nova tabulka ide vzdy na vrch zasobnika
//deklaracia ide vzdy do vrchnej tabulky a hladanie
//ide aj po vsetkych vrstvach kym nenajde
//parser vola tieto funkcie

//uvolni jednu konkretnu tabulku z pamate aj s jej klucmi
static void FreeSingleTable(symTable *table) {
    if (table == NULL)
        return;
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->items[i].state == SLOT_OCCUPIED && table->items[i].key != NULL) {
            free(table->items[i].key);
        }
    }

    free(table->items);
    free(table);
}

// postupne uvolni vsetky tabulky od vrchu zasobnika az po korenovu
void FreeAllTables(symTable *table) {
    if (table == NULL)
        return;
    symTable *currTableToDelete = table->top;
    while (currTableToDelete != NULL) {
        symTable *next = currTableToDelete->parent;
        FreeSingleTable(currTableToDelete);
        currTableToDelete = next;
    }
}

// funkcia na pushnutie novej tabulky na vrchol zasobnika
bool PushScope(symTable *table) {
    if (table == NULL)
        return false;
    symTable *child = InitTable(23);
    if (child == NULL)
        return false;
    child->parent = table->top;
    table->top = child;
    return true;
}

//funkcia pre popnutie tabulky z vrcholu zasobnika a uvolni ju
//korenova tabulka sa neda popnut, aby nezanikol zasobnik
bool PopScope(symTable *table) {
    if (table == NULL)
        return false;
    symTable *popped = table->top;
    if (popped == NULL || popped->parent == NULL)
        return false;
    table->top = popped->parent;
    FreeSingleTable(popped);
    return true;
}

// vlozi deklaraciu do tabulky na vrchu zasobnika
//ak sa tam uz nachadza, vrati false
bool InsertLocal(symTable *table, const char *key, symbolData data) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    symTable *stackTop = table->top;
    symTableItem *itemFound = FindSlot(stackTop, key);
    if (itemFound != NULL)
        return false;
    return InsertItem(stackTop, key, data);
}

//vyhladava premennu od vrchu sasobnika po parentoch az kym nenajde prvy
bool LookupVar(symTable *table, const char *key, symbolData *out) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    symTable *stackTop = table->top;
    while (stackTop->parent != NULL) {
        if (SearchItem(stackTop, key, out))
            return true;
        stackTop = stackTop->parent;
    }
    return false;
}
//vlozi funkciu do korenovej tabulky, ak uz funkcia existuje, vrati false
bool InsertFunction(symTable *table, const char *key, symbolData data) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    if (FindSlot(table, key))
        return false;
    return InsertItem(table, key, data);
}
//hlada konkretnu funkciu ale iba v korenovej tabulke (inde sa ani nemoze nachadzat)
bool LookupFunction(symTable *table, const char *key, symbolData *out) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    return SearchItem(table, key, out);
}
