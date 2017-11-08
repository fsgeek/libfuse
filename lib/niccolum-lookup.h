/*
 * Copyright (c) 2017 
 * All rights reserved.
 */

#if !defined(__LOOKUP_TABLE_H__)
#define __LOOKUP_TABLE_H__ (1)

#include <uuid/uuid.h>
#include <stdint.h>

typedef struct lookup_table *lookup_table_t;
typedef uint32_t (*lookup_table_hash_t)(uuid_t Uuid);

lookup_table_t lookup_table_create(unsigned int SizeHint, const char *Name, lookup_table_hash_t Hash);
void lookup_table_destroy(lookup_table_t Table);

int lookup_table_insert(lookup_table_t Table, uuid_t Uuid, void *Object);
int lookup_table_lookup(lookup_table_t Table, uuid_t Uuid, void **Object);
int lookup_table_remove(lookup_table_t Table, uuid_t Uuid);


#endif // __LOOKUP_TABLE_H__


/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
