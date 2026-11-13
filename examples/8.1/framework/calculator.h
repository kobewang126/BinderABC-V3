// SPDX-License-Identifier: GPL-2.0-only
#ifndef BINDERABC_LOCAL_CALCULATOR_H
#define BINDERABC_LOCAL_CALCULATOR_H
#include "binder.h"
#include <cerrno>
namespace binderabc {
constexpr uint32_t kAdd = 1, kSubtract = 2;
class Calculator final : public BBinder {
protected:
    status_t onTransact(uint32_t code, Parcel &data, Parcel *reply) override {
        if (code != kAdd && code != kSubtract) return -ENOSYS;
        uint32_t a, b;
        if (data.readU32(&a) || data.readU32(&b) || !data.atEnd()) return -EINVAL;
        return reply->writeU32(code == kAdd ? a + b : a - b);
    }
};
}
#endif
