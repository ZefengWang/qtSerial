#include "LineProtocol.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace sd {

std::vector<Frame> LineProtocol::feed(const std::uint8_t* data, std::size_t len) {
    std::vector<Frame> out;
    if (!data || len == 0) return out;

    pending_.append(reinterpret_cast<const char*>(data), len);

    // 逐行切分（兼容 \r\n 与 \n）。
    std::size_t start = 0;
    while (true) {
        std::size_t nl = pending_.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = pending_.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = nl + 1;

        if (line.empty() || line[0] == '#') continue; // 空行/注释跳过

        Frame f;
        f.name = "line";
        f.raw.assign(line.begin(), line.end());

        // 解析 key=value 键值对（空格分隔）。
        std::istringstream iss(line);
        std::string tok;
        while (iss >> tok) {
            std::size_t eq = tok.find('=');
            if (eq == std::string::npos) {
                f.text.emplace_back("text", tok);
                continue;
            }
            std::string key = tok.substr(0, eq);
            std::string val = tok.substr(eq + 1);
            char* end = nullptr;
            double d = std::strtod(val.c_str(), &end);
            if (end && *end == '\0' && !val.empty()) {
                f.numeric.emplace_back(key, d);
            } else {
                f.text.emplace_back(key, val);
            }
        }
        out.push_back(std::move(f));
    }

    pending_.erase(0, start);
    return out;
}

void LineProtocol::reset() { pending_.clear(); }

std::vector<std::uint8_t> LineProtocol::encode(
    const std::string& cmd,
    const std::vector<std::pair<std::string, double>>& args) {
    std::string line = cmd;
    for (const auto& kv : args) {
        line += " " + kv.first + "=" + std::to_string(kv.second);
    }
    line += "\n";
    std::vector<std::uint8_t> out(line.begin(), line.end());
    return out;
}

} // namespace sd