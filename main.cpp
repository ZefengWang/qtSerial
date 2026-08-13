#include "uart_interaction.h"
#include "thememanager.h"
#include "languagemanager.h"
#include "launcher/UiMode.hpp"
#include "launcher/UiModePicker.hpp"
#include "launcher/UiRunners.hpp"

#include <QApplication>
#include <QIcon>
#include <QSettings>
#include <QStringList>
#include <QProcessEnvironment>
#include <QProcess>
#include <QDebug>

// Version injected at build time from the git tag (APP_VERSION macro).
#ifndef APP_VERSION
#define APP_VERSION "2.1.0"
#endif

namespace {

// 让用户选择 UI 模式（有图形环境时弹出）。返回模式；-1 表示用户取消。
// remember 为 true 时持久化选择，下次启动不再询问。
int pickUiMode(UiMode* picked, bool* remember) {
    UiModePicker picker;
    if (picker.exec() != QDialog::Accepted)
        return -1;
    *picked = ui_mode::fromString(picker.selectedMode());
    *remember = picker.rememberChoice();
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
  // 解析命令行参数：--ui=xx 显式指定模式，其余参数保留。
  // 直接解析原始 argv（QApplication 尚未创建）。
  QStringList cliArgs;
  UiMode requested = ui_mode::parseUiArg(argc, argv, &cliArgs);

  // 是否显式指定了 --ui（覆盖自动/已保存）。
  const bool explicitUi = (requested != UiMode::Auto);

  // 是否可用于承载 GUI（弹选择框 / 桌面 / QML）。
  const bool hasGui = ui_mode::hasGraphicsEnvironment();

  // 决定最终模式：
  //   1) 命令行显式指定 -> 用之
  //   2) 无图形环境 -> TUI（QCoreApplication，无 GUI）
  //   3) 有图形 且有已保存偏好 -> 用之
  //   4) 有图形 且无偏好（首次）-> 弹选择框
  UiMode mode = UiMode::QtWidgets; // 兜底默认

  if (explicitUi) {
    mode = requested;
  } else if (!hasGui) {
    mode = UiMode::TUI; // 无头环境默认 TUI
    qInfo().noquote() << "No graphics environment detected, using TUI.";
  } else {
    UiMode saved = ui_mode::savedMode();
    if (saved != UiMode::Auto) {
      mode = saved;
    }
    // 无偏好：停留默认 QtWidgets，下方统一用 QApplication 后弹选择框。
  }

  // ============================================================
  // 无图形环境：全程 QCoreApplication（无法创建 QApplication）。
  // 仅支持 TUI / Web 两种无头宿主；显式指定 qt/qml 也回退 TUI。
  // ============================================================
  if (!hasGui) {
    if (mode == UiMode::Web) {
      QCoreApplication app(argc, argv);
      app.setApplicationName("serial-debug-web");
      int port = 8080;
      bool openBrowser = true;
      for (int i = 0; i < cliArgs.size(); ++i) {
        if (cliArgs[i] == "--port" && i + 1 < cliArgs.size()) port = cliArgs[i + 1].toInt();
        if (cliArgs[i] == "--no-browser") openBrowser = false;
      }
      // 无头环境没有图形浏览器，默认不自动打开；除非显式 --ui=web 且 --open-browser。
      if (!explicitUi) openBrowser = false;
      return runWeb(app, port, openBrowser);
    }
    // 其它（含 QML/Widgets，无图形不可用）一律回退 TUI。
    QCoreApplication app(argc, argv);
    app.setApplicationName("serial-debug-tui");
    qInfo().noquote() << "No graphics environment; falling back to TUI (mode=" << ui_mode::toString(mode) << ").";
    return runTui(app);
  }

  // ============================================================
  // 有图形环境：创建唯一的 QApplication，承载选择框与所有宿主。
  // TUI/Web 在 QApplication 下同样可运行（其子类关系）。
  // ============================================================
  QApplication a(argc, argv);
  a.setApplicationName("serial-debug");
  a.setOrganizationName("SerialDebug");

  // 首次启动（无 --ui、无已保存偏好）弹选择框。
  if (!explicitUi && ui_mode::savedMode() == UiMode::Auto) {
    UiMode picked = mode;
    bool remember = false;
    if (pickUiMode(&picked, &remember) == 0) {
      mode = picked;
      if (remember) ui_mode::saveMode(mode);
    }
  }

  switch (mode) {
    case UiMode::TUI:
      return runTui(a);
    case UiMode::Web: {
      int port = 8080;
      bool openBrowser = true;
      for (int i = 0; i < cliArgs.size(); ++i) {
        if (cliArgs[i] == "--port" && i + 1 < cliArgs.size()) port = cliArgs[i + 1].toInt();
        if (cliArgs[i] == "--no-browser") openBrowser = false;
      }
      // 有图形环境（DISPLAY/WAYLAND_DISPLAY 存在）时，启动后自动打开系统默认
      // 浏览器，并派生子进程在后台持续服务；无图形环境（无头服务器/沙箱）时
      // 无法弹浏览器，改为前台运行并打印访问地址，避免 QDesktopServices 崩溃。
      bool hasGui = !qEnvironmentVariableIsEmpty("DISPLAY")
                 || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
      if (hasGui && !qEnvironmentVariableIsSet("SERIAL_DEBUG_WEB_BG")
          && openBrowser) {
        QProcess bg;
        bg.setProgram(a.applicationFilePath());
        QStringList args;
        args << "--ui=web" << "--port" << QString::number(port);
        bg.setArguments(args);
        bg.setWorkingDirectory(a.applicationDirPath());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("SERIAL_DEBUG_WEB_BG", "1");
        bg.setProcessEnvironment(env);
        if (bg.startDetached()) {
          qInfo().noquote() << "Web server started in background (port " << port
                            << "). Parent process exiting.";
          return 0;
        }
        qWarning() << "Failed to detach background process; running web in foreground.";
      }
      if (!hasGui) openBrowser = false; // 无头环境：不自动开浏览器，避免崩溃
      return runWeb(a, port, openBrowser);
    }
#ifdef HAVE_QML
    case UiMode::QML:
      return runQml(a);
#endif
    case UiMode::QtWidgets:
    case UiMode::Auto:
    default: {
      a.setApplicationDisplayName("Serial Debug Assistant");
      a.setApplicationVersion(APP_VERSION);
      a.setWindowIcon(QIcon(":/logo"));

      LanguageManager::instance().initialize();

      QSettings settings;
      // 默认使用深色 Tokyo Night 主题（对齐 UI 原型），仅在用户显式选择
      // "跟随系统"时才回退到原生样式。
      QString savedTheme = settings.value("theme", "dark").toString();
      ThemeManager::instance().applyTheme(savedTheme);

      serial w;
      w.show();

      return a.exec();
    }
  }
}