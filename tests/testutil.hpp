#ifndef TESTS_TEST_UTIL_HPP
#define TESTS_TEST_UTIL_HPP

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testutil {

// 极简断言工具：不依赖 Qt Test，可在纯 g++ 环境运行。
inline int failures = 0;
inline int checks = 0;

inline void check(bool cond, const char* expr, const char* file, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("  [FAIL] %s:%d  %s\n", file, line, expr);
    }
}

#define CHECK(expr) testutil::check((expr), #expr, __FILE__, __LINE__)

inline void checkEqImpl(std::size_t a, std::size_t b, const char* ea, const char* eb,
                        const char* file, int line) {
    ++checks;
    if (a != b) {
        ++failures;
        std::printf("  [FAIL] %s:%d  %s(=%zu) != %s(=%zu)\n",
                    file, line, ea, a, eb, b);
    }
}

#define CHECK_EQ(a, b) testutil::checkEqImpl((std::size_t)(a), (std::size_t)(b), #a, #b, __FILE__, __LINE__)

inline int summary(const std::string& name) {
    std::printf("%s: %d checks, %d failures\n", name.c_str(), checks, failures);
    return failures == 0 ? 0 : 1;
}

} // namespace testutil

#endif // TESTS_TEST_UTIL_HPP