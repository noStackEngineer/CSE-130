/**
 * @File rwlock.h
 *
 * The implementation file needed to implement for assignment 3.
 *
 * @author Anthony Reyna
 */

#include "rwlock.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

struct rwlock {
    PRIORITY p;
    uint32_t n_way_max;
    uint32_t n_remaining;

    uint32_t waitingReaders;
    uint32_t waitingWriters;
    uint32_t activeReaders;
    uint32_t activeWriters;

    pthread_mutex_t mutex;
    pthread_cond_t read_go;
    pthread_cond_t write_go;
};

//typedef enum { READERS, WRITERS, N_WAY } PRIORITY;

/*

    READERS:
    allow readers to proceed before writers

    WRITERS:
    allow writers to proceed before readers

    N_WAY:
    allow N readers proceed between each writer

    - if no writer threads locking/waiting, then no readers should be waiting
    - if no reader threads locking/waiting, then no writers should be waiting

    - otherwise, allow min(waiting_readers, N) readers between writer_lock_1 and writer_lock_2


*/


rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *rw = malloc(sizeof(*rw));

    // Set priority: READERS, WRITERS, or N_WAY
    rw->p = p;
    rw->n_way_max   = (p == N_WAY) ? n : 0;
    rw->n_remaining = (p == N_WAY) ? n : 0;

    rw->waitingReaders = 0;
    rw->waitingWriters = 0;
    rw->activeReaders = 0;
    rw->activeWriters = 0;

    pthread_mutex_init(&rw->mutex, NULL);
    pthread_cond_init(&rw->read_go, NULL);
    pthread_cond_init(&rw->write_go, NULL);

    return rw;
}

void rwlock_delete(rwlock_t **rw) {
    if (rw == NULL || *rw == NULL) { return; }

    pthread_mutex_destroy(&(*rw)->mutex);

    free(*rw);
    *rw = NULL;

    return;
}




bool readShouldWait(rwlock_t *rw) {
    if (rw->activeWriters > 0) { return true; }

    switch (rw->p) {
        case READERS:
            return false;
        
        case WRITERS:
            return (rw->waitingWriters > 0);

        case N_WAY:
            return (rw->waitingWriters > 0) && (rw->n_remaining == 0);

        default:
            return false;
    }
}


void reader_lock(rwlock_t *rw) {
    if (!rw) { return; }

    // lock the resource and allow access to multiple readers, but NO writers
    pthread_mutex_lock(&rw->mutex);

    rw->waitingReaders++;       
    while(readShouldWait(rw)) {
        pthread_cond_wait(&rw->read_go, &rw->mutex);
    }
    rw->waitingReaders--;

    rw->activeReaders++;
    if (rw->p == N_WAY && rw->waitingWriters > 0 && rw->n_remaining > 0) {
        rw->n_remaining--;
    }

    pthread_mutex_unlock(&rw->mutex);

    return;
}

/** @brief release rw for reading--you can assume that the thread
 * releasing the lock has *already* acquired it for reading.
 */
void reader_unlock(rwlock_t *rw) {
    // when the last reader calls this
    // the lock is released and a writer is given a chance to acquire the lock
    if (!rw) { return; }

    pthread_mutex_lock(&rw->mutex);

    rw->activeReaders--;

    // Last reader checks for which signal to send
    if (rw->activeReaders == 0) {

        // if waiting writers and no more readers allowed
        if (rw->waitingWriters > 0 && ( rw->p == WRITERS || (rw->p == N_WAY && rw->n_remaining == 0))) {
            pthread_cond_signal(&rw->write_go);
        }
        // otherwise, continue with readers
        else if (rw->waitingReaders > 0) {
            pthread_cond_broadcast(&rw->read_go);
        }
        else if (rw->waitingWriters > 0) {
            // No waiting readers, but writers do exist
            pthread_cond_signal(&rw->write_go);
        }
    }

    pthread_mutex_unlock(&rw->mutex);
    
    return;
}

bool writeShouldWait(rwlock_t *rw) {
    if (rw->activeReaders > 0 || rw->activeWriters > 0) { return true; }

    switch (rw->p) {
        case READERS:
            return (rw->waitingReaders > 0);
        
        case WRITERS:
            return false;

        case N_WAY:
            return ( rw->waitingWriters > 0 && rw->waitingReaders > 0 && rw->n_remaining > 0);

        default:
            return false;
    }
}

void writer_lock(rwlock_t *rw) {
    // lock the resource and allow a single writer, NO readers and NO other writers
    if (!rw) { return; }

    pthread_mutex_lock(&rw->mutex);

    rw->waitingWriters++;

    if (rw->p == N_WAY && rw->waitingWriters == 1) {
        rw->n_remaining = rw->n_way_max;
    }

    while(writeShouldWait(rw)) {
        pthread_cond_wait(&rw->write_go, &rw->mutex);
    }

    rw->waitingWriters--;
    rw->activeWriters++;

    pthread_mutex_unlock(&rw->mutex);

    return;
}

void writer_unlock(rwlock_t *rw) {
    // unlock the shared resource and
    // allow a group of one or more readers to take control of lock
    if (!rw) { return; }

    pthread_mutex_lock(&rw->mutex);

    rw->activeWriters--;
    
    if (rw->p == WRITERS) {
        if (rw->waitingWriters > 0) {
            pthread_cond_signal(&rw->write_go);
        } else if (rw->waitingReaders > 0) {
            pthread_cond_broadcast(&rw->read_go);
        }
    }
    
    else if (rw->p == READERS) {
        if (rw->waitingReaders > 0) {
            pthread_cond_broadcast(&rw->read_go);
        } else if (rw->waitingWriters > 0) {
            pthread_cond_signal(&rw->write_go);
        }
    }
    
    else { // N_WAY
        if (rw->waitingReaders > 0) {
            if (rw->waitingWriters > 0) rw->n_remaining = rw->n_way_max;
            pthread_cond_broadcast(&rw->read_go);
        } else if (rw->waitingWriters > 0) {
            // No readers waiting; let next writer proceed
            pthread_cond_signal(&rw->write_go);
        }
    }

    pthread_mutex_unlock(&rw->mutex);
    
    return;
}
