#include "GenericBinaryProtocol.hpp"

#include <cstring>

namespace sd {

namespace {

// 把 1..8 字节（按设备字节序）解释为 double 值。
// 大端：buf[0] 为最高字节；小端：buf[0] 为最低字节。
double fieldToDouble(const FieldDesc& fd, const std::uint8_t* buf) {
    std::uint64_t raw = 0;
    if (fd.bigEndian) {
        for (int i = 0; i < fd.byteLength; ++i) raw = (raw << 8) | buf[i];
    } else {
        for (int i = fd.byteLength - 1; i >= 0; --i) raw = (raw << 8) | buf[i];
    }
    switch (fd.type) {
        case FieldType::U8:  return static_cast<double>(raw & 0xFF);
        case FieldType::U16: return static_cast<double>(raw & 0xFFFF);
        case FieldType::U32: return static_cast<double>(raw & 0xFFFFFFFFull);
        case FieldType::I8:  return static_cast<double>(static_cast<std::int8_t>(raw & 0xFF));
        case FieldType::I16: return static_cast<double>(static_cast<std::int16_t>(raw & 0xFFFF));
        case FieldType::I32: return static_cast<double>(static_cast<std::int32_t>(raw & 0xFFFFFFFFull));
        case FieldType::Bool:return static_cast<double>(raw != 0);
        case FieldType::F32: {
            std::uint32_t u = raw & 0xFFFFFFFFull;
            float f;
            std::memcpy(&f, &u, sizeof(f));
            return static_cast<double>(f);
        }
        case FieldType::F64: {
            double d;
            std::memcpy(&d, &raw, sizeof(d));
            return d;
        }
    }
    return 0.0;
}

// 把 double 物理值按字段定义写回字节（用于协议化发送）。
void writeField(const FieldDesc& fd, double phys, std::uint8_t* dst) {
    // 反算原始值：phys = raw * scale + offset  =>  raw = (phys - offset)/scale
    double v = (fd.scale == 0.0) ? 0.0 : (phys - fd.offset) / fd.scale;

    std::uint64_t raw = 0;
    int len = fd.byteLength;
    switch (fd.type) {
        case FieldType::U8:  raw = static_cast<std::uint64_t>(static_cast<std::uint8_t>(v)); len = 1; break;
        case FieldType::U16: raw = static_cast<std::uint64_t>(static_cast<std::uint16_t>(v)); len = 2; break;
        case FieldType::U32: raw = static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)); len = 4; break;
        case FieldType::I8:  raw = static_cast<std::uint64_t>(static_cast<std::uint8_t>(static_cast<std::int8_t>(v))); len = 1; break;
        case FieldType::I16: raw = static_cast<std::uint64_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(v))); len = 2; break;
        case FieldType::I32: raw = static_cast<std::uint64_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(v))); len = 4; break;
        case FieldType::Bool: raw = (v != 0.0) ? 1 : 0; len = 1; break;
        case FieldType::F32: {
            float f = static_cast<float>(v);
            std::uint32_t u;
            std::memcpy(&u, &f, sizeof(u));
            raw = u; len = 4;
            break;
        }
        case FieldType::F64: {
            double d = v;
            std::memcpy(&raw, &d, sizeof(d));
            len = 8;
            break;
        }
    }
    if (fd.bigEndian) {
        for (int i = 0; i < len; ++i) dst[i] = static_cast<std::uint8_t>((raw >> (8 * (len - 1 - i))) & 0xFF);
    } else {
        for (int i = 0; i < len; ++i) dst[i] = static_cast<std::uint8_t>((raw >> (8 * i)) & 0xFF);
    }
}

} // namespace

GenericBinaryProtocol::GenericBinaryProtocol(ProtocolSchema schema) : schema_(std::move(schema)) {
    // 归一化：允许 UI 只填 type，length 用默认值补齐。
    for (auto& f : schema_.fields) {
        if (f.byteLength <= 0) f.byteLength = fieldTypeBytes(f.type);
        if (!f.bigEndian) f.bigEndian = schema_.defaultBigEndian;
    }
}

std::vector<Frame> GenericBinaryProtocol::feed(const std::uint8_t* data, std::size_t len) {
    std::vector<Frame> out;
    if (!data || len == 0) return out;

    pending_.insert(pending_.end(), data, data + len);

    const std::size_t frameLen = schema_.totalBytes();
    if (frameLen == 0) return out;

    while (pending_.size() >= frameLen) {
        Frame f;
        f.name = schema_.name;
        f.seq  = seq_++;

        std::size_t off = 0;
        for (const auto& fd : schema_.fields) {
            if (fd.isPadding) { off += static_cast<std::size_t>(fd.byteLength); continue; }
            if (off + static_cast<std::size_t>(fd.byteLength) > frameLen) break;

            std::uint8_t buf[8] = {0};
            std::memcpy(buf, pending_.data() + off, static_cast<std::size_t>(fd.byteLength));
            double raw = fieldToDouble(fd, buf);
            double phys = raw * fd.scale + fd.offset;
            f.numeric.emplace_back(fd.name, phys);
            off += static_cast<std::size_t>(fd.byteLength);
        }

        f.raw.assign(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(frameLen));
        out.push_back(std::move(f));
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(frameLen));
    }
    return out;
}

void GenericBinaryProtocol::reset() {
    pending_.clear();
    seq_ = 0;
}

std::vector<std::uint8_t> GenericBinaryProtocol::encode(
    const std::string& cmd,
    const std::vector<std::pair<std::string, double>>& args) {
    std::vector<std::uint8_t> out(schema_.totalBytes(), 0);

    // 命令名若匹配某个字段名，也作为该字段的值（协议化发送的便捷路径）。
    std::size_t off = 0;
    for (const auto& fd : schema_.fields) {
        if (fd.isPadding) { off += static_cast<std::size_t>(fd.byteLength); continue; }

        double val = 0.0;
        bool found = false;
        for (const auto& kv : args) {
            if (kv.first == fd.name) { val = kv.second; found = true; break; }
        }
        if (!found && !cmd.empty() && cmd == fd.name) { val = 1.0; found = true; }
        if (!found) { off += static_cast<std::size_t>(fd.byteLength); continue; }

        writeField(fd, val, out.data() + off);
        off += static_cast<std::size_t>(fd.byteLength);
    }
    return out;
}

} // namespace sd