/**
 * Projekt: Implementace překladače imperativního jazyka IFJ26
 *@file symtable.c
 *@author Michal Caban (xcabanm00)
 *@brief ADT implementacia hashovacej tabulky s implicitnym zretazenim za vyuzitia hashovacieho
 *algoritmu DJB + scope pre hashtable, zasobnik tabuliek kde deklaracia
 *ide do vrchnej tabulky a hladanie ide po parentoch
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "symtable.h"

#define STEP 1
#define SYMTABLE_MAX_LOAD_FACTOR 0.65
#define DJB2_INITIAL_HASH 5381
#define DJB2_SHIFT 5

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED, SLOT_DELETED } memSlotState;

typedef struct tSymTableItem {
    char *key;
    memSlotState state;
    symbolData data;
} symTableItem;

typedef enum { LAMBDA, CLASSIC } scopeType;

struct tSymTable {
    symTableItem *items;
    size_t capacity;
    size_t count;
    size_t tombstoneCount;
    struct tSymTable *parent;
    symTable *top;
    scopeType scope;
};

/**
 * @param n cislo ktore sa testuje na prvocislo
 * @return true ak je n prvocislo, inak false
 * @brief pomocna funkcia pre zistovanie prvocisla
 */
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

/**
 * @param n cislo od ktoreho sa hlada najblizsie prvocislo
 * @return najblizsie prvocislo vacsie alebo rovne n
 * @brief pomocna funkcia pre vypocitanie najblizsieho prvocisla
 */
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

/**
 * @param key string ktory sa hashuje
 * @return hash kluca
 * @brief hashovacia funkcia
 */
static unsigned long DJBHash(const char *key) {
    unsigned long hash = DJB2_INITIAL_HASH;
    int c;

    while ((c = *key++) != '\0') {
        hash = ((hash << DJB2_SHIFT) + hash) + c;
    }

    return hash;
}

/**
 * @param table tabulka ktoru resizujeme
 * @return true ak sa resize podaril, false pri zlyhani alokacie
 * @brief ked je tabulka uz velmi zaplnena, v tomto pripade > SYMTABLE_MAX_LOAD_FACTOR (0,65), je
 * potrebne spravit resize
 * aby sme neprisli o konstantnu casovu zlozitost pri vyhladavani a vkladani
 */
static bool ResizeTable(symTable *table) {
    size_t oldCapacity = table->capacity;
    symTableItem *oldItems = table->items;
    // nova kapacita sa vypocita ako dvojnasobok starej, potom sa vyhlada najblizsie prvocislo
    size_t newCapacity = GetNextPrime(oldCapacity * 2);

    symTableItem *newItems = calloc(newCapacity, sizeof(symTableItem));
    if (newItems == NULL) {
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

/**
 * @param key kluc ktory sa kopiruje
 * @return ukazovatel na novu kopiu kluca, NULL pri zlyhani alokacie
 * @brief pomocna funkcia, skopiruje kluc do novej pamate, lebo kluc ma vlastnit tabulka, nei
 * uzivatel
 */
static char *CopyKey(const char *key) {
    size_t len = strlen(key);
    char *copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, key, len + 1);
    return copy;
}

/**
 * @param initCapacity kapacita na ktoru chce uzivatel tabulku nastavit
 * @return ukazovatel na novu tabulku, NULL pri zlyhani alokacie
 * @brief funkcia pre inicializaciu tabulky, zoberie kapacitu na ktoru ju uzivatel chce nastavit
 * a vypocita z nej najblizsie prvocislo, to je nova kapacita
 * tabulka je sama sebe vrcholom zasobnika
 */
symTable *InitTable(size_t initCapacity) {
    size_t primeCapacity = GetNextPrime(initCapacity);
    symTable *table = malloc(sizeof(symTable));
    if (table == NULL) {
        return NULL;
    }
    table->capacity = primeCapacity;
    table->count = 0;
    table->tombstoneCount = 0;
    table->parent = NULL;
    table->top = table;
    table->scope = CLASSIC;

    table->items = calloc(primeCapacity, sizeof(symTableItem));
    if (table->items == NULL) {
        free(table);
        return NULL;
    }

    return table;
}

/**
 * @param table tabulka v ktorej sa hlada
 * @param targetKey kluc ktory sa hlada
 * @return ukazovatel na najdeny prvok alebo NULL ak sa nenajde
 * @brief najde konkretny prvok v tabulke podla kluca
 */
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

/**
 * @param table tabulka v ktorej sa hlada
 * @param key kluc ktory sa hlada
 * @param out ukazovatel kam sa skopiruje hodnota, moze byt NULL
 * @return true ak sa kluc nasiel, inak false
 * @brief najde kluc v konkretnej tabulke a vracia true alebo false
 * a robi kopiu hodnoty do *out ukazovatela
 */
static bool SearchItem(symTable *table, const char *key, symbolData *out) {
    symTableItem *slot = FindSlot(table, key);
    if (slot == NULL)
        return false;
    if (out != NULL)
        *out = slot->data;
    return true;
}

/**
 * @param table tabulka do ktorej sa vklada
 * @param key kluc vkladaneho prvku, tabulka si spravi vlastnu kopiu
 * @param data hodnota ktora sa pod kluc uklada
 * @return true ak sa vlozenie podarilo, inak false
 * @brief vlozi prvok do konkretnej tabulkym vracia true ak sa operacia podarila, inak false
 */
static bool InsertItem(symTable *table, const char *key, symbolData data) {
    // ak uz item v tabulke existuje, len prepise jeho hodnotu
    symTableItem *existing = FindSlot(table, key);
    if (existing != NULL) {
        existing->data = data;
        return true;
    }
    // kontrola zaplnenosti tabulky,
    // ak je zaplnena na viac ako SYMTABLE_MAX_LOAD_FACTOR, je potrebny resize
    // pocitaju sa aj zaslepene indexy, nech sa co najviac udrzi
    // rychlost vyhladavania
    bool isFull =
        (((double)table->count + (double)table->tombstoneCount) / ((double)table->capacity - 1) >
         SYMTABLE_MAX_LOAD_FACTOR);
    bool resizeSuccess = true;

    if (isFull) {
        resizeSuccess = ResizeTable(table);
    }
    if (resizeSuccess == false) {
        return false;
    }
    // vypocita sa index, nesmieme zabudnut na modulo
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
        return false;
    }
    return true;
}

/**
 * @param table tabulka z ktorej sa maze
 * @param key kluc ktory sa ma odstranit
 * @return true ak sa prvok nasiel a odstranil, inak false
 * @brief odstranenie prvku z konkretnej tabulky
 * kluc sa uvolni a slot sa nastavi na DELETED
 */
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

// scope vrstva pre hashtable
// je to zasobnik tabuliek
// nova tabulka ide vzdy na vrch zasobnika
// deklaracia ide vzdy do vrchnej tabulky a hladanie
// ide aj po vsetkych vrstvach kym nenajde
// parser vola tieto funkcie

/**
 * @param table tabulka ktora sa ma uvolnit, moze byt NULL
 * @brief uvolni jednu konkretnu tabulku z pamate aj s jej klucmi
 */
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

/**
 * @param table tabulka v zasobniku
 * @brief postupne uvolni vsetky tabulky od vrchu zasobnika az po korenovu
 */
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

/**
 * @param table tabulka na ktorej zasobniku sa pushuje
 * @return true pri uspechu, false pri zlyhani alokacie
 * @brief funkcia na pushnutie novej tabulky na vrchol zasobnika
 */
bool PushScope(symTable *table) {
    if (table == NULL)
        return false;
    symTable *child = InitTable(SYMTABLE_INITIAL_CAPACITY);
    if (child == NULL)
        return false;
    child->parent = table->top;
    child->scope = CLASSIC;
    table->top = child;
    return true;
}

/**
 * @param table tabulka
 * @return true v pripade uspechu, inak false
 * @brief funkcia pre push scopu pri lambda funkciach
 */
bool PushLambdaScope(symTable *table) {
    if (table == NULL)
        return false;
    symTable *child = InitTable(SYMTABLE_INITIAL_CAPACITY);
    if (child == NULL)
        return false;
    child->parent = table->top;
    child->scope = LAMBDA;
    table->top = child;
    return true;
}

/**
 * @param table tabulka z ktorej zasobnika sa popuje
 * @return true pri uspechu, false ak je vrchol root alebo pri NULL
 * @brief funkcia pre popnutie tabulky z vrcholu zasobnika a uvolni ju
 * korenova tabulka sa neda popnut, aby nezanikol zasobnik
 */
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

/**
 * @param table tabulka na ktorej vrchol sa deklaruje
 * @param key meno deklarovanej premennej
 * @param data udaje o premennej
 * @return true ak sa vlozila, false ak uz v tom scope existuje alebo pri chybe alokacie
 * @brief vlozi deklaraciu do tabulky na vrchu zasobnika
 * ak sa tam uz nachadza, vrati false
 */
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

/**
 * @param table tabulka od ktorej vrchu sa zacina hladat
 * @param key hladany kluc
 * @param out ukazovatel kam sa ulozi najdena hodnota
 * @return true ak sa premenna nasla, inak false
 * @brief vyhladava premennu od vrchu sasobnika po parentoch az kym nenajde prvy a ked je to lambda
 * scope tak sa na konci vyhladavanie zastavi
 */
bool LookupVar(symTable *table, const char *key, symbolData *out) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    symTable *stackTop = table->top;
    while (stackTop->parent != NULL) {
        if (SearchItem(stackTop, key, out))
            return true;
        if (stackTop->scope == LAMBDA)
            break;
        stackTop = stackTop->parent;
    }
    return false;
}
/**
 * @param table root tabulka
 * @param key meno funkcie
 * @param data udaje o funkcii
 * @return true ak sa vlozila, false ak uz existuje alebo pri chybe alokacie
 * @brief vlozi funkciu do korenovej tabulky, ak uz funkcia existuje, vrati false
 */
bool InsertFunction(symTable *table, const char *key, symbolData data) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    if (FindSlot(table, key))
        return false;
    return InsertItem(table, key, data);
}
/**
 * @param table root tabulka
 * @param key meno hladanej funkcie
 * @param out ukazovatel kam sa ulozi najdena hodnota
 * @return true ak sa funkcia nasla, inak false
 * @brief hlada konkretnu funkciu ale iba v korenovej tabulke
 */
bool LookupFunction(symTable *table, const char *key, symbolData *out) {
    if (table == NULL)
        return false;
    if (key == NULL)
        return false;
    return SearchItem(table, key, out);
}

/**
 * @return true, ak je najvyssia tabulka zaroven aj root alebo este neexistuje
 *  false ak najvyssia tabulka nie je zaroven aj root tabulkou
 * @brief funkcia na zistenie ci je dana tabulka zaroven root tabulkou
 */
bool IsRootScope(symTable *table) {
    if (table == NULL)
        return true;
    if (table->top == table)
        return true;
    return false;
}
