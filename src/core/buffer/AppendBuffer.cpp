#include "AppendBuffer.hpp"

#include <algorithm>
#include <cstring>

namespace sd {

AppendBuffer::AppendBuffer(std::size_t cap) : buf_(cap) {}

void AppendBuffer::write(const Byte* data, std::size_t len) {
    if (!data || len == 0) return;
    if (buf_.empty()) return;

    // 单次写入超过容量：只保留最后 cap 字节，其余计入丢弃。
    if (len >= capacity()) {
        dropped_ += len - capacity();
        std::memcpy(buf_.data(), data + (len - capacity()), capacity());
        valid_ = capacity();
        return;
    }

    // 累积超过容量：从头部裁剪最旧数据。
    if (valid_ + len > capacity()) {
        std::size_t toDrop = valid_ + len - capacity();
        dropped_ += toDrop;
        std::memmove(buf_.data(), buf_.data() + toDrop, valid_ - toDrop);
        valid_ -= toDrop;
    }

    std::memcpy(buf_.data() + valid_, data, len);
    valid_ += len;
}

std::vector<IBufferStrategy::Byte> AppendBuffer::readAll() {
    std::vector<Byte> out;
    out.reserve(valid_);
    out.assign(buf_.begin(), buf_.begin() + valid_);
    valid_ = 0;
    return out;
}

std::size_t AppendBuffer::bytesAvailable() const { return valid_; }

std::uint64_t AppendBuffer::droppedCount() const { return dropped_; }

void AppendBuffer::clear() {
    valid_ = 0;
    dropped_ = 0;
}

std::size_t AppendBuffer::capacity() const { return buf_.size(); }

} // namespace sd