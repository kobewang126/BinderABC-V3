// SPDX-License-Identifier: GPL-2.0-only
#ifndef BINDERABC_LOCAL_BINDER_H
#define BINDERABC_LOCAL_BINDER_H
#include <array>
#include <cstdint>
#include <string>
extern "C" {
#include <parcel.h>
}

namespace binderabc {
using status_t = int;
class Parcel {
public:
    status_t writeU32(uint32_t value);
    status_t readU32(uint32_t *value);
    status_t writeString(const std::string &value);
    status_t readString(std::string *value);
    bool atEnd() const { return position_ == size_; }
private:
    parcel view();
    std::array<unsigned char, 256> data_{};
    size_t size_ = 0, position_ = 0;
};
class IBinder {
public:
    virtual ~IBinder() = default;
    virtual status_t transact(uint32_t code, Parcel &data, Parcel *reply) = 0;
};
class BBinder : public IBinder {
public:
    status_t transact(uint32_t code, Parcel &data, Parcel *reply) final;
protected:
    virtual status_t onTransact(uint32_t code, Parcel &data, Parcel *reply);
};
}
#endif
