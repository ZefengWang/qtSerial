#include "testutil.hpp"

#include "../src/core/FakeSource.hpp"
#include "../src/core/IClock.hpp"
#include "../src/core/buffer/RingBuffer.hpp"
#include "../src/core/buffer/AppendBuffer.hpp"
#include "../src/core/buffer/DoubleBuffer.hpp"
#include "../src/service/EventBus.hpp"
#include "../src/service/Session.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace sd;

// 集成测试：验证 数据源 → 缓冲 → 攒批 → 事件总线 的完整数据通路。
// 覆盖文档 CH.04 / CH.07：数据可靠 + 集成测试。

// 1) 全链路零丢包 + 攒批切块：1MB 数据按 16KB 阈值切块发布，字节序完整。
static void testIntegrationFullPath() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(1 << 20); // 1MB
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);

    PortConfig cfg;
    CHECK(sess.open(cfg));

    // 注入 1MB 数据（超过 16KB 阈值，应切成 64 个 16KB 块）
    const std::size_t total = 1 << 20;
    std::vector<std::uint8_t> big(total);
    for (std::size_t i = 0; i < total; ++i) big[i] = static_cast<std::uint8_t>(i & 0xFF);
    fs.pushIncoming(big);

    std::size_t got = sess.poll();
    CHECK_EQ(got, total);
    CHECK_EQ(sess.rxDropped(), 0);                       // 零丢包
    CHECK_EQ(rb.bytesAvailable(), total);                // 全部进入缓冲
    CHECK_EQ(bus.recorded("rx").size(), total / Session::kDefaultBatch); // 64 块

    // 事件总线上收到的字节序必须与注入完全一致
    std::size_t idx = 0;
    for (const auto& chunk : bus.recorded("rx")) {
        for (auto b : chunk) {
            CHECK(b == static_cast<std::uint8_t>(idx & 0xFF));
            ++idx;
        }
    }
    CHECK_EQ(idx, total);
}

// 2) 事件订阅顺序投递：订阅者按发布顺序收到全部数据。
static void testIntegrationEventOrder() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(1024);
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus, "rx", 4); // 4 字节攒批阈值

    CHECK(sess.open(PortConfig{}));

    // 订阅者必须先注册，才能收到 poll 发布的数据
    std::vector<std::uint8_t> acc;
    bus.subscribe("rx", [&](const std::vector<std::uint8_t>& d) {
        acc.insert(acc.end(), d.begin(), d.end());
    });

    std::vector<std::uint8_t> in = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    fs.pushIncoming(in);
    sess.poll();
    sess.poll(); // 空数据，触发 flushBatch 冲刷剩余积压

    CHECK_EQ(acc.size(), 9);
    for (std::size_t i = 0; i < 9; ++i) CHECK(acc[i] == static_cast<std::uint8_t>(i + 1));
}

// 3) 多订阅者：同一主题多个订阅者都能收到数据。
static void testIntegrationMultiSubscriber() {
    EventBus bus;
    std::size_t a = 0, b = 0;
    bus.subscribe("rx", [&](const auto&) { ++a; });
    bus.subscribe("rx", [&](const auto&) { ++b; });
    std::vector<std::uint8_t> d = {7, 8, 9};
    bus.publish("rx", d);
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 1);
}

// 4) 多主题隔离：不同主题互不干扰。
static void testIntegrationTopicIsolation() {
    EventBus bus;
    std::size_t rx = 0, tx = 0;
    bus.subscribe("rx", [&](const auto&) { ++rx; });
    bus.subscribe("tx", [&](const auto&) { ++tx; });
    bus.publish("rx", {1});
    bus.publish("tx", {2});
    CHECK_EQ(rx, 1);
    CHECK_EQ(tx, 1);
    CHECK_EQ(bus.recorded("rx").size(), 1);
    CHECK_EQ(bus.recorded("tx").size(), 1);
}

// 5) open 失败：不进入已打开状态，不产生收发。
static void testIntegrationOpenFailure() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(64);
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);

    fs.setOpenResult(false);
    CHECK(!sess.open(PortConfig{}));
    CHECK(!sess.isOpen());
    CHECK_EQ(sess.send({1, 2}), 0); // 未打开，发送应拒绝
    CHECK_EQ(fs.written().size(), 0);
}

// 6) close 清理：关闭后清空积压，会话可重新打开。
static void testIntegrationCloseReopen() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(256);
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);

    CHECK(sess.open(PortConfig{}));
    std::vector<std::uint8_t> in = {1, 2, 3};
    fs.pushIncoming(in);
    sess.poll();

    sess.close();
    CHECK(!sess.isOpen());

    // 关闭后不再收发
    fs.pushIncoming({4, 5});
    CHECK_EQ(sess.poll(), 0);
    CHECK_EQ(sess.send({6}), 0);

    // 可重新打开
    CHECK(sess.open(PortConfig{}));
    CHECK(sess.isOpen());
}

// 7) 三种缓冲策略在同一 Session 下均可工作（策略可替换）。
static void testIntegrationBufferSwitch() {
    for (int mode = 0; mode < 3; ++mode) {
        FakeSource fs;
        EventBus bus;
        SteadyClock clock;
        IBufferStrategy* buf = nullptr;
        RingBuffer rb(64);
        AppendBuffer ab(64);
        DoubleBuffer db(64);
        if (mode == 0) buf = &rb;
        else if (mode == 1) buf = &ab;
        else buf = &db;

        Session sess(&fs, buf, &clock, bus, "rx", 8);
        CHECK(sess.open(PortConfig{}));
        std::vector<std::uint8_t> in = {0xAA, 0x55, 0x01, 0x02};
        fs.pushIncoming(in);
        CHECK_EQ(sess.poll(), 4);
        CHECK_EQ(buf->bytesAvailable(), 4);
        CHECK_EQ(buf->droppedCount(), 0);
    }
}

// 8) 发送路径：send 写入数据源并统计 TX。
static void testIntegrationSendPath() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(64);
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);

    CHECK(sess.open(PortConfig{}));
    std::vector<std::uint8_t> out = {0x01, 0x02, 0x03};
    CHECK_EQ(sess.send(out), 3);
    CHECK_EQ(sess.txBytes(), 3);
    CHECK_EQ(fs.written().size(), 1);
    CHECK_EQ(fs.written()[0].size(), 3);
}

int main() {
    testIntegrationFullPath();
    testIntegrationEventOrder();
    testIntegrationMultiSubscriber();
    testIntegrationTopicIsolation();
    testIntegrationOpenFailure();
    testIntegrationCloseReopen();
    testIntegrationBufferSwitch();
    testIntegrationSendPath();
    return testutil::summary("integration_test");
}