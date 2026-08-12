#include "DoubleBuffer.hpp"

#include <cstring>

namespace sd {

DoubleBuffer::DoubleBuffer(std::size_t cap)
    : back_(cap), front_(cap) {}

void DoubleBuffer::write(const Byte* data, std::size_t len) {
    if (!data || len == 0) return;
    if (back_.empty()) return;

    // 后备区已满则丢弃最旧数据（防膨胀）。
    if (valid_ + len > capacity()) {
        std::size_t toDrop = valid_ + len - capacity();
        dropped_ += toDrop;
        std::memmove(back_.data(), back_.data() + toDrop, valid_ - toDrop);
        valid_ -= toDrop;
    }
    std::memcpy(back_.data() + valid_, data, len);
    valid_ += len;
}

std::vector<IBufferStrategy::Byte> DoubleBuffer::readAll() {
    // 交换前后区：把已累积的后备区交给消费者，同时拿到空的旧前区继续生产。
    std::swap(front_, back_);
    std::size_t retLen = valid_;
    valid_ = 0;

    std::vector<Byte> out;
    out.reserve(retLen);
    out.assign(front_.begin(), front_.begin() + retLen);
    return out;
}

std::size_t DoubleBuffer::bytesAvailable() const { return valid_; }

std::uint64_t DoubleBuffer::droppedCount() const { return dropped_; }

void DoubleBuffer::clear() {
    valid_ = 0;
    dropped_ = 0;
    front_.assign(front_.size(), 0);
    back_.assign(back_.size(), 0);
}

std::size_t DoubleBuffer::capacity() const { return back_.size(); }

} // namespace sd