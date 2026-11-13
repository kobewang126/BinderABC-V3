// SPDX-License-Identifier: GPL-2.0-only
#include "calculator.h"
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <thread>
using namespace binderabc;

static Parcel arguments(uint32_t a, uint32_t b) {
    Parcel data;
    assert(data.writeU32(a) == 0 && data.writeU32(b) == 0);
    return data;
}
int main() {
    // Local BBinder dispatch uses the same IBinder interface without a driver call.
    Calculator local;
    IBinder &binder = local;
    Parcel request = arguments(7, 3), local_reply;
    assert(binder.transact(kAdd, request, &local_reply) == 0);
    uint32_t result;
    assert(local_reply.readU32(&result) == 0 && result == 10);
    std::puts("local BBinder::transact -> onTransact: 10");
    Parcel local_notice = arguments(7, 3);
    assert(binder.transact(kAdd, local_notice, nullptr, MINI_TF_ONE_WAY) == 0);
    assert(local_notice.atEnd()); // Local dispatch completed on this very thread.
    auto process = ProcessState::self();
    auto registered = std::make_shared<Calculator>();
    assert(process->addObject(registered) == 0);
    assert(process->addObject(registered) == -EEXIST);
    assert(!process->findObject(999));
    uint32_t handle;
    assert(getCalculator(&handle) == 0);
    BpBinder remote(handle);
    Parcel empty, rejected;
    assert(remote.transact(kAdd, empty, nullptr) == -EINVAL);
    assert(remote.transact(kAdd, empty, nullptr, 2) == -EINVAL);
    assert(remote.transact(999, empty, &rejected) == -ENOSYS);
    assert(remote.transact(kAdd, empty, &rejected) == -EINVAL);
    // Save a reply, run another transaction on the same thread, then read the old one.
    Parcel first = arguments(7, 3), saved, second = arguments(9, 4), newer;
    assert(remote.transact(kAdd, first, &saved) == 0);
    assert(remote.transact(kSubtract, second, &newer) == 0);
    assert(saved.readU32(&result) == 0 && result == 10);
    assert(newer.readU32(&result) == 0 && result == 5);
    std::puts("unknown code and truncated Parcel rejected; owned reply survived buffer reuse");
    // Keep both threads alive together so identities cannot be confused with reused addresses.
    std::mutex lock;
    std::condition_variable changed;
    unsigned arrived = 0;
    IPCThreadState *states[2]{};
    std::thread threads[2];
    for (unsigned i = 0; i < 2; ++i) threads[i] = std::thread([&, i] {
        assert(ProcessState::self() == process);
        assert(ProcessState::self()->driverFD() == process->driverFD());
        states[i] = IPCThreadState::self();
        assert(states[i] == IPCThreadState::self());
        {
            std::unique_lock<std::mutex> guard(lock);
            ++arrived; changed.notify_all();
            changed.wait(guard, [&] { return arrived == 2; });
            assert(states[0] != states[1]);
        }
        Parcel data = arguments(20 + i, 3), reply;
        assert(remote.transact(kAdd, data, &reply) == 0);
        uint32_t value;
        assert(reply.readU32(&value) == 0 && value == 23 + i);
    });
    for (auto &thread : threads) thread.join();
    std::puts("two calling threads: same ProcessState/fd, distinct IPCThreadState and correct replies");
    for (unsigned i = 0; i < 12; ++i) {
        std::thread worker([&] {
            Parcel data = arguments(1, 2), reply;
            assert(remote.transact(kAdd, data, &reply) == 0);
        });
        worker.join();
    }
    std::puts("12 successive calling threads: TLS destruction reclaimed kernel thread records");
    // Reuse the previous C encoder's 64-bit LOCAL layout and enforce its object capacity.
    Parcel objects;
    for (unsigned i = 0; i < MINI_BINDER_MAX_OBJECTS; ++i)
        assert(objects.writeLocal(0x1122334455667788ULL) == 0);
    size_t before = objects.size();
    assert(objects.writeHandle(1) < 0 && objects.size() == before);
    parcel_object object{};
    assert(objects.readObject(&object) == 0 && object.value == 0x1122334455667788ULL);
    std::puts("C++ Parcel reused the C object codec, including 64-bit cookies and capacity checks");
}
