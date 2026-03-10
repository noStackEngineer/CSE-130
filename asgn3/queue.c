/**
 * @File queue.c
 *
 * The implementation file needed to implement for assignment 3.
 *
 * @author Anthony Reyna
 */

#include "queue.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

struct queue {
    void **buffer; // ptr to bounded buffer of void *'s
    size_t capacity; // max size of the queue

    size_t current_size; // number of elements currently in queue
    size_t head; // index of front of queue
    size_t tail; // index of back of queue

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

queue_t *queue_new(int size) {
    if (size <= 0) {
        return NULL;
    }

    // malloc queue container itself
    queue_t *new_q = malloc(sizeof(*new_q));
    if (!new_q) {
        return NULL;
    }

    // malloc bounded buffer
    new_q->buffer = malloc(size * sizeof(void *));
    if (!new_q->buffer) {
        free(new_q);
        return NULL;
    }
    new_q->capacity = (size_t) size;

    new_q->tail = 0;
    new_q->head = 0;
    new_q->current_size = 0;

    pthread_mutex_init(&new_q->mutex, NULL);
    pthread_cond_init(&new_q->not_empty, NULL);
    pthread_cond_init(&new_q->not_full, NULL);

    return new_q;
}

void queue_delete(queue_t **q) {
    if (q == NULL || *q == NULL) {
        return;
    }

    pthread_mutex_destroy(&(*q)->mutex);

    free((*q)->buffer);
    free(*q);
    *q = NULL;

    return;
}

bool queue_push(queue_t *q, void *elem) {
    if (!q) {
        return false;
    }

    // Entering critical section
    pthread_mutex_lock(&q->mutex);

    // if queue is full, wait until it's not
    while (q->current_size == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    // push element onto queue
    q->buffer[q->tail] = elem;

    // update end index (circular) and size
    q->tail = (q->tail + 1) % q->capacity;
    q->current_size++;

    // Since element was pushed, signal that queue is not_empty
    pthread_cond_signal(&q->not_empty);

    // Finished with critical section
    pthread_mutex_unlock(&q->mutex);

    return true;
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    }

    // Entering critical section
    pthread_mutex_lock(&q->mutex);

    // if queue is empty, wait until it's not
    while (q->current_size == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    // pop element from queue
    *elem = q->buffer[q->head];

    // update front index (circular) and size
    q->head = (q->head + 1) % q->capacity;
    q->current_size--;

    // Since element was popped, signal that queue is not_full
    pthread_cond_signal(&q->not_full);

    // Finished with critical section
    pthread_mutex_unlock(&q->mutex);

    return true;
}
