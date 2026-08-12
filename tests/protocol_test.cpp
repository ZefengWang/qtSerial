#include "testutil.hpp"

#include <cstring>
#include <memory>
#include <string>

#include "../src/protocol/LineProtocol.hpp"
#include "../src/protocol/CsvProtocol.hpp"
#include "../src/protocol/ProtocolRegistry.hpp"
#include "../src/service/EventBus.hpp"
#include "../src/service/ProtocolEngine.hpp"

using namespace sd;

// ---------- LineProtocol ----------
static void testLineBasic() {
    LineProtocol p;
    const char* in = "set=1 name=hello\nset=2\n";
    auto frames = p.feed(reinterpret_cast<const std::uint8_t*>(in), std::strlen(in));
    CHECK_EQ(frames.size(), 2);
    CHECK(frames[0].name == "line");
    bool found = false;
    CHECK_EQ(frames[0].numericValue("set", &found), 1.0);
    CHECK(found);
    CHECK(frames[0].textValue("name") == "hello");
    CHECK_EQ(frames[1].numericValue("set", &found), 2.0);
}

static void testLineFragment() {
    LineProtocol p;
    const char* a = "set=2"; // 半个指令，无换行
    const char* b = "\n";    // 换行补齐
    auto f1 = p.feed(reinterpret_cast<const std::uint8_t*>(a), std::strlen(a));
    CHECK_EQ(f1.size(), 0); // 未到换行，无帧
    auto f2 = p.feed(reinterpret_cast<const std::uint8_t*>(b), std::strlen(b));
    CHECK_EQ(f2.size(), 1);
    CHECK_EQ(f2[0].numericValue("set"), 2.0);
}

static void testLineCrlfAndComment() {
    LineProtocol p;
    const char* in = "# comment\nset=3\r\n\n";
    auto frames = p.feed(reinterpret_cast<const std::uint8_t*>(in), std::strlen(in));
    CHECK_EQ(frames.size(), 1); // 注释与空行被跳过
    CHECK_EQ(frames[0].numericValue("set"), 3.0);
}

static void testLineEncode() {
    LineProtocol p;
    auto bytes = p.encode("go", {{"a", 1.5}, {"b", 2.0}});
    std::string s(bytes.begin(), bytes.end());
    CHECK(s == "go a=1.500000 b=2.000000\n");
}

// ---------- CsvProtocol ----------
static void testCsvHeaderParse() {
    CsvProtocol p;
    const char* in = "t,volt,curr\n0,3.3,0.5\n1,3.4,0.6\n";
    auto frames = p.feed(reinterpret_cast<const std::uint8_t*>(in), std::strlen(in));
    CHECK_EQ(frames.size(), 2);
    CHECK(frames[0].name == "sample");
    bool found = false;
    CHECK_EQ(frames[0].numericValue("volt", &found), 3.3);
    CHECK(found);
    CHECK_EQ(frames[0].numericValue("curr"), 0.5);
    CHECK_EQ(frames[1].numericValue("volt"), 3.4);
}

static void testCsvNamedAndFallback() {
    CsvProtocol p;
    // 无表头：列名 c0,c1
    auto f1 = p.feed(reinterpret_cast<const std::uint8_t*>("1,2\n"), 4);
    CHECK_EQ(f1.size(), 1);
    CHECK(f1[0].name == "csv");
    CHECK_EQ(f1[0].numericValue("c0"), 1.0);
    CHECK_EQ(f1[0].numericValue("c1"), 2.0);

    // 行首非数值 => 帧名
    auto f2 = p.feed(reinterpret_cast<const std::uint8_t*>("err,12\n"), 7);
    CHECK_EQ(f2.size(), 1);
    CHECK(f2[0].name == "err");
    CHECK_EQ(f2[0].numericValue("c0"), 12.0);
}

static void testCsvEncode() {
    CsvProtocol p;
    auto bytes = p.encode("go", {{"x", 1.0}, {"y", 2.0}});
    std::string s(bytes.begin(), bytes.end());
    CHECK(s == "go,1.000000,2.000000\n");
}

// ---------- ProtocolRegistry ----------
static void testRegistryBuiltins() {
    ProtocolRegistry r;
    CHECK(r.contains("line"));
    CHECK(r.contains("csv"));
    auto names = r.names();
    CHECK_EQ(names.size(), 2);
    CHECK(r.create("line") != nullptr);
    CHECK(r.create("nope") == nullptr);
}

// 自定义协议扩展：注册一个把每个字节当作一帧的协议。
class ByteProtocol : public IProtocol {
public:
    std::vector<Frame> feed(const std::uint8_t* d, std::size_t n) override {
        std::vector<Frame> out;
        for (std::size_t i = 0; i < n; ++i) {
            Frame f;
            f.name = "byte";
            f.numeric.emplace_back("v", static_cast<double>(d[i]));
            out.push_back(std::move(f));
        }
        return out;
    }
    void reset() override {}
    const char* name() const override { return "byte"; }
    std::vector<std::uint8_t> encode(const std::string& cmd,
                                     const std::vector<std::pair<std::string, double>>& args) override {
        std::vector<std::uint8_t> out;
        for (const auto& kv : args) out.push_back(static_cast<std::uint8_t>(kv.second));
        return out;
    }
};

static void testRegistryCustom() {
    ProtocolRegistry r;
    r.registerFactory("byte", [] { return std::make_unique<ByteProtocol>(); });
    CHECK(r.contains("byte"));
    auto p = r.create("byte");
    CHECK(p != nullptr);
    CHECK(std::string(p->name()) == "byte");
}

// ---------- ProtocolEngine：rx -> frames ----------
static void testEngineLine() {
    EventBus bus;
    ProtocolRegistry reg;
    ProtocolEngine eng(bus, reg);

    CHECK(eng.select("line"));
    CHECK(eng.current() == "line");
    CHECK_EQ(eng.available().size(), 2);

    // 模拟 Session 发布 rx 数据
    const char* in = "set=1\nset=2\n";
    std::vector<std::uint8_t> v(in, in + std::strlen(in));
    bus.publish("rx", v);

    const auto& frames = bus.recordedFrames("frames");
    CHECK_EQ(frames.size(), 2);
    CHECK_EQ(frames[0].numericValue("set"), 1.0);
    CHECK_EQ(frames[1].numericValue("set"), 2.0);
    CHECK(frames[1].seq > frames[0].seq); // 序号递增
}

static void testEngineSelectUnknown() {
    EventBus bus;
    ProtocolRegistry reg;
    ProtocolEngine eng(bus, reg);
    CHECK(!eng.select("nope"));
    CHECK(eng.current() == ""); // 未加载，保持原状态
}

static void testEngineEncode() {
    EventBus bus;
    ProtocolRegistry reg;
    ProtocolEngine eng(bus, reg);
    CHECK(eng.select("line"));
    auto bytes = eng.build("go", {{"a", 7.0}});
    std::string s(bytes.begin(), bytes.end());
    CHECK(s == "go a=7.000000\n");
}

int main() {
    testLineBasic();
    testLineFragment();
    testLineCrlfAndComment();
    testLineEncode();
    testCsvHeaderParse();
    testCsvNamedAndFallback();
    testCsvEncode();
    testRegistryBuiltins();
    testRegistryCustom();
    testEngineLine();
    testEngineSelectUnknown();
    testEngineEncode();
    return testutil::summary("protocol_test");
}