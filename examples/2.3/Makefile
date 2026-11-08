CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules

userspace: build/version build/server_death build/stale_reply build/mmap_access build/parcel_demo build/parcel_test build/transaction_client build/transaction_server build/transaction_test

build/transaction_test: tests/transaction_test.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/transaction_test.c -o $@

build/transaction_client: userspace/transaction_client.c userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/transaction_client.c userspace/parcel.c -o $@

build/transaction_server: userspace/transaction_server.c userspace/parcel.c include/parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude userspace/transaction_server.c userspace/parcel.c -o $@

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
