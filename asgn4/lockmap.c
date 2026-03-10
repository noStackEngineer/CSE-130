/**
*
* Implementation of file: lockmap.h
*
* @author Anthony Reyna
*
*/

#include "lockmap.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct lockmap_entry {
    char *uri;
    rwlock_t *rwlock;

    struct lockmap_entry *next;
} lockmap_entry_t;

typedef struct bucket {
    pthread_mutex_t mutex;
    lockmap_entry_t *head;
} bucket_t;

struct lockmap {
    size_t num_buckets;
    bucket_t *buckets;
};

unsigned long hash_uri(const char *s);

lockmap_t *lockmap_create(size_t num_buckets) {
    lockmap_t *m = malloc(sizeof(lockmap_t));
    m->num_buckets = num_buckets;
    m->buckets = malloc(num_buckets * sizeof(bucket_t));
    for (size_t i = 0; i < num_buckets; i++) {
        pthread_mutex_init(&m->buckets[i].mutex, NULL);
        m->buckets[i].head = NULL;
    }
    return m;
}

void lockmap_destroy(lockmap_t *m) {
    for (size_t i = 0; i < m->num_buckets; ++i) {
        bucket_t *bucket = &m->buckets[i];
        pthread_mutex_lock(&bucket->mutex);
        lockmap_entry_t *entry = bucket->head;
        while (entry) {
            lockmap_entry_t *next = entry->next;
            free(entry->uri);
            rwlock_delete(&entry->rwlock);
            free(entry);
            entry = next;
        }
        pthread_mutex_unlock(&bucket->mutex);
        pthread_mutex_destroy(&bucket->mutex);
    }
    free(m->buckets);
    free(m);
}

rwlock_t *lockmap_get_lock(lockmap_t *m, const char *uri) {
    unsigned long hash = hash_uri(uri);
    size_t index = hash % m->num_buckets;
    bucket_t *bucket = &m->buckets[index];

    // Lock bucket's mutex
    pthread_mutex_lock(&bucket->mutex);

    lockmap_entry_t *curr = bucket->head;
    while (curr != NULL) {
        if ( strcmp(curr->uri, uri) == 0 ) {
            pthread_mutex_unlock(&bucket->mutex);
            return curr->rwlock;
        }
        curr = curr->next;
    }

    // uri wasn't found, creating new entry
    lockmap_entry_t *n = malloc(sizeof(*n));
    if (n == NULL) {
        pthread_mutex_unlock(&bucket->mutex);
        return NULL;
    }

    n->uri = strdup(uri);
    if (n->uri == NULL) {
        free(n);
        pthread_mutex_unlock(&bucket->mutex);
        return NULL;
    }

    n->rwlock = rwlock_new(WRITERS, 0);
    if (n->rwlock == NULL) {
        free(n->uri);
        free(n);
        pthread_mutex_unlock(&bucket->mutex);
        return NULL;
    }

    n->next = bucket->head;
    bucket->head = n;

    pthread_mutex_unlock(&bucket->mutex);
    return n->rwlock;
}

unsigned long hash_uri(const char *s) {
    unsigned long hash = 0;
    const unsigned long p = 31;

    while (*s) {
        hash = hash * p + (unsigned char)(*s++);
    }

    return hash;
}
