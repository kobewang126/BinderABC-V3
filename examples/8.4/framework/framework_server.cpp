// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cstdio>
#include <exception>
using namespace binderabc;

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        auto calculator = std::make_shared<Calculator>();
        auto process = ProcessState::self();
        int ret = process->addObject(calculator);
        if (ret) return 1;
        BpBinder manager(0);
        Parcel registration, reply;
        if (registration.writeString("calculator") ||
            registration.writeLocal(calculator->cookie())) return 1;
        ret = manager.transact(kAddService, registration, &reply);
        if (ret) {
            std::fprintf(stderr, "addService: %d\n", ret);
            return 1;
        }
        std::puts("BBinder calculator registered: LOCAL(cookie=123)");
        ret = IPCThreadState::self()->joinThreadPool();
        std::fprintf(stderr, "joinThreadPool: %d\n", ret);
        return 1;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
