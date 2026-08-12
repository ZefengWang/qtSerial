#include "testutil.hpp"

#include "../src/core/buffer/RingBuffer.hpp"
#include "../src/core/buffer/AppendBuffer.hpp"
#include "../src/core/buffer/DoubleBuffer.hpp"

using namespace sd;

static void testRingBasic() {
    RingBuffer rb(16);
    std::uint8_t d[] = {1, 2, 3, 4, 5};
    rb.write(d, 5);
    CHECK_EQ(rb.bytesAvailable(), 5);
    CHECK_EQ(rb.droppedCount(), 0);
    auto out = rb.readAll();
    CHECK_EQ(out.size(), 5);
    CHECK(out[0] == 1 && out[4] == 5);
    CHECK_EQ(rb.bytesAvailable(), 0);
}

static void testRingOverflow() {
    RingBuffer rb(8);
    std::uint8_t d[] = {1, 2, 3, 4, 5, 6};
    rb.write(d, 6);
    rb.write(d, 6); // 累计 12 > 8，应丢弃最旧 4 字节
    CHECK_EQ(rb.bytesAvailable(), 8);
    CHECK_EQ(rb.droppedCount(), 4);
    // 保留的应是最后 8 字节：第二次写入的 6 + 第一次的后 2 (5,6)
    auto out = rb.readAll();
    CHECK_EQ(out.size(), 8);
    CHECK(out[0] == 5); // 第一次的 5
    CHECK(out[1] == 6); // 第一次的 6
    CHECK(out[2] == 1); // 第二次的 1
}

static void testRingWrap() {
    RingBuffer rb(4);
    std::uint8_t a[] = {1, 2, 3};
    rb.write(a, 3);
    rb.readAll(); // head 移到 3
    std::uint8_t b[] = {4, 5};
    rb.write(b, 2); // 写 2 字节，容量 4，前面还有 head=3 处空闲 1 格
    CHECK_EQ(rb.bytesAvailable(), 2);
    auto out = rb.readAll();
    CHECK_EQ(out.size(), 2);
    CHECK(out[0] == 4 && out[1] == 5);
}

static void testAppendBasic() {
    AppendBuffer ab(16);
    std::uint8_t d[] = {9, 8, 7};
    ab.write(d, 3);
    CHECK_EQ(ab.bytesAvailable(), 3);
    auto out = ab.readAll();
    CHECK_EQ(out.size(), 3);
    CHECK(out[0] == 9 && out[2] == 7);
    CHECK_EQ(ab.bytesAvailable(), 0);
}

static void testAppendTrim() {
    AppendBuffer ab(8);
    std::uint8_t d[] = {1, 2, 3, 4, 5, 6};
    ab.write(d, 6);
    ab.write(d, 6); // 累计 12 > 8，应从头部丢弃最旧 4 字节
    CHECK_EQ(ab.bytesAvailable(), 8);
    CHECK_EQ(ab.droppedCount(), 4);
    auto out = ab.readAll();
    CHECK_EQ(out.size(), 8);
    CHECK(out[0] == 5); // 丢弃了 1,2,3,4
}

static void testDoubleBuffer() {
    DoubleBuffer db(16);
    std::uint8_t d[] = {1, 2, 3};
    db.write(d, 3);
    CHECK_EQ(db.bytesAvailable(), 3);
    auto out = db.readAll();
    CHECK_EQ(out.size(), 3);
    CHECK(out[0] == 1 && out[2] == 3);
    CHECK_EQ(db.bytesAvailable(), 0);
}

int main() {
    testRingBasic();
    testRingOverflow();
    testRingWrap();
    testAppendBasic();
    testAppendTrim();
    testDoubleBuffer();
    return testutil::summary("buffer_test");
}