// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cassert>
#include <cstdio>
using namespace binderabc;
int main() {
    Calculator calculator;
    IBinder &object = calculator;
    Parcel empty, reply;
    assert(object.transact(999, empty, &reply) == -ENOSYS);
    assert(object.transact(kAdd, empty, &reply) == -EINVAL);
    assert(object.transact(kAdd, empty, nullptr) == -EINVAL);
    Parcel too_many;
    assert(too_many.writeU32(7) == 0 && too_many.writeU32(3) == 0 && too_many.writeU32(1) == 0);
    assert(object.transact(kAdd, too_many, &reply) == -EINVAL);
    Parcel strings;
    assert(strings.writeString("calculator") == 0);
    std::string name;
    assert(strings.readString(&name) == 0 && name == "calculator" && strings.atEnd());
    assert(strings.readString(&name) == -EINVAL);
    Parcel full;
    for (int i = 0; i < 64; ++i) assert(full.writeU32(i) == 0);
    assert(full.writeU32(64) == -ENOSPC);
    uint32_t first;
    assert(full.readU32(&first) == 0 && first == 0);
    std::puts("local dispatch, malformed arguments and reused C Parcel boundaries passed");
}
