CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all clean
all: build/1.1-rpc-pipe

build:
	mkdir -p $@

build/1.1-rpc-pipe: rpc_pipe.c | build
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf build
