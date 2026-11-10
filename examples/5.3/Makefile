CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
userspace: build/version build/manager_test build/local_transfer_test build/multi_server_test build/thread_test build/concurrent_test build/server_death build/stale_reply build/pending_cancel_test build/workers_test build/return_to_owner_test build/thread_reply_test build/thread_cancel_test build/server_mapping_isolation_test build/thread_exit_test
build/version: userspace/version.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/%: tests/%.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build
