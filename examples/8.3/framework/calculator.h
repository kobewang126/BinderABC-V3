// SPDX-License-Identifier: GPL-2.0-only
#ifndef BINDERABC_CALCULATOR_H
#define BINDERABC_CALCULATOR_H
#include "binder.h"
#include <cerrno>

namespace binderabc {
constexpr uint64_t kCalculatorCookie = 123;
constexpr uint32_t kAdd = 1, kSubtract = 2;
constexpr uint32_t kAddService = 1, kGetService = 2;

class Calculator final : public BBinder {
public:
    Calculator() : BBinder(kCalculatorCookie) {}
protected:
    status_t onTransact(uint32_t code, Parcel &data, Parcel *reply) override {
        if (code != kAdd && code != kSubtract) return -ENOSYS;
        uint32_t a, b;
        if (data.objectCount() || data.readU32(&a) || data.readU32(&b) || !data.atEnd())
            return -EINVAL;
        // Keep the defined unsigned wraparound arithmetic of chapter 7.4.
        return reply->writeU32(code == kAdd ? a + b : a - b);
    }
};
inline status_t getCalculator(uint32_t *handle) {
    Parcel query, found;
    int ret = query.writeString("calculator");
    if (ret) return ret;
    ret = IPCThreadState::self()->transact(0, kGetService, query, &found);
    if (ret) return ret;
    parcel_object object{};
    if (found.objectCount() != 1 || found.readObject(&object) || !found.atEnd() ||
        object.type != OBJECT_PARCEL_HANDLE || !object.value) return -EPROTO;
    *handle = static_cast<uint32_t>(object.value);
    return 0;
}
}
#endif
