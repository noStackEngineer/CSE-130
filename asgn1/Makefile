# Single-target Makefile:
#
# List the executable, .o files, and .h files of your project here.
# Each .o file will be build automatically from its corresponding .c file.

EXEC    = memory
O_FILES = memory.o
H_FILES =

CC      = clang
CFLAGS  = -Wall -Wextra -Werror -pedantic
FORMAT  = clang-format
C_FILES = $(O_FILE:%.o=%.c)

.PHONY: all
all: $(EXEC)

$(EXEC): $(O_FILES)
	$(CC) $^ -o $@

%.o : %.c $(H_FILES)
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(EXEC) $(O_FILES)

.PHONY: format
format:
	$(FORMAT) -i -style=file $(C_FILES) $(H_FILES)

