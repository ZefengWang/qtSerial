#ifndef SRC_SERVICE_PROTOCOL_ENGINE_HPP
#define SRC_SERVICE_PROTOCOL_ENGINE_HPP

#include "../protocol/IProtocol.hpp"
#include "../protocol/ProtocolRegistry.hpp"
#include "EventBus.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sd {

// 协议引擎：把 Session 发布到 rx 主题的原始字节，经当前协议解析为帧，
// 再发布到 "frames" 帧主题。同时提供协议化编码发送。
//
// 数据通路：
//   Session::poll() -> EventBus(rx) -> [ProtocolEngine] -> EventBus(frames)
//                                                   -> 波形/3D/解析插件
class ProtocolEngine {
public:
    // 依赖注入：事件总线、协议注册表、rx 主题名、frames 主题名。
    ProtocolEngine(EventBus& bus,
                   const ProtocolRegistry& registry,
                   std::string rxTopic = "rx",
                   std::string frameTopic = "frames");

    ~ProtocolEngine();

    // 选择并加载协议（按名）。成功返回 true；名字未注册返回 false，保持原协议。
    bool select(const std::string& name);

    // 当前协议名（未加载返回空串）。
    std::string current() const;

    // 可用协议名列表。
    std::vector<std::string> available() const;

    // 协议化编码发送：把高层命令编码为字节（不关乎是否 open，编码本身即可执行）。
    // 返回编码后的字节；协议未加载返回空。
    std::vector<std::uint8_t> encode(const std::string& cmd,
                                     const std::vector<std::pair<std::string, double>>& args);

    // 下发编码后的字节（编码 + 交由上层实际 send）。
    // args 为空时等价于 sendEncoded(cmd 的原始命令)。
    std::vector<std::uint8_t> build(const std::string& cmd,
                                    const std::vector<std::pair<std::string, double>>& args);

private:
    void onRx(const std::vector<std::uint8_t>& data);

    EventBus&          bus_;
    ProtocolRegistry   registry_; // 持有一份独立注册表（含注册进来的自定义协议）
    std::string        rxTopic_;
    std::string        frameTopic_;
    std::unique_ptr<IProtocol> proto_;
    std::size_t        rxSubId_ = 0;
    std::uint64_t      seq_ = 0;
};

} // namespace sd

#endif // SRC_SERVICE_PROTOCOL_ENGINE_HPP