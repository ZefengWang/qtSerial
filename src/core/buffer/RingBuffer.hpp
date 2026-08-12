#ifndef SRC_CORE_BUFFER_RING_BUFFER_HPP
#define SRC_CORE_BUFFER_RING_BUFFER_HPP

#include "IBufferStrategy.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sd {

// 环形缓冲：固定容量，满则丢弃最旧数据。
// 适合持续高速数据流（如波形采样、持续监控），保证内存占用恒定。
// 注意：单线程使用（生产者=串口线程，消费者=UI 线程需外部加锁或专用双缓冲）。
class RingBuffer : public IBufferStrategy {
public:
    explicit RingBuffer(std::size_t cap = kDefaultCapacity);

    void write(const Byte* data, std::size_t len) override;
    std::vector<Byte> readAll() override;
    std::size_t bytesAvailable() const override;
    std::uint64_t droppedCount() const override;
    void clear() override;
    std::size_t capacity() const override;

    static constexpr std::size_t kDefaultCapacity = 1 << 20; // 1MB

private:
    std::vector<Byte> buf_;
    std::size_t       head_ = 0; // 读位置
    std::size_t       size_ = 0; // 有效字节数
    std::uint64_t     dropped_ = 0;
};

} // namespace sd

#endif // SRC_CORE_BUFFER_RING_BUFFER_HPP