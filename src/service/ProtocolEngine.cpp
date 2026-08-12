#include "ProtocolEngine.hpp"

namespace sd {

ProtocolEngine::ProtocolEngine(EventBus& bus,
                               const ProtocolRegistry& registry,
                               std::string rxTopic,
                               std::string frameTopic)
    : bus_(bus), registry_(registry),
      rxTopic_(std::move(rxTopic)), frameTopic_(std::move(frameTopic)) {
    rxSubId_ = bus_.subscribe(rxTopic_, [this](const std::vector<std::uint8_t>& d) {
        onRx(d);
    });
}

ProtocolEngine::~ProtocolEngine() {
    bus_.unsubscribe(rxTopic_, rxSubId_);
}

bool ProtocolEngine::select(const std::string& name) {
    auto p = registry_.create(name);
    if (!p) return false;
    proto_ = std::move(p);
    return true;
}

bool ProtocolEngine::applySchema(const sd::ProtocolSchema& schema) {
    if (schema.name.empty() || schema.fields.empty()) return false;
    registry_.registerSchema(schema);
    return select(schema.name);
}

std::string ProtocolEngine::current() const {
    return proto_ ? proto_->name() : std::string();
}

std::vector<std::string> ProtocolEngine::available() const {
    return registry_.names();
}

std::vector<std::uint8_t> ProtocolEngine::encode(
    const std::string& cmd,
    const std::vector<std::pair<std::string, double>>& args) {
    if (!proto_) return {};
    return proto_->encode(cmd, args);
}

std::vector<std::uint8_t> ProtocolEngine::build(
    const std::string& cmd,
    const std::vector<std::pair<std::string, double>>& args) {
    return encode(cmd, args);
}

void ProtocolEngine::onRx(const std::vector<std::uint8_t>& data) {
    if (!proto_ || data.empty()) return;
    auto frames = proto_->feed(data.data(), data.size());
    for (auto& f : frames) {
        f.seq = ++seq_;
        bus_.publishFrame(frameTopic_, f);
    }
}

} // namespace sd