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

// typedef struct queue queue_t;
struct queue {
    void **buffer;      // ptr to bounded buffer of void *'s
    size_t capacity;       // max size of the queue

    size_t current_size;   // number of elements currently in queue
    size_t head;           // index of front of queue
    size_t tail;           // index of back of queue
};

queue_t *queue_new(int size) {
    // malloc queue container itself
    queue_t *new_q = malloc(sizeof(queue_t));

    // malloc bounded buffer
    new_q->buffer = malloc(size * sizeof(void *));
    new_q->capacity = (size_t) size;

    new_q->tail = 0;
    new_q->head = 0;
    new_q->current_size = 0;

    return new_q;
}

void queue_delete(queue_t **q){
    if (q == NULL || *q == NULL) { return; }

    free((*q)->buffer);
    free(*q);
    *q = NULL;

    return;
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) { return false; }

    // TODO :: different check? when back == front?
    if (q->current_size < q->capacity) {
        // push element onto queue
        q->buffer[q->tail] = elem;
        // increment end index (circular)
        q->tail = (q->tail + 1) % q->capacity;
        q->current_size++;
    }
    else { }    // BLOCK: wait until there's space}

    return true;
}


bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) { return false; }

    if (q->current_size > 0) {
        // pop element from queue
        *elem = q->buffer[q->head];
        // increment front index (circular)
        q->head = (q->head + 1) % q->capacity;
        q->current_size--;
    }
    else { }    // BLOCK: wait until there's an element

    return true;
}
