#ifndef SRC_IO_SERIAL_SOURCE_HPP
#define SRC_IO_SERIAL_SOURCE_HPP

#include "core/DataSource.hpp"

#include <QSerialPort>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class QSerialPort;
class QSerialPortInfo;

namespace sd {

// 生产环境数据源：把 QSerialPort 封装到 DataSource 抽象接口背后。
// 上层（Service/Session/UI）只依赖 DataSource，不感知具体硬件实现；
// 测试则注入 FakeSource，实现"核心逻辑脱离硬件即可验证"。
class SerialSource : public DataSource {
public:
    SerialSource();
    ~SerialSource() override;

    // DataSource 接口
    bool open(const PortConfig& cfg) override;
    void close() override;
    std::vector<std::uint8_t> read() override;
    std::size_t write(const std::vector<std::uint8_t>& data) override;
    std::vector<std::string> scanPorts() override;
    std::string lastError() const override;

private:
    QSerialPort* port_;
    std::string  lastError_;
};

} // namespace sd

#endif // SRC_IO_SERIAL_SOURCE_HPP