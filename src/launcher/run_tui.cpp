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
#include <cstdio>

#include "core/FieldSchema.hpp"
#include "core/PortConfig.hpp"
#include "core/Frame.hpp"
#include "service/FieldPool.hpp"
#include "service/ProtocolEngine.hpp"
#include "ui/SerialWorker.hpp"

namespace {

SerialWorker* g_tui_worker = nullptr;
bool g_tui_open = false;

// 协议解析状态：当前正在编辑的字段列表（未应用）、最近一次帧。
static std::vector<sd::FieldDesc> g_editFields;
static QString g_editName = "cust";
static bool g_editBigEndian = false;
static bool g_editDirty = false;
static qint64 g_tui_rxBytes = 0;
static qint64 g_tui_txBytes = 0;

const char* fieldTypeName(sd::FieldType t) {
    switch (t) {
        case sd::FieldType::U8:   return "uint8";
        case sd::FieldType::U16:  return "uint16";
        case sd::FieldType::U32:  return "uint32";
        case sd::FieldType::I8:   return "int8";
        case sd::FieldType::I16:  return "int16";
        case sd::FieldType::I32:  return "int32";
        case sd::FieldType::F32:  return "float32";
        case sd::FieldType::F64:  return "float64";
        case sd::FieldType::Bool: return "bool";
    }
    return "?";
}

sd::FieldType fieldTypeFromName(const QString& n) {
    const QString v = n.toLower();
    if (v == "u8")   return sd::FieldType::U8;
    if (v == "u16")  return sd::FieldType::U16;
    if (v == "u32")  return sd::FieldType::U32;
    if (v == "i8")   return sd::FieldType::I8;
    if (v == "i16")  return sd::FieldType::I16;
    if (v == "i32")  return sd::FieldType::I32;
    if (v == "f32" || v == "float" || v == "float32") return sd::FieldType::F32;
    if (v == "f64" || v == "float64") return sd::FieldType::F64;
    if (v == "bool") return sd::FieldType::Bool;
    return sd::FieldType::F32;
}

void tuiEcho(const QString &s); // 前向声明（applyCurrentSchema 使用）

void applyCurrentSchema() {
    if (g_editFields.empty()) {
        tuiEcho("No fields defined. Add fields first: field add <name> <type> [len]");
        return;
    }
    sd::ProtocolSchema schema;
    schema.name = g_editName.toStdString();
    schema.defaultBigEndian = g_editBigEndian;
    schema.fields = g_editFields;
    if (g_tui_worker->applyProtocolSchema(schema)) {
        g_editDirty = false;
        tuiEcho(QString("Protocol '%1' applied (%2 fields, %3 bytes/frame).")
                    .arg(g_editName)
                    .arg(g_editFields.size())
                    .arg(schema.totalBytes()));
    } else {
        tuiEcho("Failed to apply protocol schema.");
    }
}

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
            "Serial Debug TUI — 基础界面 + 终端交互 + 协议解析\n"
            "----------------------------------------\n"
            "基础界面：\n"
            "  list                              列出可用串口\n"
            "  open <port> [baud]                打开串口（默认 115200）\n"
            "  close                             关闭串口\n"
            "  send <text>                       发送文本（默认加 CR+LF）\n"
            "  send -n <text>                    发送文本（不加换行）\n"
            "  hex <AA BB ...>                   发送十六进制字节\n"
            "  stat                              显示 RX/TX 字节计数\n"
            "终端交互：\n"
            "  term <cmd>                        发送命令到交互式设备并回显\n"
            "协议解析：\n"
            "  proto [name]                      查看/设置协议名\n"
            "  field add <name> <type> [len]     添加字段（type: u8/u16/u32/i8/i16/i32/f32/f64/bool）\n"
            "  field addpad <name> <len>         添加 padding 占位字段\n"
            "  field list                        列出已定义字段\n"
            "  field clear                       清空字段\n"
            "  field rm <name>                   删除字段\n"
            "  endian <little|big>               设置字节序\n"
            "  schema                            应用协议并开始解析\n"
            "  pstat                             查看当前协议与最近一帧字段值\n"
            "设置：\n"
            "  set theme <system|dark|light>     切换主题\n"
            "  set lang <zh|en>                  切换语言\n"
            "  set serial databits <5|6|7|8>     设置数据位\n"
            "  set serial stopbits <1|1.5|2>     设置停止位\n"
            "  set serial parity <none|even|odd> 设置校验\n"
            "  set serial flow <none|rtscts|xonxoff> 设置流控\n"
            "  set serial show                   显示当前串口参数\n"
            "  set plugin list                   查看插件管理（规划中）\n"
            "----------------------------------------\n"
            "  ui switch <qt|tui|web|qml>        切换 UI（重启进程）\n"
            "  quit | exit                       退出"));
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
        if (parts.size() < 2) { tuiEcho("usage: send <text> | send -n <text>"); return; }
        bool noNewline = false;
        QString payload = raw.mid(5);
        if (parts[1] == "-n") { noNewline = true; payload = raw.mid(8); }
        QByteArray data = payload.toLatin1();
        if (!noNewline) data += "\r\n";
        qint64 n = g_tui_worker->send(data);
        g_tui_txBytes += n;
        tuiEcho(QString("TX: %1 bytes").arg(n));
    } else if (cmd == "hex") {
        if (!g_tui_open) { tuiEcho("Port not open."); return; }
        if (parts.size() < 2) { tuiEcho("usage: hex <AA BB ...>"); return; }
        QByteArray data = SerialWorker::hexStringToByteArray(raw.mid(4));
        qint64 n = g_tui_worker->send(data);
        g_tui_txBytes += n;
        tuiEcho(QString("TX hex: %1 bytes").arg(n));
    } else if (cmd == "stat") {
        tuiEcho(QString("RX: %1 bytes   TX: %2 bytes   %3")
                    .arg(g_tui_rxBytes).arg(g_tui_txBytes)
                    .arg(g_tui_open ? "connected" : "closed"));
    } else if (cmd == "term") {
        if (!g_tui_open) { tuiEcho("Port not open."); return; }
        QString payload = raw.mid(5).trimmed();
        if (payload.isEmpty()) { tuiEcho("usage: term <command>"); return; }
        QByteArray data = payload.toLatin1() + "\r\n";
        qint64 n = g_tui_worker->send(data);
        g_tui_txBytes += n;
        tuiEcho(QString("$ %1  (TX %2 B)").arg(payload).arg(n));
    } else if (cmd == "proto") {
        if (parts.size() >= 2) { g_editName = parts[1]; g_editDirty = true;
            tuiEcho(QString("Protocol name set to '%1' (run 'schema' to apply).").arg(g_editName)); return; }
        tuiEcho(QString("Protocol: %1 (editing: %2 fields, %3)")
                    .arg(g_tui_worker->protocolEngine().current().c_str())
                    .arg(g_editFields.size())
                    .arg(g_editDirty ? "dirty" : "clean"));
    } else if (cmd == "field" && parts.size() >= 2) {
        const QString sub = parts[1].toLower();
        if (sub == "add" && parts.size() >= 4) {
            sd::FieldDesc f;
            f.name = parts[2].toStdString();
            f.type = fieldTypeFromName(parts[3]);
            f.bigEndian = g_editBigEndian;
            if (parts.size() >= 5) f.byteLength = parts[4].toInt();
            g_editFields.push_back(f);
            g_editDirty = true;
            tuiEcho(QString("Added field '%1' (%2, %3 B).")
                        .arg(QString::fromStdString(f.name))
                        .arg(fieldTypeName(f.type))
                        .arg(f.byteLength > 0 ? f.byteLength : 0));
        } else if (sub == "addpad" && parts.size() >= 3) {
            sd::FieldDesc f;
            f.name = parts[2].toStdString();
            f.isPadding = true;
            f.byteLength = (parts.size() >= 4) ? parts[3].toInt() : 1;
            g_editFields.push_back(f);
            g_editDirty = true;
            tuiEcho(QString("Added padding '%1' (%2 B).").arg(parts[2]).arg(f.byteLength));
        } else if (sub == "list") {
            if (g_editFields.empty()) { tuiEcho("No fields defined."); return; }
            tuiEcho(QString("%1 | %2 | %3 | %4").arg("name", -12).arg("type", -8).arg("len", -4).arg("note"));
            for (const auto& f : g_editFields) {
                tuiEcho(QString("%1 | %2 | %3 | %4")
                            .arg(QString::fromStdString(f.name), -12)
                            .arg(fieldTypeName(f.type), -8)
                            .arg(f.byteLength > 0 ? f.byteLength : 0)
                            .arg(f.isPadding ? "padding" : ""));
            }
        } else if (sub == "clear") {
            g_editFields.clear(); g_editDirty = true;
            tuiEcho("All fields cleared.");
        } else if (sub == "rm" && parts.size() >= 3) {
            const QString n = parts[2];
            for (auto it = g_editFields.begin(); it != g_editFields.end(); ++it)
                if (QString::fromStdString(it->name) == n) { g_editFields.erase(it); g_editDirty = true; tuiEcho("Removed " + n); return; }
            tuiEcho("Field not found: " + n);
        } else {
            tuiEcho("usage: field add|addpad|list|clear|rm");
        }
    } else if (cmd == "endian") {
        if (parts.size() < 2) { tuiEcho("usage: endian <little|big>"); return; }
        g_editBigEndian = (parts[1].toLower() == "big");
        g_editDirty = true;
        tuiEcho(QString("Byte order set to %1.").arg(g_editBigEndian ? "big-endian" : "little-endian"));
    } else if (cmd == "schema") {
        applyCurrentSchema();
    } else if (cmd == "pstat") {
        const auto& pool = g_tui_worker->fieldPool();
        if (!pool.hasSchema()) { tuiEcho("No protocol applied. Define fields and run 'schema'."); return; }
        tuiEcho(QString("Protocol: %1").arg(pool.protocolName().c_str()));
        const auto& srcs = pool.sources();
        if (srcs.empty()) { tuiEcho("  (no data sources)"); return; }
        for (const auto& s : srcs) {
            QString v = s.hasData ? QString::number(s.lastValue, 'f', 3) : "--";
            tuiEcho(QString("  %1 = %2 %3").arg(s.name.c_str(), -12).arg(v, -10).arg(s.unit.c_str()));
        }
    } else if (cmd == "set") {
        if (parts.size() < 2) { tuiEcho("usage: set theme|lang|serial|plugin ..."); return; }
        const QString sub = parts[1].toLower();
        if (sub == "theme") {
            if (parts.size() < 3) { tuiEcho("usage: set theme <system|dark|light>"); return; }
            const QString v = parts[2].toLower();
            QString name;
            if      (v == "system") name = "跟随系统";
            else if (v == "dark")   name = "深色 Tokyo Night";
            else if (v == "light")  name = "浅色";
            else { tuiEcho("Invalid theme: " + parts[2] + " (expect system|dark|light)"); return; }
            tuiEcho(QString("主题 → %1").arg(name));
        } else if (sub == "lang") {
            if (parts.size() < 3) { tuiEcho("usage: set lang <zh|en>"); return; }
            const QString v = parts[2].toLower();
            if (v == "zh") tuiEcho("语言 → 中文");
            else if (v == "en") tuiEcho("语言 → English");
            else { tuiEcho("Invalid lang: " + parts[2] + " (expect zh|en)"); return; }
        } else if (sub == "serial") {
            if (parts.size() < 3) { tuiEcho("usage: set serial databits|stopbits|parity|flow|show"); return; }
            sd::PortConfig& cfg = g_tui_worker->config();
            const QString attr = parts[2].toLower();
            if (attr == "show") {
                tuiEcho(QString("串口参数: 数据位 %1 | 停止位 %2 | 校验 %3 | 流控 %4")
                            .arg(cfg.dataBits)
                            .arg(cfg.stopBits)
                            .arg(cfg.parity == 0 ? "None" : cfg.parity == 1 ? "Even" : cfg.parity == 2 ? "Odd" : "?")
                            .arg(cfg.flowControl == 0 ? "None" : cfg.flowControl == 1 ? "RTS-CTS" : cfg.flowControl == 2 ? "XON-XOFF" : "?"));
                return;
            } else if (attr == "databits") {
                if (parts.size() < 4) { tuiEcho("usage: set serial databits <5|6|7|8>"); return; }
                const int v = parts[3].toInt();
                if (v < 5 || v > 8) { tuiEcho("Invalid databits: " + parts[3] + " (expect 5|6|7|8)"); return; }
                cfg.dataBits = v;
                tuiEcho(QString("数据位 → %1").arg(v));
            } else if (attr == "stopbits") {
                if (parts.size() < 4) { tuiEcho("usage: set serial stopbits <1|1.5|2>"); return; }
                const QString v = parts[3];
                if (v == "1" || v == "1.5" || v == "2") {
                    cfg.stopBits = (v == "2") ? 2 : 1; // PortConfig.stopBits 为 int，1.5 存为 1
                    tuiEcho(QString("停止位 → %1").arg(v));
                } else { tuiEcho("Invalid stopbits: " + v + " (expect 1|1.5|2)"); }
            } else if (attr == "parity") {
                if (parts.size() < 4) { tuiEcho("usage: set serial parity <none|even|odd>"); return; }
                const QString v = parts[3].toLower();
                if      (v == "none") { cfg.parity = 0; tuiEcho("校验 → None"); }
                else if (v == "even") { cfg.parity = 1; tuiEcho("校验 → Even"); }
                else if (v == "odd")  { cfg.parity = 2; tuiEcho("校验 → Odd"); }
                else { tuiEcho("Invalid parity: " + parts[3] + " (expect none|even|odd)"); }
            } else if (attr == "flow") {
                if (parts.size() < 4) { tuiEcho("usage: set serial flow <none|rtscts|xonxoff>"); return; }
                const QString v = parts[3].toLower();
                if      (v == "none")    { cfg.flowControl = 0; tuiEcho("流控 → None"); }
                else if (v == "rtscts")  { cfg.flowControl = 1; tuiEcho("流控 → RTS-CTS"); }
                else if (v == "xonxoff") { cfg.flowControl = 2; tuiEcho("流控 → XON-XOFF"); }
                else { tuiEcho("Invalid flow: " + parts[3] + " (expect none|rtscts|xonxoff)"); }
            } else {
                tuiEcho("usage: set serial databits|stopbits|parity|flow|show");
            }
        } else if (sub == "plugin") {
            if (parts.size() < 3 || parts[2].toLower() != "list") {
                tuiEcho("usage: set plugin list");
                return;
            }
            tuiEcho("插件管理 · 规划中");
        } else {
            tuiEcho("usage: set theme|lang|serial|plugin ...");
        }
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
        g_tui_rxBytes += data.size();
        QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        QTextStream out(stdout);
        out << "[" << ts << "] RX: " << QString::fromLatin1(data) << "\n";
        out.flush();
    });

    // 协议解析帧：显示字段名/值（对齐原型"协议解析"界面）。
    QObject::connect(g_tui_worker, &SerialWorker::frameReceived,
                     [](const sd::Frame &frame) {
        QTextStream out(stdout);
        out << "── frame #" << frame.seq << " [" << QString::fromStdString(frame.name) << "]\n";
        for (const auto& kv : frame.numeric)
            out << "    " << QString::fromStdString(kv.first) << " = "
                << QString::number(kv.second, 'g', 6) << "\n";
        for (const auto& kv : frame.text)
            out << "    " << QString::fromStdString(kv.first) << " = "
                << QString::fromStdString(kv.second) << "\n";
        out.flush();
    });

    tuiEcho("Serial Debug TUI — 基础界面 + 终端交互 + 协议解析 (type 'help').");
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