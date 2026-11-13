// SPDX-License-Identifier: GPL-2.0-only
#include "binder.h"
#include <cerrno>
namespace binderabc {
parcel Parcel::view() {
    parcel p{};
    parcel_init(&p, data_.data(), data_.size());
    p.size = size_;
    p.position = position_;
    return p;
}
status_t Parcel::writeU32(uint32_t value) {
    auto p = view();
    if (parcel_write_u32(&p, value)) return -ENOSPC;
    size_ = p.size;
    return 0;
}
status_t Parcel::readU32(uint32_t *value) {
    if (!value) return -EINVAL;
    auto p = view();
    if (parcel_read_u32(&p, value)) return -EINVAL;
    position_ = p.position;
    return 0;
}
status_t Parcel::writeString(const std::string &value) {
    auto p = view();
    if (parcel_write_string(&p, value.data(), value.size())) return -ENOSPC;
    size_ = p.size;
    return 0;
}
status_t Parcel::readString(std::string *value) {
    if (!value) return -EINVAL;
    auto p = view();
    const unsigned char *text;
    size_t length;
    if (parcel_read_string(&p, &text, &length)) return -EINVAL;
    value->assign(reinterpret_cast<const char *>(text), length);
    position_ = p.position;
    return 0;
}
status_t BBinder::transact(uint32_t code, Parcel &data, Parcel *reply) {
    if (!reply) return -EINVAL;
    return onTransact(code, data, reply);
}
status_t BBinder::onTransact(uint32_t, Parcel &, Parcel *) { return -ENOSYS; }
}
