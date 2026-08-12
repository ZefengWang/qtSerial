// ============================================================
// run_web.cpp —— 浏览器界面后端（WebSocket + 静态 HTML 服务）
//
// 被统一入口 main 调用（--ui=web）。
// 复用同一套 core/service 与 SerialWorker。
// 用 Qt Network 手写一个极简 HTTP + WebSocket(RFC6455) 服务端，
// 避免依赖可能缺失的 QtWebSockets 模块。
//
// 启动：  serial-debug --ui=web [--port 8080]
// 访问：  http://localhost:8080
//
// 前端通过 WebSocket 与后端通信：
//   {cmd:"list"}                       -> {ok, ports:[]}
//   {cmd:"open", port, baud}           -> {ok, err?}
//   {cmd:"close"}                      -> {ok}
//   {cmd:"send", data}                 -> {ok, bytes}
// 后端推送： {"type":"rx","data":"..."}  {"type":"status","open":bool}
// ============================================================
#include "core/PortConfig.hpp"
#include "core/FieldSchema.hpp"
#include "core/Frame.hpp"
#include "ui/SerialWorker.hpp"
#include "launcher/UiMode.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QCryptographicHash>
#include <QHash>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <cstdio>

namespace {

// 自动打开系统默认浏览器（延迟到事件循环起来后执行）。
// 无图形环境（无头服务器/沙箱）下不打开，避免 QDesktopServices 崩溃。
void openBrowserLater(int port) {
    bool hasGui = !qEnvironmentVariableIsEmpty("DISPLAY")
               || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    if (!hasGui) return;
    QTimer::singleShot(0, [port]() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://localhost:%1").arg(port)));
    });
}

constexpr int kWebPort = 8080;
SerialWorker* g_web_worker = nullptr;
QTcpServer*  g_web_server = nullptr;
QSet<QTcpSocket*> g_web_clients;
QHash<QTcpSocket*, QByteArray> g_web_buf;

// ============================================================
// 会话管理：统计浏览器页面连接数；无会话时自动退出进程防泄漏。
// ============================================================
void wsSend(QTcpSocket *s, const QJsonObject &obj); // 前向声明（定义在下方）
QTimer* g_web_exitTimer = nullptr;   // 无会话自动退出倒计时
bool    g_web_autoExit   = true;     // 是否启用自动退出
int     g_web_exitDelaySec = 5;      // 无会话后的退出延迟（秒）

// 广播当前会话数到所有已连接的浏览器页面。
void broadcastSessions() {
    QJsonObject o;
    o["type"] = "session";
    o["count"] = g_web_clients.size();
    for (QTcpSocket *s : g_web_clients) wsSend(s, o);
}

// 无会话后调度自动退出；延迟期间若新会话接入则取消。
void scheduleAutoExit() {
    if (!g_web_autoExit) return;
    if (g_web_exitTimer) return; // 已在倒计时
    g_web_exitTimer = new QTimer;
    g_web_exitTimer->setSingleShot(true);
    QObject::connect(g_web_exitTimer, &QTimer::timeout, []() {
        g_web_exitTimer = nullptr;
        if (g_web_clients.isEmpty()) {
            std::fprintf(stderr, "[session] 无浏览器会话连接，自动退出进程（防止资源泄漏）.\n");
            std::fflush(stderr);
            if (QCoreApplication *ap = QCoreApplication::instance())
                ap->quit();
        }
    });
    g_web_exitTimer->start(g_web_exitDelaySec * 1000);
}

void cancelAutoExit() {
    if (g_web_exitTimer) {
        g_web_exitTimer->stop();
        g_web_exitTimer->deleteLater();
        g_web_exitTimer = nullptr;
    }
}

// 协议编辑状态（前端编辑、apply 前暂存）。
std::vector<sd::FieldDesc> g_web_fields;
QString g_web_endian = "little";
QString g_web_protoName = "cust";

// 设置页状态（软件设置 / 高级串口设置）
QString g_web_theme = "dark";
QString g_web_lang  = "zh";
bool    g_web_autoBrowse = true;
int     g_web_timerMs   = 1000;
int     g_web_dataBits  = 8;
int     g_web_stopBits  = 1;
QString g_web_parity = "none";
QString g_web_flow   = "none";

sd::FieldType webFieldTypeFromString(const QString &s) {
    const QString v = s.toLower();
    if (v == "u8") return sd::FieldType::U8;
    if (v == "u16") return sd::FieldType::U16;
    if (v == "u32") return sd::FieldType::U32;
    if (v == "i8") return sd::FieldType::I8;
    if (v == "i16") return sd::FieldType::I16;
    if (v == "i32") return sd::FieldType::I32;
    if (v == "float") return sd::FieldType::F32;
    if (v == "double") return sd::FieldType::F64;
    if (v == "bool") return sd::FieldType::Bool;
    return sd::FieldType::F32;
}
QString webFieldTypeToString(sd::FieldType t) {
    switch (t) {
        case sd::FieldType::U8: return "u8";
        case sd::FieldType::U16: return "u16";
        case sd::FieldType::U32: return "u32";
        case sd::FieldType::I8: return "i8";
        case sd::FieldType::I16: return "i16";
        case sd::FieldType::I32: return "i32";
        case sd::FieldType::F32: return "float";
        case sd::FieldType::F64: return "double";
        case sd::FieldType::Bool: return "bool";
    }
    return "float";
}
std::vector<sd::FieldDesc> jsonToFields(const QJsonArray &arr) {
    std::vector<sd::FieldDesc> out;
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        sd::FieldDesc f;
        f.name = o.value("name").toString().toStdString();
        f.isPadding = o.value("pad").toBool(false);
        if (f.isPadding) {
            f.byteLength = o.value("len").toInt(1);
        } else {
            f.type = webFieldTypeFromString(o.value("type").toString());
            f.byteLength = o.value("len").toInt();
            if (f.byteLength <= 0) f.byteLength = sd::fieldTypeBytes(f.type);
        }
        out.push_back(f);
    }
    return out;
}
QJsonArray fieldsToJson(const std::vector<sd::FieldDesc> &fields) {
    QJsonArray arr;
    for (const auto &f : fields) {
        QJsonObject o;
        o["name"] = QString::fromStdString(f.name);
        o["pad"] = f.isPadding;
        if (f.isPadding) o["len"] = f.byteLength;
        else { o["type"] = webFieldTypeToString(f.type); o["len"] = f.byteLength; }
        arr.append(o);
    }
    return arr;
}

QByteArray wsFrame(const QByteArray &payload) {
    QByteArray frame;
    frame.append(char(0x81));
    int len = payload.size();
    if (len < 126) {
        frame.append(char(len));
    } else if (len < 65536) {
        frame.append(char(126));
        frame.append(char((len >> 8) & 0xFF));
        frame.append(char(len & 0xFF));
    } else {
        frame.append(char(127));
        for (int i = 7; i >= 0; --i) frame.append(char((static_cast<qint64>(len) >> (i * 8)) & 0xFF));
    }
    frame.append(payload);
    return frame;
}

void wsSend(QTcpSocket *s, const QJsonObject &obj) {
    if (!s || s->state() != QAbstractSocket::ConnectedState) return;
    s->write(wsFrame(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    s->flush();
}

QByteArray wsTryParse(QByteArray &buf, bool *complete) {
    *complete = false;
    if (buf.size() < 2) return {};
    unsigned char b0 = buf[0];
    unsigned char b1 = buf[1];
    if ((b0 & 0x0F) != 1) {
        buf.remove(0, 1);
        return {};
    }
    qint64 len = b1 & 0x7F;
    int idx = 2;
    if (len == 126) {
        if (buf.size() < 4) return {};
        len = (static_cast<unsigned char>(buf[2]) << 8) | static_cast<unsigned char>(buf[3]);
        idx = 4;
    } else if (len == 127) {
        if (buf.size() < 10) return {};
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | static_cast<unsigned char>(buf[2 + i]);
        idx = 10;
    }
    bool masked = (b1 & 0x80) != 0;
    if (masked) {
        if (buf.size() < idx + 4) return {};
        QByteArray mask = buf.mid(idx, 4);
        idx += 4;
        if (buf.size() < idx + len) return {};
        QByteArray payload = buf.mid(idx, len);
        for (int i = 0; i < len; ++i) payload[i] = payload[i] ^ mask[i % 4];
        buf.remove(0, idx + len);
        *complete = true;
        return payload;
    } else {
        if (buf.size() < idx + len) return {};
        QByteArray payload = buf.mid(idx, len);
        buf.remove(0, idx + len);
        *complete = true;
        return payload;
    }
}

void serveIndex(QTcpSocket *s) {
    // 读取内嵌资源（严格对齐原型的四页前端，见 src/launcher/web/index.html）。
    QFile webFile(QStringLiteral(":/web/index.html"));
    QByteArray html;
    if (webFile.open(QIODevice::ReadOnly))
        html = webFile.readAll();
    else
        html = "<html><body><h3>web/index.html resource missing</h3></body></html>";
    QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                      "Content-Length: " + QByteArray::number(html.size()) +
                      "\r\nConnection: close\r\n\r\n" + html;
    s->write(resp);
    s->flush();
    s->disconnectFromHost();
}

void handleWebCommand(QTcpSocket *s, const QJsonObject &obj) {
    QString cmd = obj.value("cmd").toString();
    QJsonObject resp;
    if (cmd == "list") {
        QStringList ports = g_web_worker->scanPorts();
        resp["type"] = "ports";
        resp["ports"] = QJsonArray::fromStringList(ports);
    } else if (cmd == "open") {
        sd::PortConfig cfg = g_web_worker->config();
        cfg.name = obj.value("port").toString().toStdString();
        cfg.baudRate = obj.value("baud").toInt(115200);
        bool ok = g_web_worker->open(cfg);
        resp["ok"] = ok;
        if (!ok) resp["err"] = g_web_worker->lastError();
    } else if (cmd == "close") {
        g_web_worker->close();
        resp["ok"] = true;
    } else if (cmd == "send") {
        QString data = obj.value("data").toString();
        bool hex = obj.value("hex").toBool(false);
        bool newline = obj.value("newline").toBool(true);
        QByteArray bytes;
        if (hex) bytes = SerialWorker::hexStringToByteArray(data);
        else { bytes = data.toLatin1(); if (newline) bytes += "\r\n"; }
        qint64 n = g_web_worker->send(bytes);
        resp["ok"] = true;
        resp["bytes"] = static_cast<double>(n);
    } else if (cmd == "setproto") {
        // 前端编辑字段定义：保存到后端状态备用（不立即应用）。
        g_web_fields = jsonToFields(obj.value("fields").toArray());
        g_web_endian = obj.value("endian").toString("little");
        resp["ok"] = true;
    } else if (cmd == "getproto") {
        resp["type"] = "proto";
        resp["fields"] = fieldsToJson(g_web_fields);
        resp["endian"] = g_web_endian;
        resp["name"] = g_web_protoName;
    } else if (cmd == "getsettings") {
        resp["type"] = "settings";
        resp["theme"] = g_web_theme;
        resp["lang"] = g_web_lang;
        resp["autobrowse"] = g_web_autoBrowse;
        resp["timerMs"] = g_web_timerMs;
        resp["dataBits"] = g_web_dataBits;
        resp["stopBits"] = g_web_stopBits;
        resp["parity"] = g_web_parity;
        resp["flow"] = g_web_flow;
        resp["autoExit"] = g_web_autoExit;
        resp["exitDelay"] = g_web_exitDelaySec;
    } else if (cmd == "setsettings") {
        if (obj.contains("theme"))  g_web_theme  = obj.value("theme").toString();
        if (obj.contains("lang"))   g_web_lang   = obj.value("lang").toString();
        if (obj.contains("autobrowse")) g_web_autoBrowse = obj.value("autobrowse").toBool();
        if (obj.contains("timerMs")) g_web_timerMs = obj.value("timerMs").toInt(1000);
        if (obj.contains("dataBits")) g_web_dataBits = obj.value("dataBits").toInt(8);
        if (obj.contains("stopBits")) g_web_stopBits = obj.value("stopBits").toInt(1);
        if (obj.contains("parity"))  g_web_parity = obj.value("parity").toString();
        if (obj.contains("flow"))    g_web_flow   = obj.value("flow").toString();
        if (obj.contains("autoExit")) {
            g_web_autoExit = obj.value("autoExit").toBool();
            // 关闭自动退出时，取消已调度的退出倒计时。
            if (!g_web_autoExit) cancelAutoExit();
        }
        if (obj.contains("exitDelay")) g_web_exitDelaySec = qMax(1, obj.value("exitDelay").toInt(5));
        resp["ok"] = true;
    } else if (cmd == "applyproto") {
        sd::ProtocolSchema schema;
        schema.name = obj.value("name").toString("cust").toStdString();
        schema.defaultBigEndian = (obj.value("endian").toString("little") == "big");
        schema.fields = g_web_fields;
        bool ok = g_web_worker->applyProtocolSchema(schema);
        resp["ok"] = ok;
        if (ok) {
            g_web_protoName = QString::fromStdString(schema.name);
            resp["type"] = "schema";
            resp["name"] = g_web_protoName;
            resp["endian"] = schema.defaultBigEndian ? "big" : "little";
        } else {
            resp["err"] = "invalid schema (need at least one data field)";
        }
    } else {
        resp["ok"] = false;
        resp["err"] = "unknown command";
    }
    wsSend(s, resp);
}

} // namespace

// 运行 Web 服务事件循环。app 为 QCoreApplication（或 QApplication）。
// openBrowser=true 时启动后自动用系统默认浏览器打开页面。
int runWeb(QCoreApplication& app, int port, bool openBrowser) {
    SerialWorker worker;
    g_web_worker = &worker;

    QObject::connect(g_web_worker, &SerialWorker::dataReceived, [](const QByteArray &data) {
        QJsonObject o;
        o["type"] = "rx";
        o["data"] = QString::fromUtf8(data);
        QJsonArray bytes;
        for (int i = 0; i < data.size(); ++i) bytes.append(data[i] & 0xFF);
        o["bytes"] = bytes;
        for (QTcpSocket *s : g_web_clients) wsSend(s, o);
    });
    QObject::connect(g_web_worker, &SerialWorker::frameReceived, [](const sd::Frame &frame) {
        QJsonObject o;
        o["type"] = "frame";
        o["seq"] = static_cast<double>(frame.seq);
        o["name"] = QString::fromStdString(frame.name);
        QJsonObject num;
        for (const auto &kv : frame.numeric) num[QString::fromStdString(kv.first)] = kv.second;
        o["numeric"] = num;
        QJsonObject txt;
        for (const auto &kv : frame.text) txt[QString::fromStdString(kv.first)] = QString::fromStdString(kv.second);
        o["text"] = txt;
        for (QTcpSocket *s : g_web_clients) wsSend(s, o);
    });
    QObject::connect(g_web_worker, &SerialWorker::connectionChanged, [](bool open) {
        QJsonObject o;
        o["type"] = "status";
        o["open"] = open;
        for (QTcpSocket *s : g_web_clients) wsSend(s, o);
    });

    g_web_server = new QTcpServer;
    int actualPort = port;
    if (!g_web_server->listen(QHostAddress::Any, port)) {
        // 给定端口被占用：尝试从 8080 往后找空闲端口，最多试 10 个
        for (int i = 1; i <= 10; ++i) {
            actualPort = 8080 + i;
            if (g_web_server->listen(QHostAddress::Any, actualPort)) {
                std::fprintf(stderr, "Warning: port %d occupied, found free port %d\n", port, actualPort);
                break;
            }
        }
        if (!g_web_server->isListening()) {
            std::fprintf(stderr, "Failed to listen (tried %d..%d), all ports occupied\n", 8080, 8080 + 10);
            return 1;
        }
    }
    QObject::connect(g_web_server, &QTcpServer::newConnection, [&]() {
        while (QTcpSocket *s = g_web_server->nextPendingConnection()) {
            // 用隐式共享的 QByteArray 按值存取，避免 QHash 扩容导致引用失效
            g_web_buf[s] = QByteArray();
            QObject::connect(s, &QTcpSocket::readyRead, [s]() {
                QByteArray chunk = s->readAll();
                if (chunk.isEmpty()) return;
                // 按值读取 + 追加 + 写回，绝不跨语句持有 QHash 引用。
                // 并发连接触发 QHash 重新哈希时，引用会失效导致堆损坏。
                QByteArray buf = g_web_buf.value(s);
                buf += chunk;

                if (buf.contains("Upgrade: websocket")) {
                    int ki = buf.indexOf("Sec-WebSocket-Key:");
                    if (ki >= 0) {
                        int nl = buf.indexOf("\r\n", ki);
                        QByteArray key = buf.mid(ki + 19, nl - ki - 19).trimmed();
                        QByteArray accept = QCryptographicHash::hash(
                            key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                            QCryptographicHash::Sha1).toBase64();
                        QByteArray resp = "HTTP/1.1 101 Switching Protocols\r\n"
                                          "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                                          "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
                        s->write(resp);
                        s->flush();
                        g_web_clients.insert(s);
                        g_web_buf.remove(s);
                        // 有新会话接入：取消自动退出，并广播会话数。
                        cancelAutoExit();
                        broadcastSessions();
                        return;
                    }
                }
                if (buf.contains("GET ")) {
                    serveIndex(s);
                    g_web_buf.remove(s);
                    return;
                }

                bool complete = false;
                QByteArray payload = wsTryParse(buf, &complete);
                if (complete && !payload.isEmpty()) {
                    QJsonDocument doc = QJsonDocument::fromJson(payload);
                    if (doc.isObject()) handleWebCommand(s, doc.object());
                    // 已完整消费，清空缓冲
                    g_web_buf.remove(s);
                } else if (!complete) {
                    // 半包：保留缓冲，等待后续数据
                    g_web_buf[s] = buf;
                } else {
                    g_web_buf.remove(s);
                }
            });
            QObject::connect(s, &QTcpSocket::disconnected, [s]() {
                g_web_clients.remove(s);
                g_web_buf.remove(s);
                s->deleteLater();
                // 会话数变化：广播，并在无会话时调度自动退出。
                broadcastSessions();
                scheduleAutoExit();
            });
            QObject::connect(s, &QAbstractSocket::errorOccurred, [s]() {
                // 出错时清理，避免悬挂引用
                g_web_clients.remove(s);
                g_web_buf.remove(s);
                broadcastSessions();
                scheduleAutoExit();
            });
        }
    });

    // 明确提示监听端口（写 stderr，确保在图形环境 QApplication 下也能看到）。
    std::fprintf(stderr,
        "\n====================================================================\n"
        "  Serial Debug Assistant · Web 界面已启动\n"
        "  本机访问:  http://localhost:%d\n"
        "  局域网访问: http://<本机IP>:%d   （同一网络内其它设备可打开）\n"
        "  后台运行中，Ctrl+C 停止。\n"
        "====================================================================\n\n",
        actualPort, actualPort);
    std::fflush(stderr);

    if (openBrowser)
        openBrowserLater(actualPort);

    return app.exec();
}