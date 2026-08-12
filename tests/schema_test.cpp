#include "testutil.hpp"

#include <cstring>
#include <memory>

#include "../src/protocol/GenericBinaryProtocol.hpp"
#include "../src/protocol/ProtocolRegistry.hpp"
#include "../src/service/FieldPool.hpp"
#include "../src/service/ViewManager.hpp"

using namespace sd;

// 构造一个典型 6 轴姿态 schema：imu header(padding) + wx,wy,wz (I16) + pad + accel(3xI16)
static ProtocolSchema makeImuSchema() {
    ProtocolSchema s;
    s.name = "imu";
    FieldDesc hdr; hdr.name = "hdr"; hdr.type = FieldType::U8; hdr.byteLength = 2; hdr.isPadding = true;
    FieldDesc wx;  wx.name = "wx"; wx.type = FieldType::I16; wx.scale = 0.01;  wx.unit = "rad/s";
    FieldDesc wy;  wy.name = "wy"; wy.type = FieldType::I16; wy.scale = 0.01;  wy.unit = "rad/s";
    FieldDesc wz;  wz.name = "wz"; wz.type = FieldType::I16; wz.scale = 0.01;  wz.unit = "rad/s";
    FieldDesc ax;  ax.name = "ax"; ax.type = FieldType::I16; ax.scale = 0.001; ax.unit = "g";
    FieldDesc ay;  ay.name = "ay"; ay.type = FieldType::I16;
    FieldDesc az;  az.name = "az"; az.type = FieldType::I16;
    s.fields = {hdr, wx, wy, wz, ax, ay, az};
    return s;
}

// ---------- GenericBinaryProtocol ----------
static void testBinaryParse() {
    auto schema = makeImuSchema();
    // 帧长 = 2 + 2*6 = 14
    CHECK_EQ(schema.totalBytes(), 14);
    GenericBinaryProtocol p(schema);

    // 构造一帧：hdr=0xAA55, wx=100*(-0.01)=-0.01? 用 100 -> 1.0 rad/s
    std::vector<std::uint8_t> frame = {
        0xAA, 0x55,            // hdr
        0x64, 0x00,            // wx = 100 (0x64) 小端 -> 1.0
        0x64, 0x00,            // wy = 1.0
        0x64, 0x00,            // wz = 1.0
        0xE8, 0x03,            // ax = 1000 (0x03E8) -> 1.0 g
        0x00, 0x00,            // ay = 0
        0x01, 0x00,            // az = 1
    };
    auto frames = p.feed(frame.data(), frame.size());
    CHECK_EQ(frames.size(), 1);
    CHECK(frames[0].name == "imu");
    bool found = false;
    CHECK_EQ(frames[0].numericValue("wx", &found), 1.0);
    CHECK(found);
    CHECK_EQ(frames[0].numericValue("ax", &found), 1.0);
    CHECK(found);
    CHECK_EQ(frames[0].numericValue("az"), 1.0);
    // padding 不产出字段
    CHECK_EQ(frames[0].numericValue("hdr", &found), 0.0);
    CHECK(!found);
}

static void testBinaryFragment() {
    auto schema = makeImuSchema();
    GenericBinaryProtocol p(schema);

    // 半帧喂入应无成帧，补齐后成一帧
    std::vector<std::uint8_t> half(schema.totalBytes() - 1, 0x00);
    half[0] = 0xAA; half[1] = 0x55;
    auto f1 = p.feed(half.data(), half.size());
    CHECK_EQ(f1.size(), 0);

    std::uint8_t last[1] = {0x00}; // 补最后一个字节
    auto f2 = p.feed(last, 1);
    CHECK_EQ(f2.size(), 1);
    (void)f2;
}

static void testBinaryMultiFrame() {
    auto schema = makeImuSchema();
    GenericBinaryProtocol p(schema);
    // 2 帧连续
    std::vector<std::uint8_t> two(schema.totalBytes() * 2, 0x00);
    two[0] = 0xAA; two[1] = 0x55;
    auto frames = p.feed(two.data(), two.size());
    CHECK_EQ(frames.size(), 2);
    CHECK(frames[1].seq > frames[0].seq);
}

static void testBinaryEncode() {
    auto schema = makeImuSchema();
    GenericBinaryProtocol p(schema);
    // 编码 wx=1.0 (scale 0.01 -> raw 100 = 0x64)
    auto bytes = p.encode("", {{"wx", 1.0}});
    CHECK_EQ(bytes.size(), schema.totalBytes());
    // 小端：第 2,3 字节为 0x64,0x00
    CHECK(bytes[2] == 0x64 && bytes[3] == 0x00);
}

// ---------- registerSchema ----------
static void testRegisterSchema() {
    ProtocolRegistry r;
    auto schema = makeImuSchema();
    r.registerSchema(schema);
    CHECK(r.contains("imu"));
    auto p = r.create("imu");
    CHECK(p != nullptr);
    CHECK(std::string(p->name()) == "imu");
    // 解析可用
    std::vector<std::uint8_t> frame(14, 0x00);
    frame[2] = 0x64; frame[3] = 0x00;
    auto frames = p->feed(frame.data(), frame.size());
    CHECK_EQ(frames.size(), 1);
    CHECK_EQ(frames[0].numericValue("wx"), 1.0);
}

// ---------- FieldPool ----------
static void testFieldPool() {
    FieldPool pool;
    CHECK(!pool.hasSchema());
    pool.setSchema(makeImuSchema());
    CHECK(pool.hasSchema());
    CHECK(pool.protocolName() == "imu");

    // 数据源剔除 padding：应剩 6 个
    CHECK_EQ(pool.sources().size(), 6);
    CHECK(pool.findSource("wx") != nullptr);
    CHECK(pool.findSource("hdr") == nullptr); // padding 不作为数据源

    // onFrame 更新最新值
    Frame f;
    f.numeric.emplace_back("wx", 3.5);
    f.numeric.emplace_back("ay", 9.8);
    pool.onFrame(f);
    const FieldSource* wx = pool.findSource("wx");
    CHECK(wx != nullptr && wx->lastValue == 3.5 && wx->hasData);
    const FieldSource* ay = pool.findSource("ay");
    CHECK(ay != nullptr && ay->lastValue == 9.8 && ay->hasData);
    // 未出现的字段 hasData 仍为 false
    const FieldSource* wz = pool.findSource("wz");
    CHECK(wz != nullptr && !wz->hasData);

    pool.clear();
    CHECK(!pool.hasSchema());
    CHECK(pool.sources().empty());
}

// ---------- ViewManager ----------
static void testViewManager() {
    ViewManager vm;
    CHECK(!vm.configured());
    CHECK(vm.type() == ViewType::None);

    vm.setType(ViewType::Wave);
    CHECK(vm.configured());
    CHECK(vm.type() == ViewType::Wave);

    // 添加多实例、多数据源
    auto id1 = vm.addView({"wx", "wy", "wz", "ax", "ay", "az"}, "六轴");
    CHECK(!id1.empty());
    auto id2 = vm.addView({"wx"}, "单轴");
    CHECK(!id2.empty());
    CHECK_EQ(vm.views().size(), 2);

    // 单字段绑定
    auto id3 = vm.addView({"az"}, "Z 轴");
    CHECK(!id3.empty());
    CHECK_EQ(vm.views().size(), 3);

    // 移除
    CHECK(vm.removeView(id2));
    CHECK_EQ(vm.views().size(), 2);
    CHECK(vm.findView(id2) == nullptr);
    CHECK(vm.findView(id1) != nullptr);

    // 互斥切换：清空旧实例
    vm.setType(ViewType::Table);
    CHECK(vm.views().empty());
    CHECK(vm.type() == ViewType::Table);
}

static void testViewValidation() {
    FieldPool pool;
    pool.setSchema(makeImuSchema());
    ViewManager vm;
    vm.setType(ViewType::Wave);
    vm.addView({"wx", "az"}, "ok");
    vm.addView({"wx", "nope", "az"}, "bad");
    auto invalid = vm.validateFields(pool);
    CHECK_EQ(invalid.size(), 1);
    CHECK(invalid[0] == "nope");
}

int main() {
    testBinaryParse();
    testBinaryFragment();
    testBinaryMultiFrame();
    testBinaryEncode();
    testRegisterSchema();
    testFieldPool();
    testViewManager();
    testViewValidation();
    return testutil::summary("schema_test");
}