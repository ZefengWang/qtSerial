#ifndef SRC_CORE_ICLOCK_HPP
#define SRC_CORE_ICLOCK_HPP

#include <cstdint>

namespace sd {

// 时钟抽象：让"定时发送、超时、性能统计"等时间相关逻辑可在测试中注入虚拟时钟，免等待真实时间。
class IClock {
public:
    virtual ~IClock() = default;

    // 单调递增时间，单位毫秒（用于计时/超时判断）。
    virtual std::int64_t monotonicMs() const = 0;
};

// 真实时钟：基于稳定时钟的实现。
class SteadyClock : public IClock {
public:
    std::int64_t monotonicMs() const override;
};

} // namespace sd

#endif // SRC_CORE_ICLOCK_HPP