CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g

.PHONY: all clean
all: build/parcel_demo build/parcel_test

build/parcel_demo: userspace/parcel_demo.c userspace/parcel.c include/parcel.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/parcel_demo.c userspace/parcel.c -o $@

build/parcel_test: tests/parcel_test.c userspace/parcel.c include/parcel.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/parcel_test.c userspace/parcel.c -o $@

clean:
	rm -rf build
