#include "FakeSource.hpp"

#include <algorithm>

namespace sd {

void FakeSource::pushIncoming(const std::vector<std::uint8_t>& data) {
    incoming_.insert(incoming_.end(), data.begin(), data.end());
}

bool FakeSource::open(const PortConfig& cfg) {
    cfg_ = cfg;
    open_ = openResult_;
    if (!open_) lastError_ = "FakeSource: open rejected by test";
    return open_;
}

void FakeSource::close() {
    open_ = false;
    incoming_.clear();
}

std::vector<std::uint8_t> FakeSource::read() {
    std::vector<std::uint8_t> out = std::move(incoming_);
    incoming_.clear();
    return out;
}

std::size_t FakeSource::write(const std::vector<std::uint8_t>& data) {
    written_.push_back(data);
    return data.size();
}

std::vector<std::string> FakeSource::scanPorts() {
    return scanResult_;
}

std::string FakeSource::lastError() const { return lastError_; }

} // namespace sd