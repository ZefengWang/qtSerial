#include "PluginRegistry.hpp"

#include <algorithm>

namespace sd {

void PluginRegistry::add(std::unique_ptr<IPlugin> plugin) {
    if (plugin) plugins_.push_back(std::move(plugin));
}

IPlugin* PluginRegistry::find(const std::string& id) const {
    auto it = std::find_if(plugins_.begin(), plugins_.end(),
                           [&](const auto& p) { return p->id() == id; });
    return it == plugins_.end() ? nullptr : it->get();
}

std::vector<IPlugin*> PluginRegistry::all() const {
    std::vector<IPlugin*> out;
    out.reserve(plugins_.size());
    for (const auto& p : plugins_) out.push_back(p.get());
    return out;
}

bool PluginRegistry::activate(const std::string& id) {
    auto* p = find(id);
    if (!p) return false;
    p->onActivate();
    return true;
}

bool PluginRegistry::deactivate(const std::string& id) {
    auto* p = find(id);
    if (!p) return false;
    p->onDeactivate();
    return true;
}

void PluginRegistry::broadcast(const std::vector<std::uint8_t>& data) {
    for (auto& p : plugins_) p->onData(data);
}

} // namespace sd