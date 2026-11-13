// SPDX-License-Identifier: GPL-2.0-only
#ifndef BINDERABC_FRAMEWORK_BINDER_H
#define BINDERABC_FRAMEWORK_BINDER_H

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <mini_binder.h>
extern "C" {
#include <object_parcel.h>
}

namespace binderabc {
using status_t = int; // 0 or a negative errno; business results live in Parcel.

// Own the received bytes before releasing the driver's reply buffer.
// Encoding still uses the C Parcel/object_parcel taught in earlier chapters.
class Parcel {
public:
    status_t writeU32(uint32_t value);
    status_t readU32(uint32_t *value);
    status_t writeString(const std::string &value);
    status_t readString(std::string *value);
    status_t writeLocal(uint64_t cookie);
    status_t writeHandle(uint32_t handle);
    status_t readObject(parcel_object *object);
    bool atEnd() const { return position_ == size_; }
    size_t size() const { return size_; }
    size_t objectCount() const { return count_; }
private:
    object_parcel view();
    status_t writeObject(uint32_t type, uint64_t value);
    std::array<unsigned char, MINI_BINDER_MAX_DATA> data_{};
    std::array<uint32_t, MINI_BINDER_MAX_OBJECTS> offsets_{};
    size_t size_ = 0, position_ = 0, count_ = 0;
};

class IBinder {
public:
    virtual ~IBinder() = default;
    virtual status_t transact(uint32_t code, Parcel &data, Parcel *reply) = 0;
};

class BBinder : public IBinder {
public:
    explicit BBinder(uint64_t cookie) : cookie_(cookie) {}
    status_t transact(uint32_t code, Parcel &data, Parcel *reply) final;
    uint64_t cookie() const { return cookie_; }
protected:
    virtual status_t onTransact(uint32_t code, Parcel &data, Parcel *reply);
private:
    uint64_t cookie_;
};

class ProcessState {
public:
    static std::shared_ptr<ProcessState> self();
    ~ProcessState();
    ProcessState(const ProcessState &) = delete;
    ProcessState &operator=(const ProcessState &) = delete;
    int driverFD() const { return fd_; }
    status_t addObject(const std::shared_ptr<BBinder> &object);
    std::shared_ptr<BBinder> findObject(uint64_t cookie);
private:
    ProcessState();
    int fd_ = -1;
    size_t page_size_ = 0;
    const unsigned char *mapping_ = nullptr;
    std::mutex objects_lock_;
    std::unordered_map<uint64_t, std::shared_ptr<BBinder>> objects_;
};

}
#endif
