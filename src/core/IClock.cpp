#include "IClock.hpp"

#include <chrono>

namespace sd {

static std::int64_t nowMs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::int64_t SteadyClock::monotonicMs() const {
    return nowMs();
}

} // namespace sd