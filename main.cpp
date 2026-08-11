#include "uart_interaction.h"
#include "thememanager.h"
#include "languagemanager.h"
#include <QApplication>
#include <QIcon>
#include <QSettings>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  // 设置应用程序信息
  a.setApplicationName("Serial Debug Assistant");
  a.setOrganizationName("SerialDebug");
  a.setApplicationVersion("2.0");
  a.setWindowIcon(QIcon(":/logo"));

  // 初始化语言管理器（从 QSettings 读取上次选择，或使用系统语言）
  LanguageManager::instance().initialize();

  // 应用保存的主题
  QSettings settings;
  QString savedTheme = settings.value("theme", "dark").toString();
  ThemeManager::instance().applyTheme(savedTheme);

  serial w;
  w.show();

  return a.exec();
}
