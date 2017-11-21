/*
 * Copyright (c) 2017 Tony Mason
 * All rights reserved.
 */

#if !defined(__NICCOLUM_LOOKUP_H__)
#define __NICCOLUM_LOOKUP_H__ (1)

#define _FILE_OFFSET_BITS (64)

#include <fuse_lowlevel.h>
#include <uuid/uuid.h>
#include <stdint.h>


//
// Niccolum must be able to look up by inode number and UUID (the two sources of handles)
//
typedef struct _niccolum_object {
    fuse_ino_t inode;
    uuid_t uuid;
    // TODO: we may need additional data here
} niccolum_object_t;

niccolum_object_t *niccolum_object_lookup_by_ino(fuse_ino_t inode);
niccolum_object_t *niccolum_object_lookup_by_uuid(uuid_t *uuid);
void niccolum_object_release(niccolum_object_t *object);
niccolum_object_t *niccolum_object_create(fuse_ino_t inode, uuid_t *uuid);



#endif // __NICCOLUM_LOOKUP_H__


/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
