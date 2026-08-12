#ifndef SRC_PLUGIN_IPLUGIN_HPP
#define SRC_PLUGIN_IPLUGIN_HPP

#include "IViewHost.hpp"

#include "../core/Frame.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sd {

// 插件统一接口：让波形、3D、协议解析等扩展即插即用，不改核心层。
// 插件通过事件总线订阅数据，通过统一视图宿主呈现。
class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual std::string id() const = 0;                              // 唯一标识
    virtual std::string name() const = 0;                            // 显示名
    virtual IViewHost*  view() = 0;                                  // 统一视图宿主

    // 订阅回调（原始字节流）。基础插件消费它。
    virtual void onData(const std::vector<std::uint8_t>&) = 0;

    // 订阅回调（协议解析后的结构化帧）。可视化插件消费它。
    // 新增默认空实现，避免破坏既有插件子类。
    virtual void onFrame(const Frame&) {}

    virtual void onActivate() = 0;                            // 激活
    virtual void onDeactivate() = 0;                          // 停用
};

} // namespace sd

#endif // SRC_PLUGIN_IPLUGIN_HPP