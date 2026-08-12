#ifndef SRC_PROTOCOL_PROTOCOL_REGISTRY_HPP
#define SRC_PROTOCOL_PROTOCOL_REGISTRY_HPP

#include "IProtocol.hpp"

#include "../core/FieldSchema.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sd {

// 协议注册表：按名注册/查协议工厂。
// 内置 "line" 与 "csv"；用户自定义协议通过 registerFactory() 注册，
// 之后即可在任意前端（TUI/Qt/QML/浏览器）按名选用。
class ProtocolRegistry {
public:
    using Factory = std::function<std::unique_ptr<IProtocol>()>;

    ProtocolRegistry();

    // 注册一个协议工厂（按名覆盖）。name 须非空。
    void registerFactory(const std::string& name, Factory factory);

    // 创建一个协议实例；名字未注册时返回 nullptr。
    std::unique_ptr<IProtocol> create(const std::string& name) const;

    // 已注册的协议名列表（升序）。
    std::vector<std::string> names() const;

    // 是否已注册。
    bool contains(const std::string& name) const;

    // 注册一个"可配置二进制协议"：按 schema 生成协议并注册。
    // name 取自 schema.name；已存在同名会覆盖。
    void registerSchema(const ProtocolSchema& schema);

private:
    std::map<std::string, Factory> factories_;
};

} // namespace sd

#endif // SRC_PROTOCOL_PROTOCOL_REGISTRY_HPP