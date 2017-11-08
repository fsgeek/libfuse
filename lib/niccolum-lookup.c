/*
 * Copyright (c) 2017 
 * All rights reserved.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lookup-table.h"

#include <string.h>
#include <pthread.h>
// #include <crt/nstime.h>
// #include <crt/logging.h>
// #include <crt/list.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

struct list {
    struct list *forward;
    struct list *back;
};


#if !defined(offset_of)
#define offset_of(type, field) (unsigned long)&(((type *)0)->field)
#endif // offset_of

#if !defined(container_of)
#define container_of(ptr, type, member) ((type *)(((char *)ptr) - offset_of(type, member)))
#endif // container_of

#define list_is_empty(list) ((list)->forward == (list)->back)
#define list_head(list) ((list)->forward)
#define list_remove(item) do { \
    (item)->back->forward = (item)->forward; \
    (item)->forward->back = (item)->back; \
    (item)->forward = (item)->back = NULL; } while (0)

#define list_for_each(_list, _itr)                                      \
    for ((_itr) = list_head((_list));                                   \
         (_itr) != (_list);                                             \
         (_itr) = (_itr)->forward)

#if 0
#define list_insert_tail(list, item) do { \
    (item)->forward = (list); \
    (item)->back = (list)->back; \
    (list)->back->forward = (item); \
    (list)->back = (item); \
} while (0)
#else
static void list_insert_tail(struct list *list, struct list *item);
static void list_insert_tail(struct list *list, struct list *item) {
    item->forward = list;
    item->back = list->back;
    list->back->forward = item;
    list->back = item;
}
#endif // 0
        
struct lookup_table {
    unsigned char EntryCountShift;
    unsigned char Name[7];
    pthread_rwlock_t TableLock;
    lookup_table_hash_t Hash;
    struct list TableBuckets[1];
};

typedef struct lookup_table_entry {
    struct list ListEntry;
    uuid_t       Uuid;
    void       *Object;
} lookup_table_entry_t, *plookup_table_entry_t;

static lookup_table_entry_t *lookup_table_entry_create(uuid_t Uuid, void *Object)
{
    lookup_table_entry_t *new_entry = malloc(sizeof(lookup_table_entry_t));

    while (NULL != new_entry) {
        memcpy(&new_entry->Uuid, Uuid, sizeof(uuid_t));
        new_entry->Object = Object;
        break;
    }

    return new_entry;
}

static void lookup_table_entry_destroy(lookup_table_entry_t *DeadEntry)
{
    free(DeadEntry);
}

static uint32_t default_hash(uuid_t Uuid)
{
    uint32_t hash = ~0;
    const char *blob = (const char *)Uuid;

    for (unsigned char index = 0; index < sizeof(uuid_t); index += sizeof(uint32_t)) {
        hash ^= *(const uint32_t *)&blob[index];
    }

    return hash;
}

static uint32_t lookup_table_hash(lookup_table_t Table, uuid_t Uuid)
{
    return (Table->Hash(Uuid) & ((1<<Table->EntryCountShift)-1));
}

lookup_table_t
lookup_table_create(unsigned int SizeHint, const char *Name, lookup_table_hash_t Hash)
{
    lookup_table_t table = malloc(sizeof(struct lookup_table));
    unsigned char entry_count_shift = 0;
    unsigned entrycount;

    if (SizeHint > 65536) {
        SizeHint = 65536;
    }

    while (((unsigned int)(1<<entry_count_shift)) < SizeHint) {
        entry_count_shift++;
    }

    entrycount = 1 << entry_count_shift;

    table = malloc(offset_of(struct lookup_table, TableBuckets) + (sizeof(struct list) * entrycount));

    while (NULL != table) {
        table->EntryCountShift = entry_count_shift;
        memcpy(table->Name, Name, 7);
        table->Hash = Hash ? Hash : default_hash;
        pthread_rwlock_init(&table->TableLock, NULL);

        for (unsigned index = 0; index < entrycount; index++) {
            table->TableBuckets[index].back = table->TableBuckets[index].forward = &table->TableBuckets[index];
        }
        break;
    }

    return table;
}

void lookup_table_destroy(lookup_table_t Table)
{
    unsigned bucket_index = 0;
    lookup_table_entry_t *table_entry;

    pthread_rwlock_wrlock(&Table->TableLock);

    for (bucket_index = 0; bucket_index < (unsigned) (1<<Table->EntryCountShift); bucket_index++) {
        while (!list_is_empty(&Table->TableBuckets[bucket_index])) {
            table_entry = container_of(list_head(&Table->TableBuckets[bucket_index]), struct lookup_table_entry, ListEntry);
            list_remove(&table_entry->ListEntry);
            lookup_table_entry_destroy(table_entry);
        }
    }

    pthread_rwlock_unlock(&Table->TableLock);

    free(Table);

    return;
}


static struct lookup_table_entry *lookup_table_locked(const lookup_table_t Table, uuid_t Uuid)
{
    uint32_t bucket_index = lookup_table_hash(Table, Uuid);
    struct lookup_table_entry *table_entry = NULL;
    struct list *le;

    list_for_each(&Table->TableBuckets[bucket_index], le) {
        table_entry = container_of(le, struct lookup_table_entry, ListEntry);
        if (0 == uuid_compare(Uuid, table_entry->Uuid)) {
            return table_entry;
        }
    }

    return NULL;

}

int
lookup_table_insert(lookup_table_t Table, uuid_t Uuid, void *Object)
{
    lookup_table_entry_t *entry = lookup_table_entry_create(Uuid, Object);
    int status = ENOMEM;
    uint32_t bucket_index = lookup_table_hash(Table, Uuid);
    struct lookup_table_entry *table_entry = NULL;

    while (NULL != entry) {
        memcpy(&entry->Uuid, Uuid, sizeof(uuid_t));
        entry->Object = Object;

        pthread_rwlock_wrlock(&Table->TableLock);
        table_entry = lookup_table_locked(Table, Uuid);

        if (table_entry) {
            status = EEXIST;
        }
        else {
            list_insert_tail(&Table->TableBuckets[bucket_index], &entry->ListEntry);
            status = 0;
        }
        pthread_rwlock_unlock(&Table->TableLock);

        break;
    }

    if (0 != status) {
        if (NULL != entry) {
            free(entry);
            entry = NULL;
        }
    }

    return status;
}

int
lookup_table_lookup(lookup_table_t Table, uuid_t Uuid, void **Object)
{
    struct lookup_table_entry *entry;

    pthread_rwlock_rdlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Uuid);
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry) {
        *Object = entry->Object;
    }
    else {
        *Object = NULL;
    }

    return NULL == entry ? ENODATA : 0;
}

int lookup_table_remove(lookup_table_t Table, uuid_t Uuid)
{
    struct lookup_table_entry *entry;
    int status = ENODATA;

    pthread_rwlock_wrlock(&Table->TableLock);
    entry = lookup_table_locked(Table, Uuid);

    if (entry) {
        list_remove(&entry->ListEntry);
    }
    pthread_rwlock_unlock(&Table->TableLock);

    if (entry) {
        lookup_table_entry_destroy(entry);
        status = 0;
    }

    return status;

}



/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
