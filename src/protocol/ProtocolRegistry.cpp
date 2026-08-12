#include "ProtocolRegistry.hpp"

#include "CsvProtocol.hpp"
#include "GenericBinaryProtocol.hpp"
#include "LineProtocol.hpp"

#include <algorithm>

namespace sd {

ProtocolRegistry::ProtocolRegistry() {
    // 内置协议。
    registerFactory("line", [] { return std::make_unique<LineProtocol>(); });
    registerFactory("csv", [] { return std::make_unique<CsvProtocol>(); });
}

void ProtocolRegistry::registerFactory(const std::string& name, Factory factory) {
    if (name.empty() || !factory) return;
    factories_[name] = std::move(factory);
}

std::unique_ptr<IProtocol> ProtocolRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end()) return nullptr;
    return it->second();
}

std::vector<std::string> ProtocolRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(factories_.size());
    for (const auto& kv : factories_) out.push_back(kv.first);
    return out;
}

bool ProtocolRegistry::contains(const std::string& name) const {
    return factories_.find(name) != factories_.end();
}

void ProtocolRegistry::registerSchema(const ProtocolSchema& schema) {
    if (schema.name.empty() || schema.fields.empty()) return;
    // 捕获 schema 副本，避免工厂被构造多次时共享可变状态。
    registerFactory(schema.name, [schema] {
        return std::make_unique<GenericBinaryProtocol>(schema);
    });
}

} // namespace sd