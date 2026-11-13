// SPDX-License-Identifier: GPL-2.0-only
#include "binder.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace binderabc {
object_parcel Parcel::view() {
    object_parcel p{};
    object_parcel_init(&p, data_.data(), data_.size());
    p.bytes.size = size_;
    p.bytes.position = position_;
    p.count = count_;
    std::memcpy(p.offsets, offsets_.data(), count_ * sizeof(uint32_t));
    return p;
}
status_t Parcel::writeU32(uint32_t value) {
    auto p = view();
    if (parcel_write_u32(&p.bytes, value)) return -ENOSPC;
    size_ = p.bytes.size;
    return 0;
}
status_t Parcel::readU32(uint32_t *value) {
    if (!value) return -EINVAL;
    auto p = view();
    if (parcel_read_u32(&p.bytes, value)) return -EINVAL;
    position_ = p.bytes.position;
    return 0;
}
status_t Parcel::writeString(const std::string &value) {
    auto p = view();
    if (parcel_write_string(&p.bytes, value.data(), value.size())) return -ENOSPC;
    size_ = p.bytes.size;
    return 0;
}
status_t Parcel::readString(std::string *value) {
    if (!value) return -EINVAL;
    auto p = view();
    const unsigned char *text;
    size_t length;
    if (parcel_read_string(&p.bytes, &text, &length)) return -EINVAL;
    value->assign(reinterpret_cast<const char *>(text), length);
    position_ = p.bytes.position;
    return 0;
}
status_t Parcel::writeObject(uint32_t type, uint64_t value) {
    auto p = view();
    int ret = object_parcel_write(&p, type, value);
    if (ret) return ret;
    size_ = p.bytes.size;
    count_ = p.count;
    std::memcpy(offsets_.data(), p.offsets, count_ * sizeof(uint32_t));
    return 0;
}
status_t Parcel::writeLocal(uint64_t cookie) {
    if (!cookie) return -EINVAL;
    return writeObject(OBJECT_PARCEL_LOCAL, cookie);
}
status_t Parcel::writeHandle(uint32_t handle) {
    if (!handle) return -EINVAL;
    return writeObject(OBJECT_PARCEL_HANDLE, handle);
}
status_t Parcel::readObject(parcel_object *object) {
    if (!object) return -EINVAL;
    auto p = view();
    int ret = object_parcel_read_next(&p, object);
    if (!ret) position_ = p.bytes.position;
    return ret;
}
ProcessState::ProcessState() {
    fd_ = ::open("/dev/mini_binder", O_RDWR | O_CLOEXEC);
    if (fd_ < 0) throw std::system_error(errno, std::generic_category(), "open binder");
    long page = ::sysconf(_SC_PAGESIZE);
    if (page <= 0) { ::close(fd_); throw std::runtime_error("invalid page size"); }
    page_size_ = static_cast<size_t>(page);
    void *address = ::mmap(nullptr, page_size_, PROT_READ, MAP_SHARED, fd_, 0);
    if (address == MAP_FAILED) {
        int error = errno;
        ::close(fd_);
        throw std::system_error(error, std::generic_category(), "mmap binder");
    }
    mapping_ = static_cast<const unsigned char *>(address);
}
ProcessState::~ProcessState() {
    ::munmap(const_cast<unsigned char *>(mapping_), page_size_);
    ::close(fd_);
}
std::shared_ptr<ProcessState> ProcessState::self() {
    // Function-local static initialization is serialized by C++11 and later.
    static auto process = std::shared_ptr<ProcessState>(new ProcessState);
    return process;
}
status_t ProcessState::addObject(const std::shared_ptr<BBinder> &object) {
    if (!object || !object->cookie()) return -EINVAL;
    std::lock_guard<std::mutex> guard(objects_lock_);
    if (!objects_.emplace(object->cookie(), object).second) return -EEXIST;
    return 0;
}
std::shared_ptr<BBinder> ProcessState::findObject(uint64_t cookie) {
    std::lock_guard<std::mutex> guard(objects_lock_);
    auto it = objects_.find(cookie);
    return it == objects_.end() ? nullptr : it->second;
}

status_t BBinder::transact(uint32_t code, Parcel &data, Parcel *reply) {
    if (!reply) return -EINVAL;
    return onTransact(code, data, reply);
}
status_t BBinder::onTransact(uint32_t, Parcel &, Parcel *) { return -ENOSYS; }
}
