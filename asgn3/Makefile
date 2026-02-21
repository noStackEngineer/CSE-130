.PHONY: all clean test

all: queue.o rwlock.o

CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic
HEADERS = queue.h rwlock.h


#%.o: %.c $(HEADERS)
#	clang -c $(CFLAGS) $< -o $@


# build object file for Queue API
queue.o: queue.c queue.h
	$(CC) -c $(CFLAGS) $< -o $@

# build object file for RW Lock API
rwlock.o: rwlock.c rwlock.h
	$(CC) -c $(CFLAGS) $< -o $@


# build test executables / not particularly necessary
test: test_queue test_rwlock

test_queue: queue_users/basic_push_pop.c queue.o
	$(CC) -I. $^ -o test_queue

test_rwlock: rwlock_users/basic_lock_unlock.c rwlock.o
	$(CC) -I. $^ -o test_rwlock

clean:
	rm -f queue.o rwlock.o test_queue test_rwlock