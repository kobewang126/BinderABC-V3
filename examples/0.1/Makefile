CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all clean
all: build/p01-call build/p01-fork

build:
	mkdir -p $@

build/p01-call: call.c | build
	$(CC) $(CFLAGS) $< -o $@

build/p01-fork: fork.c | build
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf build
