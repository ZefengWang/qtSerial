#ifndef SRC_CORE_FAKE_SOURCE_HPP
#define SRC_CORE_FAKE_SOURCE_HPP

#include "DataSource.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sd {

// 测试用数据源：注入确定性字节流，绕过真实硬件。
// 这是依赖注入让核心逻辑可脱离 UI/硬件测试的关键实现。
class FakeSource : public DataSource {
public:
    // 扫描时返回的端口列表。
    void setScanResult(std::vector<std::string> ports) { scanResult_ = std::move(ports); }

    // 注入一段"待读取"数据（模拟下位机发来的数据）。
    void pushIncoming(const std::vector<std::uint8_t>& data);

    // 是否已打开。
    bool isOpen() const { return open_; }

    // 记录所有写出的数据（供断言）。
    const std::vector<std::vector<std::uint8_t>>& written() const { return written_; }

    // 打开失败时返回的错误。
    void setOpenResult(bool ok) { openResult_ = ok; }

    // ---- DataSource ----
    bool open(const PortConfig& cfg) override;
    void close() override;
    std::vector<std::uint8_t> read() override;
    std::size_t write(const std::vector<std::uint8_t>& data) override;
    std::vector<std::string> scanPorts() override;
    std::string lastError() const override;

private:
    std::vector<std::uint8_t> incoming_;   // 待消费的输入数据
    std::vector<std::vector<std::uint8_t>> written_;
    std::vector<std::string> scanResult_;
    PortConfig cfg_;
    bool open_ = false;
    bool openResult_ = true;
    std::string lastError_;
};

} // namespace sd

#endif // SRC_CORE_FAKE_SOURCE_HPP