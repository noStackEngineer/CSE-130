.PHONY: all
all: httpserver

OBJS=httpserver.o
DEBUGFLAGS=-g -O0
CFLAGS=-Wall -Wextra -Werror -pedantic
HEADERS=listener_socket.h iowrapper.h debug.h protocol.h

httpserver: $(OBJS) asgn2_helper_funcs.a
	clang $(OBJS) asgn2_helper_funcs.a -o httpserver

%.o: %.c $(HEADERS)
	clang -c $(CFLAGS) $< -o $@

.PHONY: debug
debug: CFLAGS += $(DEBUGFLAGS)
debug: clean all

.PHONY: clean
clean:
	rm -f $(OBJS) httpserver


# Reminder:
# targets : prereqs
# 	commands
