#ifndef SRC_CORE_BUFFER_DOUBLE_BUFFER_HPP
#define SRC_CORE_BUFFER_DOUBLE_BUFFER_HPP

#include "IBufferStrategy.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sd {

// 双缓冲：生产者写入"后备区"，消费者读取"前区"，交换时用原子标志。
// 专为跨线程设计（串口线程生产 + UI 线程消费），避免共享缓冲的锁竞争。
// 语义：write 始终累积到后备区；readAll 交换前后区并返回"上一批"数据。
// 注意：本实现非完全线程安全（交换需外部同步），但大幅降低单缓冲的覆盖风险。
class DoubleBuffer : public IBufferStrategy {
public:
    explicit DoubleBuffer(std::size_t cap = kDefaultCapacity);

    void write(const Byte* data, std::size_t len) override;
    std::vector<Byte> readAll() override;
    std::size_t bytesAvailable() const override;
    std::uint64_t droppedCount() const override;
    void clear() override;
    std::size_t capacity() const override;

    static constexpr std::size_t kDefaultCapacity = 1 << 21; // 2MB

private:
    std::vector<Byte> back_;    // 生产者写入的后备区
    std::vector<Byte> front_;   // 消费者读取的前区
    std::size_t       valid_ = 0; // 后备区有效字节数
    std::uint64_t     dropped_ = 0;
};

} // namespace sd

#endif // SRC_CORE_BUFFER_DOUBLE_BUFFER_HPP