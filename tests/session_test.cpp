#include "testutil.hpp"

#include "../src/core/FakeSource.hpp"
#include "../src/service/EventBus.hpp"
#include "../src/service/Session.hpp"
#include "../src/core/buffer/RingBuffer.hpp"

#include <cstdint>

using namespace sd;

static void testFakeSource() {
    FakeSource fs;
    fs.setScanResult({"COM1", "COM2"});
    auto ports = fs.scanPorts();
    CHECK_EQ(ports.size(), 2);
    CHECK(ports[0] == "COM1");

    PortConfig cfg;
    cfg.name = "COM1";
    cfg.baudRate = 115200;
    CHECK(fs.open(cfg));
    CHECK(fs.isOpen());

    std::vector<std::uint8_t> in = {0xAA, 0x55, 0x01};
    fs.pushIncoming(in);
    auto got = fs.read();
    CHECK_EQ(got.size(), 3);
    CHECK(got[0] == 0xAA);

    std::vector<std::uint8_t> out = {0x11};
    fs.write(out);
    CHECK_EQ(fs.written().size(), 1);
}

static void testEventBus() {
    EventBus bus;
    const std::string topic = "rx";
    std::size_t received = 0;
    bus.subscribe(topic, [&](const std::vector<std::uint8_t>&) { ++received; });

    std::vector<std::uint8_t> d = {1, 2};
    bus.publish(topic, d);
    bus.publish(topic, d);
    CHECK_EQ(received, 2);
    CHECK_EQ(bus.recorded(topic).size(), 2);
}

static void testEventBusUnsubscribe() {
    EventBus bus;
    std::size_t a = 0, b = 0;
    auto idA = bus.subscribe("rx", [&](const auto&) { ++a; });
    bus.subscribe("rx", [&](const auto&) { ++b; });
    std::vector<std::uint8_t> d = {1};
    bus.publish("rx", d);
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 1);
    bus.unsubscribe("rx", idA);
    bus.publish("rx", d);
    CHECK_EQ(a, 1); // 退订后不再触发
    CHECK_EQ(b, 2);
}

static void testSessionNoLoss() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(1 << 20); // 1MB
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);

    PortConfig cfg;
    sess.open(cfg);

    // 注入 1MB 数据
    std::vector<std::uint8_t> big(1 << 20, 0xAB);
    fs.pushIncoming(big);
    std::size_t got = sess.poll();
    CHECK_EQ(got, 1 << 20);
    CHECK_EQ(sess.rxDropped(), 0); // 零丢包
    CHECK_EQ(rb.bytesAvailable(), 1 << 20);
    CHECK_EQ(bus.recorded("rx").size(), (1 << 20) / Session::kDefaultBatch); // 攒批次
}

static void testSessionSend() {
    FakeSource fs;
    EventBus bus;
    RingBuffer rb(1024);
    SteadyClock clock;
    Session sess(&fs, &rb, &clock, bus);
    PortConfig cfg;
    sess.open(cfg);

    std::vector<std::uint8_t> out = {0x01, 0x02};
    std::size_t n = sess.send(out);
    CHECK_EQ(n, 2);
    CHECK_EQ(sess.txBytes(), 2);
    CHECK_EQ(fs.written().size(), 1);
}

int main() {
    testFakeSource();
    testEventBus();
    testEventBusUnsubscribe();
    testSessionNoLoss();
    testSessionSend();
    return testutil::summary("session_test");
}