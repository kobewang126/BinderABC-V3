// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <sys/ioctl.h>
using namespace binderabc;
int main() {
    std::shared_ptr<ProcessState> connections[2];
    std::thread threads[2];
    for (unsigned i = 0; i < 2; ++i)
        threads[i] = std::thread([&, i] { connections[i] = ProcessState::self(); });
    for (auto &thread : threads) thread.join();
    auto process = ProcessState::self();
    assert(connections[0] == process && connections[1] == process);
    assert(connections[0]->driverFD() == connections[1]->driverFD());
    uint32_t version;
    assert(::ioctl(process->driverFD(), MINI_BINDER_GET_VERSION, &version) == 0);
    std::printf("two threads: one ProcessState, shared device fd; protocol=%04u\n", version);
    auto object = std::make_shared<Calculator>();
    assert(process->addObject(object) == 0);
    auto found = process->findObject(kCalculatorCookie);
    assert(found == object);
    Parcel data, reply;
    assert(data.writeU32(7) == 0 && data.writeU32(3) == 0);
    assert(found->transact(kAdd, data, &reply) == 0);
    uint32_t result;
    assert(reply.readU32(&result) == 0 && result == 10);
    std::printf("cookie 123 -> local BBinder: add(7, 3) = %u\n", result);
}
