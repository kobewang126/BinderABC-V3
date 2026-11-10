CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
userspace: build/version build/manager_test build/local_transfer_test build/multi_server_test build/thread_test
build/version: userspace/version.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/%: tests/%.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build
