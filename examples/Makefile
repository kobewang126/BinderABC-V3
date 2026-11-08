CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all run-0.1 run-0.2 run-1.1 clean
all: build/p01-call build/p01-fork build/p02-syscalls build/1.1-rpc-pipe

build:
	mkdir -p $@

build/p01-call: 0.1/call.c | build
	$(CC) $(CFLAGS) $< -o $@

build/p01-fork: 0.1/fork.c | build
	$(CC) $(CFLAGS) $< -o $@

build/p02-syscalls: 0.2/syscalls.c | build
	$(CC) $(CFLAGS) $< -o $@

build/1.1-rpc-pipe: 1.1/rpc_pipe.c | build
	$(CC) $(CFLAGS) $< -o $@

run-0.1: build/p01-call build/p01-fork
	./build/p01-call
	./build/p01-fork

run-0.2: build/p02-syscalls
	./build/p02-syscalls

run-1.1: build/1.1-rpc-pipe
	./build/1.1-rpc-pipe

clean:
	rm -rf build
