// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cstdio>
using namespace binderabc;
int main() {
    Calculator calculator;
    IBinder &object = calculator;
    for (uint32_t code : {kAdd, kSubtract}) {
        Parcel data, reply;
        if (data.writeU32(7) || data.writeU32(3)) return 1;
        int status = object.transact(code, data, &reply);
        uint32_t result;
        if (status || reply.readU32(&result) || !reply.atEnd()) return 1;
        std::printf("local BBinder %s(7, 3) = %u\n", code == kAdd ? "add" : "sub", result);
        if (result != (code == kAdd ? 10U : 4U)) return 1;
    }
}
