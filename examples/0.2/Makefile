CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all clean
all: build/p02-syscalls

build:
	mkdir -p $@

build/p02-syscalls: syscalls.c | build
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf build
