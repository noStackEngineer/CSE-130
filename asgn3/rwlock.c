/**
 * @File rwlock.h
 *
 * The header file that you need to implement for assignment 3.
 *
 * @author Andi Quinn, Mitchell Elliott, and Gurpreet Dhillon.
 */

#include "rwlock.h"
#include <stdint.h>

/** @struct rwlock_t
 *
 *  @brief This typedef renames the struct rwlock.  Your `c` file
 *  should define the variables that you need for your reader/writer
 *  lock.
 */
//typedef struct rwlock rwlock_t;

//typedef enum { READERS, WRITERS, N_WAY } PRIORITY;

/** @brief Dynamically allocates and initializes a new rwlock with
 *         priority p, and, if using N_WAY priority, n.
 *
 *  @param The priority of the rwlock
 *
 *  @param The n value, if using N_WAY priority
 *
 *  @return a pointer to a new rwlock_t
 */

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *new_rw = malloc(sizeof(rwlock_t));
    // TODO :: initialize rwlock variables
    return new_rw;
}

/** @brief Delete your rwlock and free all of its memory.
 *
 *  @param rw the rwlock to be deleted.  Note, you should assign the
 *  passed in pointer to NULL when returning (i.e., you should set *rw
 *  = NULL after deallocation).
 *
 */
void rwlock_delete(rwlock_t **rw) {
    if (rw == NULL || *rw == NULL) { return; }

    // TODO :: free rwlock variables

    free(*rw);
    *rw = NULL;

    return;
}

/** @brief acquire rw for reading
 *
 */
void reader_lock(rwlock_t *rw) {
    return;
}

/** @brief release rw for reading--you can assume that the thread
 * releasing the lock has *already* acquired it for reading.
 *
 */
void reader_unlock(rwlock_t *rw) {
    return;
}

/** @brief acquire rw for writing
 *
 */
void writer_lock(rwlock_t *rw) {
    return;
}

/** @brief release rw for writing--you can assume that the thread
 * releasing the lock has *already* acquired it for writing.
 *
 */
void writer_unlock(rwlock_t *rw) {
    return;
}
