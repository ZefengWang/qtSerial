#ifndef SRC_SERVICE_SESSION_HPP
#define SRC_SERVICE_SESSION_HPP

#include "../core/DataSource.hpp"
#include "../core/IClock.hpp"
#include "../core/buffer/IBufferStrategy.hpp"
#include "EventBus.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sd {

// 会话：Service 层的门面，编排 数据源 → 缓冲 → 事件总线。
// 依赖注入所有核心组件，因此无需真实硬件即可构造与测试。
class Session {
public:
    // 注入依赖：数据源、缓冲策略、时钟、事件总线。
    // 不持有所有权（由调用方保证生命周期）是默认约定；也提供 owned 变体。
    Session(DataSource*           source,
            IBufferStrategy*      buffer,
            IClock*               clock,
            EventBus&             bus,
            const std::string&    rxTopic = "rx",
            std::size_t           batchThreshold = kDefaultBatch);

    ~Session();

    // 打开/关闭端口。
    bool open(const PortConfig& cfg);
    void close();
    bool isOpen() const { return open_; }

    // 发送数据（写入数据源）。
    std::size_t send(const std::vector<std::uint8_t>& data);

    // 从数据源拉取并分发到缓冲/事件总线（由串口线程或定时器调用）。
    // 返回本次拉取的字节数。
    std::size_t poll();

    // 统计
    std::uint64_t rxBytes() const { return rxBytes_; }
    std::uint64_t txBytes() const { return txBytes_; }
    std::uint64_t rxDropped() const { return buffer_ ? buffer_->droppedCount() : 0; }

    // 访问内部组件（测试用）
    IBufferStrategy* buffer() const { return buffer_; }
    // 替换缓冲策略（需在 open() 之前调用；不持有所有权）
    void setBuffer(IBufferStrategy* b) { buffer_ = b; }
    EventBus&        bus() { return bus_; }
    DataSource*      source() const { return source_; }

    static constexpr std::size_t kDefaultBatch = 16 * 1024; // 16KB 攒批阈值

private:
    void flushBatch(); // 把积压数据发布到事件总线
    DataSource*      source_ = nullptr;
    IBufferStrategy* buffer_ = nullptr;
    IClock*          clock_ = nullptr;
    EventBus&        bus_;
    std::string      rxTopic_;
    std::size_t      batchThreshold_;
    bool             open_ = false;
    std::uint64_t    rxBytes_ = 0;
    std::uint64_t    txBytes_ = 0;
    std::vector<std::uint8_t> pending_; // 攒批缓冲
};

} // namespace sd

#endif // SRC_SERVICE_SESSION_HPP