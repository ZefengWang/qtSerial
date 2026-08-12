#include "uart_interaction.h"
#include "thememanager.h"
#include "languagemanager.h"
#include <QApplication>
#include <QIcon>
#include <QSettings>

// Version injected at build time from the git tag (APP_VERSION macro).
// Fallback keeps local builds working when the macro is absent.
#ifndef APP_VERSION
#define APP_VERSION "2.1.0"
#endif

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  // 设置应用程序信息
  // applicationName 同时决定 WM_CLASS，需与 .desktop 的 StartupWMClass 一致
  a.setApplicationName("serial-debug");
  a.setApplicationDisplayName("Serial Debug Assistant");
  a.setOrganizationName("SerialDebug");
  a.setApplicationVersion(APP_VERSION);
  a.setWindowIcon(QIcon(":/logo"));

  // 初始化语言管理器（从 QSettings 读取上次选择，或使用系统语言）
  LanguageManager::instance().initialize();

  // 应用保存的主题；默认使用 "system" —— 跟随系统原生风格（GNOME/Adwaita），
  // 而不是强绑某一种配色。暗色/亮色仅作为用户主动选择的选项。
  QSettings settings;
  QString savedTheme = settings.value("theme", "system").toString();
  ThemeManager::instance().applyTheme(savedTheme);

  serial w;
  w.show();

  return a.exec();
}
