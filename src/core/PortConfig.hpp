#ifndef SRC_CORE_PORT_CONFIG_HPP
#define SRC_CORE_PORT_CONFIG_HPP

#include <cstdint>
#include <string>

namespace sd {

// 串口参数配置（与 Qt 控件的整数枚举对齐，但保持不依赖 Qt）
struct PortConfig {
    std::string name;
    int         baudRate    = 115200;
    int         dataBits    = 8;   // 5/6/7/8
    int         parity      = 0;   // 0=No,1=Even,2=Odd,3=Space,4=Mark
    int         stopBits    = 1;   // 1/1.5/2
    int         flowControl = 0;   // 0=No,1=Hardware,2=Software

    bool operator==(const PortConfig& o) const {
        return name == o.name && baudRate == o.baudRate &&
               dataBits == o.dataBits && parity == o.parity &&
               stopBits == o.stopBits && flowControl == o.flowControl;
    }
    bool operator!=(const PortConfig& o) const { return !(*this == o); }
};

} // namespace sd

#endif // SRC_CORE_PORT_CONFIG_HPP