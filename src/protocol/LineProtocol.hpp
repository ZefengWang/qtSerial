#ifndef SRC_PROTOCOL_LINE_PROTOCOL_HPP
#define SRC_PROTOCOL_LINE_PROTOCOL_HPP

#include "IProtocol.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sd {

// 行协议（内置默认协议）：以换行符 \n 分隔帧。
// 每行作为一帧的文本，若行内容形如 key=value 则以空格分隔多个键值对，
// 数值可转为 double 的进入 numeric 字段，否则进入 text 字段。
// 编码：cmd arg1=val1 arg2=val2...\n
class LineProtocol : public IProtocol {
public:
    LineProtocol() = default;

    std::vector<Frame> feed(const std::uint8_t* data, std::size_t len) override;
    void reset() override;
    const char* name() const override { return "line"; }

    std::vector<std::uint8_t> encode(
        const std::string& cmd,
        const std::vector<std::pair<std::string, double>>& args) override;

private:
    std::string pending_; // 未完成行的残片
};

} // namespace sd

#endif // SRC_PROTOCOL_LINE_PROTOCOL_HPP