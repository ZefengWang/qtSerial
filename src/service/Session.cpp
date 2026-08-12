#include "Session.hpp"

namespace sd {

Session::Session(DataSource*        source,
                 IBufferStrategy*   buffer,
                 IClock*            clock,
                 EventBus&          bus,
                 const std::string& rxTopic,
                 std::size_t        batchThreshold)
    : source_(source), buffer_(buffer), clock_(clock),
      bus_(bus), rxTopic_(rxTopic), batchThreshold_(batchThreshold) {}

Session::~Session() {
    if (open_) close();
}

bool Session::open(const PortConfig& cfg) {
    if (!source_) return false;
    if (source_->open(cfg)) {
        open_ = true;
        pending_.clear();
        return true;
    }
    return false;
}

void Session::close() {
    if (source_) source_->close();
    open_ = false;
    pending_.clear();
}

std::size_t Session::send(const std::vector<std::uint8_t>& data) {
    if (!open_ || !source_) return 0;
    std::size_t n = source_->write(data);
    txBytes_ += n;
    return n;
}

std::size_t Session::poll() {
    if (!open_ || !source_ || !buffer_) return 0;
    auto data = source_->read();
    std::size_t n = data.size();
    if (n == 0) {
        flushBatch(); // 无新数据时也冲刷积压，避免延迟
        return 0;
    }
    rxBytes_ += n;

    // 写入缓冲（生产）。
    buffer_->write(data.data(), data.size());

    // 攒批：满阈值即切块发布，避免逐字节信号风暴，也避免单次超大载荷占用过多内存。
    pending_.insert(pending_.end(), data.begin(), data.end());
    while (batchThreshold_ > 0 && pending_.size() >= batchThreshold_) {
        std::vector<std::uint8_t> chunk(pending_.begin(), pending_.begin() + batchThreshold_);
        bus_.publish(rxTopic_, chunk);
        pending_.erase(pending_.begin(), pending_.begin() + batchThreshold_);
    }
    return n;
}

void Session::flushBatch() {
    if (pending_.empty()) return;
    bus_.publish(rxTopic_, pending_);
    pending_.clear();
}

} // namespace sd