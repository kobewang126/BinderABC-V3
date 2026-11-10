CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all clean
all: build/fd_demo
build/fd_demo: fd_demo.c
	mkdir -p build
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -rf build
