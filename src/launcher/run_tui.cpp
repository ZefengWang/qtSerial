// ============================================================
// run_tui.cpp —— 终端无头界面（TUI）运行逻辑
//
// 被统一入口 main 调用（--ui=tui 或自动检测到无图形环境）。
// 复用同一套 core/service 与 SerialWorker。
// 提供纯文本交互：
//   help              帮助
//   list              列出可用串口
//   open <port> [baud] 打开串口（默认 115200）
//   close             关闭串口
//   send <text>       发送文本（附加 \r\n）
//   hex <AA BB ...>   发送十六进制字节
//   ui switch <mode>  切换到其它界面宿主（重启进程）
//   quit / exit       退出
//
// 数据回显：接收到的数据实时打印到 stdout（带时间戳前缀）。
// ============================================================
#include "core/PortConfig.hpp"
#include "ui/SerialWorker.hpp"
#include "launcher/UiMode.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QDateTime>
#include <QByteArray>
#include <iostream>

namespace {

SerialWorker* g_tui_worker = nullptr;
bool g_tui_open = false;

void tuiEcho(const QString &s) {
    QTextStream out(stdout);
    out << s << "\n";
    out.flush();
}

void handleTuiLine(const QString &raw) {
    QString line = raw.trimmed();
    if (line.isEmpty()) return;
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QString cmd = parts[0].toLower();

    if (cmd == "help") {
        tuiEcho(QString(
            "Commands:\n"
            "  help                     this help\n"
            "  list                     list available serial ports\n"
            "  open <port> [baud]       open port (default baud 115200)\n"
            "  close                    close current port\n"
            "  send <text>              send text (append CR+LF)\n"
            "  hex <AA BB ...>          send hex bytes\n"
            "  ui switch <qt|web|qml>   switch UI host (restart app)\n"
            "  quit | exit              quit"));
    } else if (cmd == "list") {
        QStringList ports = g_tui_worker->scanPorts();
        if (ports.isEmpty()) {
            tuiEcho("No serial ports found.");
        } else {
            tuiEcho("Available ports:");
            for (const QString &p : ports) tuiEcho("  " + p);
        }
    } else if (cmd == "open") {
        if (parts.size() < 2) { tuiEcho("usage: open <port> [baud]"); return; }
        sd::PortConfig cfg = g_tui_worker->config();
        cfg.name = parts[1].toStdString();
        cfg.baudRate = (parts.size() >= 3) ? parts[2].toInt() : 115200;
        if (g_tui_worker->open(cfg)) {
            g_tui_open = true;
            tuiEcho(QString("Opened %1 @ %2").arg(cfg.name.c_str()).arg(cfg.baudRate));
        } else {
            tuiEcho(QString("Failed to open: %1").arg(g_tui_worker->lastError()));
        }
    } else if (cmd == "close") {
        g_tui_worker->close();
        g_tui_open = false;
        tuiEcho("Closed.");
    } else if (cmd == "send") {
        if (!g_tui_open) { tuiEcho("Port not open."); return; }
        if (parts.size() < 2) { tuiEcho("usage: send <text>"); return; }
        QByteArray data = raw.mid(5).toLatin1() + "\r\n";
        g_tui_worker->send(data);
        tuiEcho(QString("TX: %1 bytes").arg(data.size()));
    } else if (cmd == "hex") {
        if (!g_tui_open) { tuiEcho("Port not open."); return; }
        if (parts.size() < 2) { tuiEcho("usage: hex <AA BB ...>"); return; }
        QByteArray data = SerialWorker::hexStringToByteArray(raw.mid(4));
        g_tui_worker->send(data);
        tuiEcho(QString("TX hex: %1 bytes").arg(data.size()));
    } else if (cmd == "ui" && parts.size() >= 2 && parts[1] == "switch") {
        if (parts.size() < 3) { tuiEcho("usage: ui switch <qt|web|qml>"); return; }
        ui_mode::restartWithMode(ui_mode::fromString(parts[2]));
    } else if (cmd == "quit" || cmd == "exit") {
        QCoreApplication::quit();
    } else {
        tuiEcho("Unknown command: " + cmd + " (type 'help')");
    }
}

} // namespace

// 运行 TUI 事件循环。app 必须为 QCoreApplication（或 QApplication）。
int runTui(QCoreApplication& app) {
    SerialWorker worker;
    g_tui_worker = &worker;

    QObject::connect(g_tui_worker, &SerialWorker::dataReceived,
                     [](const QByteArray &data) {
        QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        QTextStream out(stdout);
        out << "[" << ts << "] RX: " << QString::fromLatin1(data) << "\n";
        out.flush();
    });

    tuiEcho("Serial Debug TUI (same core as desktop). Type 'help'.");
    tuiEcho("Ports: " + g_tui_worker->scanPorts().join(", "));

    QFile inFile;
    inFile.open(stdin, QIODevice::ReadOnly);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, [&inFile]() {
        while (inFile.canReadLine()) {
            QByteArray line = inFile.readLine();
            handleTuiLine(QString::fromUtf8(line));
        }
    });
    poll.start(50);

    return app.exec();
}