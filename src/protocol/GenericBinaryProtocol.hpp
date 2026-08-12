#ifndef SRC_PROTOCOL_GENERIC_BINARY_PROTOCOL_HPP
#define SRC_PROTOCOL_GENERIC_BINARY_PROTOCOL_HPP

#include "../core/FieldSchema.hpp"
#include "IProtocol.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sd {

// 通用二进制协议：依据 ProtocolSchema 把字节流增量解析成 Frame。
// 这是"可配置协议"的落地实现——用户在协议解析界面配置字段，
// 生成 schema，本协议按字节序/类型/比例/偏移解析，padding 字段剔除。
//
// 数据通路：Session -> EventBus(rx) -> [本协议] -> EventBus(frames)
class GenericBinaryProtocol : public IProtocol {
public:
    explicit GenericBinaryProtocol(ProtocolSchema schema);

    // IProtocolParser
    std::vector<Frame> feed(const std::uint8_t* data, std::size_t len) override;
    void reset() override;
    const char* name() const override { return schema_.name.c_str(); }

    // IProtocolEncoder
    std::vector<std::uint8_t> encode(
        const std::string& cmd,
        const std::vector<std::pair<std::string, double>>& args) override;

    // 访问 schema（供 UI 回显 / 测试）。
    const ProtocolSchema& schema() const { return schema_; }

private:
    ProtocolSchema schema_;
    std::vector<std::uint8_t> pending_; // 未凑满一帧的残片
    std::uint64_t seq_ = 0;
};

} // namespace sd

#endif // SRC_PROTOCOL_GENERIC_BINARY_PROTOCOL_HPP