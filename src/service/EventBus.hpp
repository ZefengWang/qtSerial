#ifndef SRC_SERVICE_EVENT_BUS_HPP
#define SRC_SERVICE_EVENT_BUS_HPP

#include "../core/Frame.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace sd {

// 事件总线：插件与 UI 的订阅/发布通道。
// 生产者在串口线程发布数据，订阅者在各自上下文消费。
// 支持两条类型化通道：
//   1) 原始字节主题（subscribe/publish）—— 承载 rx 原始数据流；
//   2) 帧主题（subscribeFrame/publishFrame）—— 承载协议解析后的结构化帧。
// 均按主题顺序投递，可录制（供测试断言）。
class EventBus {
public:
    using DataHandler = std::function<void(const std::vector<std::uint8_t>&)>;
    using FrameHandler = std::function<void(const Frame&)>;

    // ---------- 原始字节通道 ----------
    // 订阅某个主题，返回订阅 ID（用于退订）。
    std::size_t subscribe(const std::string& topic, DataHandler handler);
    // 退订。
    void unsubscribe(const std::string& topic, std::size_t subId);
    // 发布数据到某个主题（同步调用所有订阅者）。
    void publish(const std::string& topic, const std::vector<std::uint8_t>& data);
    // 供测试：录制某个主题发布过的所有数据。
    const std::vector<std::vector<std::uint8_t>>& recorded(const std::string& topic) const;

    // ---------- 帧通道 ----------
    // 订阅帧主题，返回订阅 ID。
    std::size_t subscribeFrame(const std::string& topic, FrameHandler handler);
    void        unsubscribeFrame(const std::string& topic, std::size_t subId);
    // 发布一帧到帧主题（同步调用所有订阅者）。
    void publishFrame(const std::string& topic, const Frame& frame);
    // 供测试：录制某个帧主题发布过的所有帧。
    const std::vector<Frame>& recordedFrames(const std::string& topic) const;

private:
    struct Sub {
        std::size_t id;
        DataHandler handler;
    };
    struct FrameSub {
        std::size_t id;
        FrameHandler handler;
    };

    std::mutex mutex_;
    std::map<std::string, std::vector<Sub>> subs_;
    std::map<std::string, std::vector<std::vector<std::uint8_t>>> record_;
    std::map<std::string, std::vector<FrameSub>> frameSubs_;
    std::map<std::string, std::vector<Frame>> frameRecord_;
    std::size_t nextId_ = 1;
};

} // namespace sd

#endif // SRC_SERVICE_EVENT_BUS_HPP