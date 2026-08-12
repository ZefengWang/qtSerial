#include "EventBus.hpp"

#include <algorithm>

namespace sd {

std::size_t EventBus::subscribe(const std::string& topic, DataHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto id = nextId_++;
    subs_[topic].push_back({id, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(const std::string& topic, std::size_t subId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subs_.find(topic);
    if (it == subs_.end()) return;
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [subId](const Sub& s) { return s.id == subId; }),
              vec.end());
}

void EventBus::publish(const std::string& topic, const std::vector<std::uint8_t>& data) {
    std::vector<DataHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        record_[topic].push_back(data); // 录制（始终记录，供测试）
        auto it = subs_.find(topic);
        if (it == subs_.end()) return;
        for (const auto& s : it->second) handlers.push_back(s.handler);
    }
    for (auto& h : handlers) h(data); // 锁外调用，避免回调内再 publish 死锁
}

const std::vector<std::vector<std::uint8_t>>& EventBus::recorded(const std::string& topic) const {
    static const std::vector<std::vector<std::uint8_t>> empty;
    auto it = record_.find(topic);
    return it == record_.end() ? empty : it->second;
}

std::size_t EventBus::subscribeFrame(const std::string& topic, FrameHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto id = nextId_++;
    frameSubs_[topic].push_back({id, std::move(handler)});
    return id;
}

void EventBus::unsubscribeFrame(const std::string& topic, std::size_t subId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = frameSubs_.find(topic);
    if (it == frameSubs_.end()) return;
    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [subId](const FrameSub& s) { return s.id == subId; }),
              vec.end());
}

void EventBus::publishFrame(const std::string& topic, const Frame& frame) {
    std::vector<FrameHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frameRecord_[topic].push_back(frame); // 录制
        auto it = frameSubs_.find(topic);
        if (it == frameSubs_.end()) return;
        for (const auto& s : it->second) handlers.push_back(s.handler);
    }
    for (auto& h : handlers) h(frame);
}

const std::vector<Frame>& EventBus::recordedFrames(const std::string& topic) const {
    static const std::vector<Frame> empty;
    auto it = frameRecord_.find(topic);
    return it == frameRecord_.end() ? empty : it->second;
}

} // namespace sd