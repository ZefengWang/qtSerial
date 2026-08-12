#ifndef SRC_LAUNCHER_UI_MODE_HPP
#define SRC_LAUNCHER_UI_MODE_HPP

#include <QString>
#include <QStringList>

// ============================================================
// UiMode —— 单一可执行文件的 UI 模式管理。
//
// 一款程序，四种界面宿主（Qt Widgets / TUI / Web / QML）。
// 本模块负责：
//   1. UI 模式枚举与字符串映射（--ui=xx / 配置持久化用）。
//   2. 检测当前是否有图形环境（决定默认模式）。
//   3. 读写用户偏好的 UI 模式（QSettings 持久化）。
//   4. 以新模式重启自身进程（切换 UI 宿主）。
//
// 说明：不同 UI 宿主需要不同的 QGuiApplication 类型——
//   widgets -> QApplication
//   qml     -> QGuiApplication（QApplication 是其子类，可复用）
//   tui/web -> QCoreApplication（无头）
// 因此统一入口依据模式创建对应 Application 并分发。
// ============================================================

enum class UiMode {
    Auto,      // 自动：有图形选 Qt Widgets，无图形选 TUI
    QtWidgets, // 桌面（默认，功能最全）
    TUI,       // 无头终端文本界面
    Web,       // 浏览器界面（WebSocket + 静态页）
    QML        // QML 界面（需 Qt Quick，可选 GPU）
};

namespace ui_mode {

// 是否已启用 QML 宿主（编译期由 HAVE_QML 控制）。
bool qmlAvailable();

// 模式 -> 字符串（"auto"/"qt"/"tui"/"web"/"qml"）。
QString toString(UiMode m);

// 字符串 -> 模式；无法识别返回 UiMode::Auto。
UiMode fromString(const QString& s);

// 检测当前进程是否有可用图形环境。
// Linux/Unix 依据 DISPLAY/WAYLAND_DISPLAY；Windows/macOS 恒为 true。
bool hasGraphicsEnvironment();

// 解析命令行 --ui=xxx 参数；无则返回 UiMode::Auto。
// 同时返回剥离掉 --ui 参数后的剩余参数。
// 接收原始 argc/argv（在 QCoreApplication 实例化前也可安全调用）。
UiMode parseUiArg(int argc, char* argv[], QStringList* rest = nullptr);

// 读写用户保存的 UI 模式偏好（配置文件）。
// 返回 UiMode::Auto 表示从未设置过。
UiMode savedMode();
void saveMode(UiMode m);
void clearSavedMode();

// 以指定模式重启自身进程（QProcess::startDetached 后退出当前进程）。
// argsOverride 可覆盖传给新进程的参数（保留 --ui=xx）。
void restartWithMode(UiMode m, const QStringList& args = QStringList());

// 给 UI 展示用的模式中文名。
QString displayName(UiMode m);

// 三种可见模式名称列表（排除 Auto/QML 视编译而定）。
QStringList modeNames(bool includeQml);

} // namespace ui_mode

#endif // SRC_LAUNCHER_UI_MODE_HPP