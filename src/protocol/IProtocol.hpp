#ifndef SRC_PROTOCOL_IPROTOCOL_HPP
#define SRC_PROTOCOL_IPROTOCOL_HPP

#include "../core/Frame.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sd {

// 协议解析器接口：把字节流增量解析成结构化帧。
// 有状态：内部保留未完成帧的残片，feed() 每次产出 0..n 个完整帧。
// 所有实现必须纯 C++、无 Qt 依赖，便于在 core 层单测与多种前端复用。
class IProtocolParser {
public:
    virtual ~IProtocolParser() = default;

    // 喂入新字节，返回本轮解析出的完整帧（可能为空）。
    virtual std::vector<Frame> feed(const std::uint8_t* data, std::size_t len) = 0;

    // 丢弃未完成状态（如切换协议/清空接收区时调用）。
    virtual void reset() = 0;

    // 协议名（用于显示与注册）。
    virtual const char* name() const = 0;
};

// 协议编码器接口：把高层命令编码成字节流。
// 与解析器对应，供"协议化发送"使用。
class IProtocolEncoder {
public:
    virtual ~IProtocolEncoder() = default;

    // 编码命令。cmd 为命令名，args 为数值参数（如 {"voltage", 5.0}）。
    // 返回待写入串口的字节；失败返回空。
    virtual std::vector<std::uint8_t> encode(
        const std::string& cmd,
        const std::vector<std::pair<std::string, double>>& args) = 0;
};

// 一个完整协议 = 解析 + 编码。用户自定义协议实现本接口后注册即可。
// name() 继承自 IProtocolParser，是协议级单一名字。
class IProtocol : public IProtocolParser, public IProtocolEncoder {
};

} // namespace sd

#endif // SRC_PROTOCOL_IPROTOCOL_HPP