#include "rwlock.h"

#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  rwlock_t *rw = rwlock_new(READERS, -1);

  if (rw == NULL) {
    return 1;
  }

  reader_lock(rw);
  printf("I got a reader lock!");
  reader_unlock(rw);

  writer_lock(rw);
  printf("I got a writer lock!");
  writer_unlock(rw);

  rwlock_delete(&rw);
  return 0;
}
