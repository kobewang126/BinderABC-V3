CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O0 -g
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O0 -g
HEADERS := framework/binder.h framework/calculator.h include/parcel.h
.PHONY: all userspace clean
all: userspace
userspace: build/local_demo build/local_test
build/parcel.o: userspace/parcel.c include/parcel.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@
build/local_demo: userspace/local_demo.cpp framework/binder.cpp $(HEADERS) build/parcel.o
	$(CXX) $(CXXFLAGS) -Iinclude -Iframework $< framework/binder.cpp build/parcel.o -o $@
build/local_test: tests/local_test.cpp framework/binder.cpp $(HEADERS) build/parcel.o
	$(CXX) $(CXXFLAGS) -Iinclude -Iframework $< framework/binder.cpp build/parcel.o -o $@
clean:
	rm -rf build
