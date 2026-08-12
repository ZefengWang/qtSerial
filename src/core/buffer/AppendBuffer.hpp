#ifndef SRC_CORE_BUFFER_APPEND_BUFFER_HPP
#define SRC_CORE_BUFFER_APPEND_BUFFER_HPP

#include "IBufferStrategy.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sd {

// 追加缓冲：累积所有数据，达到容量上限后从头部裁剪最旧数据。
// 适合需要"完整历史 + 上限保护"的场景（如接收日志面板），防止内存无限膨胀。
// 采用紧凑表示：有效数据始终位于 buf_[0, size_)。
class AppendBuffer : public IBufferStrategy {
public:
    explicit AppendBuffer(std::size_t cap = kDefaultCapacity);

    void write(const Byte* data, std::size_t len) override;
    std::vector<Byte> readAll() override;
    std::size_t bytesAvailable() const override;
    std::uint64_t droppedCount() const override;
    void clear() override;
    std::size_t capacity() const override;

    static constexpr std::size_t kDefaultCapacity = 1 << 22; // 4MB

private:
    std::vector<Byte> buf_;
    std::size_t       valid_ = 0; // 有效字节数
    std::uint64_t     dropped_ = 0;
};

} // namespace sd

#endif // SRC_CORE_BUFFER_APPEND_BUFFER_HPP