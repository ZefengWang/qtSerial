#ifndef SRC_LAUNCHER_UI_RUNNERS_HPP
#define SRC_LAUNCHER_UI_RUNNERS_HPP

class QCoreApplication;
class QGuiApplication;
class QApplication;

// ============================================================
// UI 宿主运行入口声明（统一入口 main 分发调用）。
//
// tui/web 需要 QCoreApplication（或 QApplication 子类）；
// qml 需要至少 QGuiApplication；widgets 需要 QApplication。
// 各函数返回事件循环退出码。
// ============================================================

// 终端无头界面。app 需为 QCoreApplication（可传 QApplication）。
int runTui(QCoreApplication& app);

// 浏览器后端。app 需为 QCoreApplication（可传 QApplication）。
// openBrowser=true 时启动后自动用系统默认浏览器打开页面。
int runWeb(QCoreApplication& app, int port, bool openBrowser = true);

// QML 界面。仅当编译期启用了 Qt Quick（HAVE_QML）时可用。
// app 需为 QGuiApplication（QApplication 是其子类，可传）。
#ifdef HAVE_QML
int runQml(QGuiApplication& app);
#endif

#endif // SRC_LAUNCHER_UI_RUNNERS_HPP