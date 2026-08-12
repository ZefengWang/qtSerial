#ifndef SRC_CORE_FRAME_HPP
#define SRC_CORE_FRAME_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sd {

// 一帧解析后的结构化数据。
// 波形、3D、协议解析等所有上层展示都消费 Frame，而非原始字节。
// 设计为"宽表"：数值字段（供波形/3D 取点）+ 文本字段 + 原始字节。
struct Frame {
    // 帧类型名（如 "sample", "status", "error"）。
    std::string name;

    // 数值字段：{字段名, 值}。波形图按字段名取序。
    std::vector<std::pair<std::string, double>> numeric;

    // 文本字段：{字段名, 字符串}。
    std::vector<std::pair<std::string, std::string>> text;

    // 原始字节（该帧对应的原始数据，便于回显/调试）。
    std::vector<std::uint8_t> raw;

    // 单调序号，供上层保证顺序。
    std::uint64_t seq = 0;

    // 便捷：取数值字段（不存在时返回 0，并置 found=false）。
    double numericValue(const std::string& key, bool* found = nullptr) const {
        for (const auto& kv : numeric) {
            if (kv.first == key) {
                if (found) *found = true;
                return kv.second;
            }
        }
        if (found) *found = false;
        return 0.0;
    }

    // 便捷：取文本字段（不存在时返回空串）。
    std::string textValue(const std::string& key) const {
        for (const auto& kv : text) {
            if (kv.first == key) return kv.second;
        }
        return {};
    }
};

} // namespace sd

#endif // SRC_CORE_FRAME_HPP