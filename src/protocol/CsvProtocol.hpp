#ifndef SRC_PROTOCOL_CSV_PROTOCOL_HPP
#define SRC_PROTOCOL_CSV_PROTOCOL_HPP

#include "IProtocol.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sd {

// CSV 数值流协议（适合波形等连续采样）：
//   首行为表头（如 "t,volt,curr"），随后每行 "1,3.3,0.5" 解析为一帧，
//   数值字段按表头命名。无表头时按列名 c0,c1,... 命名。
//   若行首单元格非数值，则作为帧名（如 "err,12" -> name="err"）。
// 编码：cmd,val1,val2,...\n
class CsvProtocol : public IProtocol {
public:
    CsvProtocol() = default;

    std::vector<Frame> feed(const std::uint8_t* data, std::size_t len) override;
    void reset() override;
    const char* name() const override { return "csv"; }

    std::vector<std::uint8_t> encode(
        const std::string& cmd,
        const std::vector<std::pair<std::string, double>>& args) override;

private:
    std::string  pending_;   // 残片
    bool         haveHeader_ = false;
    std::vector<std::string> header_; // 表头列名（可能为空）
};

} // namespace sd

#endif // SRC_PROTOCOL_CSV_PROTOCOL_HPP