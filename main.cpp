#include "uart_interaction.h"
#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QIcon>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  // 设置应用程序信息
  a.setApplicationName("Serial Debug Assistant");
  a.setApplicationVersion("2.0");
  a.setWindowIcon(QIcon(":/logo"));

  // 加载翻译
  QTranslator translator;
  if (translator.load("./MySerial_zh_CN.qm")) {
    a.installTranslator(&translator);
  }

  serial w;
  w.show();

  return a.exec();
}
