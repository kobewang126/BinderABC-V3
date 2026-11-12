CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
userspace: build/parcel_binding_test build/version build/role_test build/manager_test build/mapping_test build/thread_reply_test build/thread_cancel_test build/pending_cancel_test build/server_death build/stale_reply build/mixed_object_test build/fd_failure_test
build/version: userspace/version.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/%: tests/%.c userspace/parcel.c userspace/object_parcel.c include/parcel_binding.h include/parcel.h include/object_parcel.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< userspace/parcel.c userspace/object_parcel.c -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build
