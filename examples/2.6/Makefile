CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules

userspace: build/version build/command_test build/server_death build/stale_reply build/mmap_access build/parcel_demo build/parcel_test build/mapped_test build/calc_server build/calc_client build/calc_test

build/command_test: tests/command_test.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@

build/calc_server: userspace/calc_server.c userspace/command.h userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/calc_server.c userspace/parcel.c -o $@

build/calc_client: userspace/calc_client.c userspace/calc_ipc.h userspace/command.h userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/calc_client.c userspace/parcel.c -o $@

build/calc_test: tests/calc_test.c userspace/calc_ipc.h userspace/command.h userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/calc_test.c userspace/parcel.c -o $@

build/mapped_test: tests/mapped_test.c userspace/command.h userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/mapped_test.c userspace/parcel.c -o $@

build/parcel_demo: userspace/parcel_demo.c userspace/parcel.c include/parcel.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/parcel_demo.c userspace/parcel.c -o $@

build/parcel_test: tests/parcel_test.c userspace/parcel.c include/parcel.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/parcel_test.c userspace/parcel.c -o $@

build/%: userspace/%.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@

build/server_death: tests/server_death.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@

build/stale_reply: tests/stale_reply.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@

build/mmap_access: tests/mmap_access.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build
