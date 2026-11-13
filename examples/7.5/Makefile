CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
PARCEL_SOURCES := userspace/parcel.c userspace/object_parcel.c
PARCEL_HEADERS := include/parcel.h include/object_parcel.h
APP_HEADERS := userspace/lab.h userspace/command.h userspace/service.h include/mini_binder.h
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all module userspace clean
all: module userspace
module:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel modules
userspace: build/oneway_test build/service_exit_test build/version build/object_demo build/protocol_test build/manager_test build/mapping_test build/stale_reply build/server_death build/looper_test build/looper_demo build/servicemanager build/calculator_server build/calculator_client build/write_read_test build/thread_reply_test build/parcel_test build/parcel_binding_test build/service_parcel_test
build/version: userspace/version.c include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/servicemanager build/calculator_server build/calculator_client: build/%: userspace/%.c $(APP_HEADERS) $(PARCEL_SOURCES) $(PARCEL_HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< $(PARCEL_SOURCES) -o $@
build/parcel_test build/parcel_binding_test build/service_parcel_test: build/%: tests/%.c $(APP_HEADERS) $(PARCEL_SOURCES) $(PARCEL_HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< $(PARCEL_SOURCES) -o $@
build/object_demo build/looper_demo: build/%: userspace/%.c userspace/lab.h userspace/command.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude $< -o $@
build/%: tests/%.c userspace/lab.h userspace/command.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR)/kernel clean
	rm -rf build

build/service_exit_test: tests/service_exit_test.c userspace/command.h include/mini_binder.h
	mkdir -p build
	$(CC) $(CFLAGS) -pthread -Iinclude $< -o $@
