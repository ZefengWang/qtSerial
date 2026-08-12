#ifndef SRC_CORE_BUFFER_IBUFFER_STRATEGY_HPP
#define SRC_CORE_BUFFER_IBUFFER_STRATEGY_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sd {

// 缓冲策略抽象：把"数据怎么存"抽象成接口，运行时按场景切换。
// 解决高速串口丢包与内存膨胀的核心机制。
class IBufferStrategy {
public:
    using Byte = std::uint8_t;

    virtual ~IBufferStrategy() = default;

    // 生产者：写入数据。
    virtual void write(const Byte* data, std::size_t len) = 0;
    void write(const std::vector<Byte>& data) { write(data.data(), data.size()); }

    // 消费者：取走全部可用数据（取出后从缓冲移除）。
    virtual std::vector<Byte> readAll() = 0;

    // 当前可用数据量（字节）。
    virtual std::size_t bytesAvailable() const = 0;

    // 累计丢包计数（因容量受限而丢弃的字节数）。
    virtual std::uint64_t droppedCount() const = 0;

    // 重置缓冲，清空所有数据与计数。
    virtual void clear() = 0;

    // 缓冲容量上限（字节）。不同实现语义不同，见各实现注释。
    virtual std::size_t capacity() const = 0;
};

} // namespace sd

#endif // SRC_CORE_BUFFER_IBUFFER_STRATEGY_HPP