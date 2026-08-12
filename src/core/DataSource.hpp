#ifndef SRC_CORE_DATA_SOURCE_HPP
#define SRC_CORE_DATA_SOURCE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "PortConfig.hpp"

namespace sd {

// 数据源抽象：Core 层唯一的数据来源边界。
// 生产实现包 QSerialPort；测试实现注入确定性字节流。
// 依赖注入的核心：上层只依赖本接口，不依赖具体硬件实现。
class DataSource {
public:
    virtual ~DataSource() = default;

    // 打开/关闭端口。返回是否成功。
    virtual bool open(const PortConfig& cfg) = 0;
    virtual void close() = 0;

    // 读取当前可用数据（非阻塞，返回实际读到的字节）。
    virtual std::vector<std::uint8_t> read() = 0;

    // 写入数据，返回实际写入字节数。
    virtual std::size_t write(const std::vector<std::uint8_t>& data) = 0;

    // 扫描可用端口名列表。
    virtual std::vector<std::string> scanPorts() = 0;

    // 最近一次操作错误的人类可读描述。
    virtual std::string lastError() const = 0;
};

} // namespace sd

#endif // SRC_CORE_DATA_SOURCE_HPP