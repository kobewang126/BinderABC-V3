CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
userspace: build/version build/object_demo build/protocol_test build/manager_test build/mapping_test build/stale_reply build/server_death build/looper_test build/looper_demo
build/version: userspace/version.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/object_demo build/looper_demo: build/%: userspace/%.c userspace/lab.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
build/%: tests/%.c userspace/lab.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build
