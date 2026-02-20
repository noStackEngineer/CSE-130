// Basic test to show that the queue API works in non-concurrent settings.
// By: Andi Quinn

#include "queue.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  queue_t *q = queue_new(10);
  if (q == NULL) {
    printf("failed to allocate?");
    return 1;
  }

  // push a 1
  queue_push(q, (void *)1);

  // expect to pop a 1
  uintptr_t r;
  queue_pop(q, (void **)&r);
  if (r != 1) {
    // if not, then we failed
    return 1;
  }

  // push a 0
  queue_push(q, (void *)0);

  // expect to pop a 0
  queue_pop(q, (void **)&r);
  if (r != 0) {
    // if not, then we failed
    return 1;
  }

  // push values 0,...,9
  for (int64_t i = 0; i < 10; ++i) {
    queue_push(q, (void *)i);
  }

  // expect to pop values 0,...,9
  for (int64_t i = 0; i < 10; ++i) {
    queue_pop(q, (void **)&r);
    if (r != i) {
      // if not, then we failed
      return 1;
    }
  }

  queue_delete(&q);
  return 0;
}
