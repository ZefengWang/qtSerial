#include "FieldPool.hpp"

namespace sd {

void FieldPool::setSchema(const ProtocolSchema& schema) {
    schema_ = schema;
    sources_.clear();
    hasDataFields_ = false;

    for (const auto& fd : schema_.fields) {
        if (fd.isPadding) continue; // padding 只占位，不产出数据源
        FieldSource fs;
        fs.name      = fd.name;
        fs.type      = fd.type;
        fs.unit      = fd.unit;
        fs.scale     = fd.scale;
        fs.offset    = fd.offset;
        fs.bigEndian = fd.bigEndian;
        sources_.push_back(std::move(fs));
    }
    hasDataFields_ = !sources_.empty();
}

const FieldSource* FieldPool::findSource(const std::string& name) const {
    for (const auto& s : sources_) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

void FieldPool::onFrame(const Frame& frame) {
    if (sources_.empty()) return;
    for (auto& s : sources_) {
        bool found = false;
        double v = frame.numericValue(s.name, &found);
        if (found) {
            s.lastValue = v;
            s.hasData   = true;
        }
    }
}

void FieldPool::clear() {
    schema_ = ProtocolSchema{};
    sources_.clear();
    hasDataFields_ = false;
}

} // namespace sd