// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cstdio>
#include <exception>
using namespace binderabc;

int main() {
    try {
        uint32_t handle;
        int ret = getCalculator(&handle);
        if (ret) {
            std::fprintf(stderr, "getService: %d\n", ret);
            return 1;
        }
        std::printf("BpBinder calculator: handle=%u\n", handle);
        BpBinder service(handle);
        for (uint32_t code : {kAdd, kSubtract}) {
            Parcel args, reply;
            if (args.writeU32(7) || args.writeU32(3)) return 1;
            ret = service.transact(code, args, &reply);
            uint32_t result;
            if (ret || reply.readU32(&result) || !reply.atEnd()) return 1;
            std::printf("BpBinder %s(7, 3) = %u\n", code == kAdd ? "add" : "sub", result);
            if (result != (code == kAdd ? 10U : 4U)) return 1;
        }
        Parcel notice;
        if (notice.writeU32(42) || service.transact(kNotify, notice, nullptr, MINI_TF_ONE_WAY)) return 1;
        std::puts("oneway submitted: 42 (business completion is not acknowledged)");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
