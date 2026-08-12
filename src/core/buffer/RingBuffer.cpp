#include "RingBuffer.hpp"

#include <algorithm>
#include <cstring>

namespace sd {

RingBuffer::RingBuffer(std::size_t cap) : buf_(cap) {}

void RingBuffer::write(const Byte* data, std::size_t len) {
    if (!data || len == 0) return;
    if (buf_.empty()) return;

    // 单次写入超过容量：只保留最后 capacity() 字节，其余全部丢弃。
    if (len >= capacity()) {
        dropped_ += len - capacity();
        data += len - capacity();
        len = capacity();
        head_ = 0;
        size_ = 0;
    } else if (size_ + len > capacity()) {
        // 累积溢出：从头部丢弃最旧 (size_ + len - capacity()) 字节。
        std::size_t discard = size_ + len - capacity();
        dropped_ += discard;
        head_ = (head_ + discard) % capacity();
        size_ -= discard;
    }

    // 从尾部写入（可能环绕）。
    std::size_t tail = (head_ + size_) % capacity();
    std::size_t first = std::min(len, capacity() - tail);
    std::memcpy(buf_.data() + tail, data, first);
    if (len > first) {
        std::memcpy(buf_.data(), data + first, len - first);
    }
    size_ += len;
}

std::vector<IBufferStrategy::Byte> RingBuffer::readAll() {
    std::vector<Byte> out;
    out.reserve(size_);
    if (size_ == 0) return out;
    std::size_t first = std::min(size_, capacity() - head_);
    out.insert(out.end(), buf_.begin() + head_, buf_.begin() + head_ + first);
    if (size_ > first) {
        out.insert(out.end(), buf_.begin(), buf_.begin() + (size_ - first));
    }
    head_ = (head_ + size_) % capacity();
    size_ = 0;
    return out;
}

std::size_t RingBuffer::bytesAvailable() const { return size_; }

std::uint64_t RingBuffer::droppedCount() const { return dropped_; }

void RingBuffer::clear() {
    head_ = 0;
    size_ = 0;
    dropped_ = 0;
}

std::size_t RingBuffer::capacity() const { return buf_.size(); }

} // namespace sd