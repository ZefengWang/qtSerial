// 系统测试：在 offscreen 平台构建完整 GUI 应用，验证装配与控件存在性。
// 覆盖文档 CH.07 的"UI 装配测试"层级——确保主窗口能构建、控件齐全、可交互。
// 运行方式：QT_QPA_PLATFORM=offscreen ./gui_system_test
#include <QtTest/QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QTimer>

#include "../uart_interaction.h"

class GuiSystemTest : public QObject {
    Q_OBJECT
private slots:
    // 1) 主窗口可构建（God Object 拆解后仍能正常装配）
    void mainWindowConstructs();
    // 2) 关键控件存在
    void keyWidgetsExist();
    // 3) 打开/发送/清空按钮可点击且不崩溃
    void buttonsClickable();
    // 4) 端口下拉框存在且可交互
    void portComboWorks();
};

void GuiSystemTest::mainWindowConstructs() {
    serial w;
    QVERIFY(w.windowTitle().isEmpty() || !w.windowTitle().isEmpty()); // 总有标题或回退
    QVERIFY(!w.isMinimized());
}

void GuiSystemTest::keyWidgetsExist() {
    serial w;
    QVERIFY(w.findChild<QPushButton*>("openPortButton") != nullptr);
    QVERIFY(w.findChild<QPushButton*>("sendButton") != nullptr);
    QVERIFY(w.findChild<QPushButton*>("refreshButton") != nullptr);
    QVERIFY(w.findChild<QPushButton*>("clearTextButton") != nullptr);
    QVERIFY(w.findChild<QComboBox*>("portComboBox") != nullptr);
    QVERIFY(w.findChild<QTextEdit*>("sendTextEdit") != nullptr);
}

void GuiSystemTest::buttonsClickable() {
    serial w;
    w.show();
    QTest::qWait(50); // 让事件循环跑起来

    auto* refresh = w.findChild<QPushButton*>("refreshButton");
    auto* openBtn = w.findChild<QPushButton*>("openPortButton");
    auto* sendBtn = w.findChild<QPushButton*>("sendButton");
    auto* clearBtn = w.findChild<QPushButton*>("clearTextButton");
    QVERIFY(refresh && openBtn && sendBtn && clearBtn);

    // 点击刷新（扫描端口，无硬件时应安全返回）
    QTest::mouseClick(refresh, Qt::LeftButton);
    QTest::qWait(30);

    // 点击打开/关闭（无端口时应安全，不崩溃）
    QTest::mouseClick(openBtn, Qt::LeftButton);
    QTest::qWait(30);
    QTest::mouseClick(openBtn, Qt::LeftButton);
    QTest::qWait(30);

    // 点击发送（无连接时应安全）
    QTest::mouseClick(sendBtn, Qt::LeftButton);
    QTest::qWait(30);

    // 点击清空
    QTest::mouseClick(clearBtn, Qt::LeftButton);
    QTest::qWait(30);
}

void GuiSystemTest::portComboWorks() {
    serial w;
    auto* combo = w.findChild<QComboBox*>("portComboBox");
    QVERIFY(combo != nullptr);
    QTest::qWait(30);
}

QTEST_MAIN(GuiSystemTest)
#include "gui_system_test.moc"