.PHONY: all clean test

all: queue.o

CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic
HEADERS = queue.h


#%.o: %.c $(HEADERS)
#	clang -c $(CFLAGS) $< -o $@


# build object file for API
queue.o: queue.c $(HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@

# build test executable
test: test_queue

test_queue: queue_users/basic_push_pop.c queue.o
	$(CC) -I. $^ -o test_queue

clean:
	rm -f queue.o test_queue