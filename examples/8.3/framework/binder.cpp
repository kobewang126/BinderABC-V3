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
status_t Parcel::importData(const void *data, const mini_binder_transaction_data &tx) {
    if (tx.size > data_.size() || tx.object_count > offsets_.size()) return -EPROTO;
    // Validate offsets using the existing decoder, then own a bounded copy.
    Parcel result;
    if (tx.size) std::memcpy(result.data_.data(), data, tx.size);
    result.size_ = tx.size;
    result.count_ = tx.object_count;
    std::memcpy(result.offsets_.data(), tx.object_offsets, tx.object_count * sizeof(uint32_t));
    auto p = result.view();
    for (size_t i = 0; i < p.count; ++i) {
        parcel_object object{};
        if (object_parcel_read(&p, i, &object)) return -EPROTO;
    }
    *this = result;
    return 0;
}
void Parcel::exportData(mini_binder_transaction_data *tx) const {
    tx->size = size_;
    tx->object_count = count_;
    std::memcpy(tx->data, data_.data(), size_);
    std::memcpy(tx->object_offsets, offsets_.data(), count_ * sizeof(uint32_t));
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

IPCThreadState::IPCThreadState() : process_(ProcessState::self()) {}
IPCThreadState *IPCThreadState::self() {
    static thread_local IPCThreadState thread;
    return &thread;
}
IPCThreadState::~IPCThreadState() {
    // Release the kernel thread record, but leave the shared fd open.
    ::ioctl(process_->driverFD(), MINI_BINDER_THREAD_EXIT);
}
status_t IPCThreadState::talkWithDriver(mini_binder_command *out, mini_binder_command *in) {
    mini_binder_write_read bwr{};
    bwr.write_size = out ? sizeof(*out) : 0;
    bwr.write_buffer = reinterpret_cast<uintptr_t>(out);
    bwr.read_size = in ? sizeof(*in) : 0;
    bwr.read_buffer = reinterpret_cast<uintptr_t>(in);
    if (::ioctl(process_->driverFD(), MINI_BINDER_WRITE_READ, &bwr) < 0) return -errno;
    return 0;
}
status_t IPCThreadState::transact(uint32_t handle, uint32_t code, Parcel &data, Parcel *reply, uint32_t flags) {
    if ((flags & ~MINI_TF_ONE_WAY) || (!reply && !(flags & MINI_TF_ONE_WAY))) return -EINVAL;
    mini_binder_command out{}, event{};
    out.cmd = MINI_BC_TRANSACTION;
    out.transaction.target_handle = handle;
    out.transaction.code = code;
    out.transaction.flags = flags;
    data.exportData(&out.transaction);
    if (flags & MINI_TF_ONE_WAY) return talkWithDriver(&out, nullptr);
    int ret = talkWithDriver(&out, &event);
    if (ret) return ret; // EINTR cancels the call: do not silently resubmit it.
    if (event.cmd != MINI_BR_REPLY) return -EPROTO;
    const auto &tx = event.transaction;
    if (tx.reply_offset > process_->page_size_ ||
        tx.size > process_->page_size_ - tx.reply_offset) ret = -EPROTO;
    else ret = reply->importData(process_->mapping_ + tx.reply_offset, tx);
    // Reply owns the bytes before FREE_REPLY makes this mapping position reusable.
    mini_binder_command release{};
    release.cmd = MINI_BC_FREE_REPLY;
    int freed = talkWithDriver(&release, nullptr);
    return ret ? ret : (freed ? freed : tx.status);
}
status_t IPCThreadState::executeCommand(const mini_binder_command &event) {
    if (event.cmd != MINI_BR_TRANSACTION) return -EPROTO;
    const auto &tx = event.transaction;
    Parcel data, reply;
    uintptr_t base = reinterpret_cast<uintptr_t>(process_->mapping_);
    int status = -EPROTO;
    if (tx.data_address >= base && tx.data_address - base <= process_->page_size_ &&
        tx.size <= process_->page_size_ - (tx.data_address - base))
        status = data.importData(reinterpret_cast<const void *>(tx.data_address), tx);
    auto object = process_->findObject(tx.object_cookie);
    if (!status) status = object ? object->transact(tx.code, data, &reply) : -ENOENT;
    mini_binder_command out{};
    // Oneway has no business reply; release the current request to advance its Node queue.
    if (tx.flags & MINI_TF_ONE_WAY) {
        out.cmd = MINI_BC_FREE_BUFFER;
        return talkWithDriver(&out, nullptr);
    }
    out.cmd = MINI_BC_REPLY;
    out.transaction.status = status;
    if (!status) reply.exportData(&out.transaction);
    return talkWithDriver(&out, nullptr);
}
status_t IPCThreadState::joinThreadPool() {
    mini_binder_command enter{};
    enter.cmd = MINI_BC_ENTER_LOOPER;
    int ret = talkWithDriver(&enter, nullptr);
    if (ret) return ret;
    for (;;) {
        mini_binder_command event{};
        ret = talkWithDriver(nullptr, &event);
        if (ret) break;
        ret = executeCommand(event);
        if (ret) break;
    }
    mini_binder_command exit{};
    exit.cmd = MINI_BC_EXIT_LOOPER;
    talkWithDriver(&exit, nullptr);
    return ret;
}
status_t BBinder::transact(uint32_t code, Parcel &data, Parcel *reply) {
    if (!reply) return -EINVAL;
    return onTransact(code, data, reply);
}
status_t BBinder::onTransact(uint32_t, Parcel &, Parcel *) { return -ENOSYS; }
}
