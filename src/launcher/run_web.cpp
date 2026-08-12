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
#include "ui/SerialWorker.hpp"
#include "launcher/UiMode.hpp"

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
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
#include <cstdio>

namespace {

constexpr int kWebPort = 8080;
SerialWorker* g_web_worker = nullptr;
QTcpServer*  g_web_server = nullptr;
QSet<QTcpSocket*> g_web_clients;
QHash<QTcpSocket*, QByteArray> g_web_buf;

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
    s->write(wsFrame(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
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
    QByteArray html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Serial Debug · Web</title>"
        "<style>"
        "body{font-family:system-ui,sans-serif;background:#16161e;color:#c0caf5;margin:0;padding:24px}"
        ".card{background:#24283b;border-radius:12px;padding:16px;margin-bottom:16px}"
        "h1{font-size:20px;margin:0 0 12px} label{font-size:12px;opacity:.7}"
        "select,input{background:#1a1b26;color:#c0caf5;border:1px solid #3a3f5a;border-radius:6px;padding:6px;margin:4px 0}"
        "button{background:#7aa2f7;color:#1a1b26;border:none;border-radius:6px;padding:8px 14px;cursor:pointer;margin:4px 4px 0 0;font-weight:600}"
        "button.sec{background:#9ece6a} #log{background:#12121a;border-radius:8px;padding:10px;height:280px;overflow:auto;font-family:monospace;font-size:12px;white-space:pre-wrap}"
        ".row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}"
        "</style></head><body>"
        "<h1>Serial Debug Assistant · Browser UI</h1>"
        "<div class='card'><label>串口配置</label><div class='row'>"
        "<select id='port'></select><input id='baud' value='115200' style='width:90px'>"
        "<button id='open'>打开</button><button id='close' class='sec'>关闭</button>"
        "<span id='status' style='opacity:.8'></span></div></div>"
        "<div class='card'><label>发送</label><div class='row'>"
        "<input id='sendbox' style='flex:1' placeholder='输入内容，回车发送'><button id='send'>发送</button>"
        "<label><input type='checkbox' id='hex'> HEX</label></div></div>"
        "<div class='card'><label>接收</label><div id='log'></div></div>"
        "<script>"
        "var ws=new WebSocket('ws://'+location.host);var log=document.getElementById('log');"
        "function add(t){log.textContent+=t+'\\n';log.scrollTop=log.scrollHeight;}"
        "ws.onmessage=function(e){var m=JSON.parse(e.data);"
        "if(m.type==='rx')add('[RX] '+m.data);"
        "if(m.type==='status')document.getElementById('status').textContent=m.open?'已连接':'未连接';"
        "if(m.type==='ports'){var p=document.getElementById('port');p.innerHTML='';"
        "m.ports.forEach(function(x){var o=document.createElement('option');o.text=x;p.add(o);});}};"
        "ws.onopen=function(){ws.send(JSON.stringify({cmd:'list'}));};"
        "document.getElementById('open').onclick=function(){ws.send(JSON.stringify({cmd:'open',port:document.getElementById('port').value,baud:parseInt(document.getElementById('baud').value)}));};"
        "document.getElementById('close').onclick=function(){ws.send(JSON.stringify({cmd:'close'}));};"
        "document.getElementById('send').onclick=sendCmd;document.getElementById('sendbox').onkeydown=function(e){if(e.key==='Enter')sendCmd();};"
        "function sendCmd(){var v=document.getElementById('sendbox').value;if(!v)return;"
        "ws.send(JSON.stringify({cmd:'send',data:v,hex:document.getElementById('hex').checked}));"
        "add('[TX] '+v);document.getElementById('sendbox').value='';}"
        "</script></body></html>";
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
        QByteArray bytes;
        if (hex) bytes = SerialWorker::hexStringToByteArray(data);
        else { bytes = data.toLatin1(); bytes += "\r\n"; }
        qint64 n = g_web_worker->send(bytes);
        resp["ok"] = true;
        resp["bytes"] = static_cast<double>(n);
    } else {
        resp["ok"] = false;
        resp["err"] = "unknown command";
    }
    wsSend(s, resp);
}

} // namespace

// 运行 Web 服务事件循环。app 为 QCoreApplication（或 QApplication）。
int runWeb(QCoreApplication& app, int port) {
    SerialWorker worker;
    g_web_worker = &worker;

    QObject::connect(g_web_worker, &SerialWorker::dataReceived, [](const QByteArray &data) {
        QJsonObject o;
        o["type"] = "rx";
        o["data"] = QString::fromUtf8(data);
        for (QTcpSocket *s : g_web_clients) wsSend(s, o);
    });
    QObject::connect(g_web_worker, &SerialWorker::connectionChanged, [](bool open) {
        QJsonObject o;
        o["type"] = "status";
        o["open"] = open;
        for (QTcpSocket *s : g_web_clients) wsSend(s, o);
    });

    g_web_server = new QTcpServer;
    if (!g_web_server->listen(QHostAddress::Any, port)) {
        std::fprintf(stderr, "Failed to listen on port %d\n", port);
        return 1;
    }
    QObject::connect(g_web_server, &QTcpServer::newConnection, [&]() {
        while (QTcpSocket *s = g_web_server->nextPendingConnection()) {
            g_web_buf[s].clear();
            QObject::connect(s, &QTcpSocket::readyRead, [s]() {
                QByteArray &buf = g_web_buf[s];
                buf += s->readAll();

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
                        buf.clear();
                        return;
                    }
                }
                if (buf.contains("GET ")) {
                    serveIndex(s);
                    buf.clear();
                    return;
                }

                bool complete = false;
                QByteArray payload = wsTryParse(buf, &complete);
                if (complete && !payload.isEmpty()) {
                    QJsonDocument doc = QJsonDocument::fromJson(payload);
                    if (doc.isObject()) handleWebCommand(s, doc.object());
                }
            });
            QObject::connect(s, &QTcpSocket::disconnected, [s]() {
                g_web_clients.remove(s);
                g_web_buf.remove(s);
                s->deleteLater();
            });
        }
    });

    std::printf("Serial Debug Web UI listening on http://localhost:%d\n", port);
    std::fflush(stdout);
    return app.exec();
}