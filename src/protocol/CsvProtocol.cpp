#include "CsvProtocol.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace sd {

namespace {

// 按逗号切分，容忍回车。
std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : line) {
        if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else if (ch != '\r') {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out;
}

bool tryDouble(const std::string& s, double* out) {
    if (s.empty()) return false;
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (end && *end == '\0') {
        if (out) *out = d;
        return true;
    }
    return false;
}

} // namespace

std::vector<Frame> CsvProtocol::feed(const std::uint8_t* data, std::size_t len) {
    std::vector<Frame> out;
    if (!data || len == 0) return out;

    pending_.append(reinterpret_cast<const char*>(data), len);

    std::size_t start = 0;
    while (true) {
        std::size_t nl = pending_.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = pending_.substr(start, nl - start);
        start = nl + 1;
        if (line.empty()) continue;

        auto cells = splitCsv(line);
        if (cells.empty()) continue;

        // 判断该行是否存在数值单元格。
        bool anyNumeric = false;
        for (const auto& c : cells) {
            double d = 0;
            if (tryDouble(c, &d)) { anyNumeric = true; break; }
        }

        // 首条"全文本"行 => 表头（如 "t,volt,curr"）。
        if (!haveHeader_ && !anyNumeric) {
            header_ = cells;
            haveHeader_ = true;
            continue;
        }

        Frame f;
        std::size_t colOffset = 0;
        bool firstNum = false;
        double dummy = 0;
        if (tryDouble(cells[0], &dummy)) firstNum = true;

        if (!firstNum) {
            f.name = cells[0]; // 行首非数值 => 帧名（如 "err,12" -> name="err"）
            colOffset = 1;
        } else {
            f.name = haveHeader_ ? "sample" : "csv";
        }
        f.raw.assign(line.begin(), line.end());

        for (std::size_t i = colOffset; i < cells.size(); ++i) {
            double v = 0;
            if (!tryDouble(cells[i], &v)) continue;
            std::string key;
            if (haveHeader_ && (i - colOffset) < header_.size()) {
                key = header_[i - colOffset];
            } else {
                key = "c" + std::to_string(i - colOffset);
            }
            f.numeric.emplace_back(key, v);
        }
        out.push_back(std::move(f));
    }

    pending_.erase(0, start);
    return out;
}

void CsvProtocol::reset() {
    pending_.clear();
    haveHeader_ = false;
    header_.clear();
}

std::vector<std::uint8_t> CsvProtocol::encode(
    const std::string& cmd,
    const std::vector<std::pair<std::string, double>>& args) {
    std::string line = cmd;
    for (const auto& kv : args) {
        line += "," + std::to_string(kv.second);
    }
    line += "\n";
    std::vector<std::uint8_t> out(line.begin(), line.end());
    return out;
}

} // namespace sd