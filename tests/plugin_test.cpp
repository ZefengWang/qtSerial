#include "testutil.hpp"

#include "../src/plugin/PluginRegistry.hpp"
#include "../src/plugin/IPlugin.hpp"
#include "../src/plugin/IViewHost.hpp"

#include <memory>

using namespace sd;

// 测试用最小插件
class NullViewHost : public IViewHost {
public:
    QWidget* asWidget() override { return nullptr; }
    QQuickItem* asQmlItem() override { return nullptr; }
};

class TestPlugin : public IPlugin {
public:
    std::string id() const override { return "test"; }
    std::string name() const override { return "Test Plugin"; }
    IViewHost* view() override { return &view_; }
    void onData(const std::vector<std::uint8_t>& d) override { ++received_; lastLen_ = d.size(); }
    void onActivate() override { ++activated_; }
    void onDeactivate() override { ++deactivated_; }

    int received_ = 0;
    std::size_t lastLen_ = 0;
    int activated_ = 0;
    int deactivated_ = 0;
    NullViewHost view_;
};

static void testRegistry() {
    PluginRegistry reg;
    auto p = std::make_unique<TestPlugin>();
    auto* raw = p.get();
    reg.add(std::move(p));

    CHECK(reg.find("test") == raw);
    CHECK(reg.find("nope") == nullptr);
    CHECK_EQ(reg.all().size(), 1);

    CHECK(reg.activate("test"));
    CHECK(raw->activated_ == 1);
    CHECK(!reg.activate("nope"));
    CHECK(reg.deactivate("test"));
    CHECK(raw->deactivated_ == 1);

    std::vector<std::uint8_t> d = {1, 2, 3};
    reg.broadcast(d);
    CHECK(raw->received_ == 1);
    CHECK_EQ(raw->lastLen_, 3);
}

int main() {
    testRegistry();
    return testutil::summary("plugin_test");
}