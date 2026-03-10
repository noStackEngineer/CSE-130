#ifndef LOCKMAP_H
#define LOCKMAP_H

#include "rwlock.h"
#include <stddef.h>

typedef struct lockmap lockmap_t;

lockmap_t *lockmap_create(size_t num_buckets);
void lockmap_destroy(lockmap_t *m);

/* Returns a stable pointer to the rwlock for this URI. */
rwlock_t *lockmap_get_lock(lockmap_t *m, const char *uri);


#endif /* LOCKMAP_H */
