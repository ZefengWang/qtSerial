#ifndef SRC_PLUGIN_PLUGIN_REGISTRY_HPP
#define SRC_PLUGIN_PLUGIN_REGISTRY_HPP

#include "IPlugin.hpp"

#include <memory>
#include <string>
#include <vector>

namespace sd {

// 插件注册表：管理插件的注册、检索与生命周期。
// 不持有所有权默认（也可用 shared_ptr 持有），由装配器统一管理。
class PluginRegistry {
public:
    // 注册一个插件（获取所有权）。
    void add(std::unique_ptr<IPlugin> plugin);

    // 按 id 查找。
    IPlugin* find(const std::string& id) const;

    // 全部插件。
    std::vector<IPlugin*> all() const;

    // 激活/停用某个插件。
    bool activate(const std::string& id);
    bool deactivate(const std::string& id);

    // 向所有插件分发数据（由事件总线回调调用）。
    void broadcast(const std::vector<std::uint8_t>& data);

private:
    std::vector<std::unique_ptr<IPlugin>> plugins_;
};

} // namespace sd

#endif // SRC_PLUGIN_PLUGIN_REGISTRY_HPP