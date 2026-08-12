// PortMonitor 单元测试：用注入的假扫描器验证热插拔检测逻辑，
// 不依赖真实 /dev 硬件。offscreen 平台运行。
#include <QtTest/QtTest>
#include <QStringList>

#include <functional>

#include "../src/io/PortMonitor.hpp"

class PortMonitorTest : public QObject {
    Q_OBJECT
private slots:
    void detectsAddAndRemove();
    void toggleEnable();
};

// 用可变列表模拟"串口集合"，验证添加/移除事件。
void PortMonitorTest::detectsAddAndRemove() {
    QStringList current;
    current << "/dev/ttyUSB0" << "/dev/ttyUSB1";

    PortMonitor pm([&] { return current; });
    pm.setEnabled(false); // 先关推送，避免初始 compare 干扰

    QStringList added, removed;
    QObject::connect(&pm, &PortMonitor::portAdded, [&](const QString& p) { added << p; });
    QObject::connect(&pm, &PortMonitor::portRemoved, [&](const QString& p) { removed << p; });

    pm.refresh(); // 初始：两端口
    QCOMPARE(pm.ports().size(), 2);

    // 新增一个
    current << "/dev/ttyACM0";
    pm.refresh();
    QCOMPARE(added.size(), 1);
    QCOMPARE(added.last(), QString("/dev/ttyACM0"));
    QCOMPARE(pm.ports().size(), 3);

    // 移除两个中的一个
    current.removeAll("/dev/ttyUSB1");
    pm.refresh();
    QCOMPARE(removed.size(), 1);
    QCOMPARE(removed.last(), QString("/dev/ttyUSB1"));
    QCOMPARE(pm.ports().size(), 2);

    // 无变化时无事件
    int beforeAdded = added.size();
    int beforeRemoved = removed.size();
    pm.refresh();
    QCOMPARE(added.size(), beforeAdded);
    QCOMPARE(removed.size(), beforeRemoved);
}

void PortMonitorTest::toggleEnable() {
    QStringList current;
    current << "/dev/ttyUSB0";
    PortMonitor pm([&] { return current; });
    QVERIFY(!pm.isEnabled());
    pm.setEnabled(true);
    QVERIFY(pm.isEnabled());
    pm.setEnabled(false);
    QVERIFY(!pm.isEnabled());
    // 关闭后再刷新不崩溃
    current << "/dev/ttyACM0";
    pm.refresh();
    QCOMPARE(pm.ports().size(), 2);
}

QTEST_GUILESS_MAIN(PortMonitorTest)
#include "port_monitor_test.moc"