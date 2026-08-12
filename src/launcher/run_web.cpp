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
void openBrowserLater(int port) {
    QTimer::singleShot(0, [port]() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://localhost:%1").arg(port)));
    });
}

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
        "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Serial Debug Assistant · Web</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:system-ui,-apple-system,'PingFang SC',sans-serif;background:#16161e;color:#c0caf5;height:100vh;display:flex;overflow:hidden}"
        ".side{width:170px;background:#1a1b26;padding:14px 10px;display:flex;flex-direction:column;gap:4px;flex-shrink:0}"
        ".brand{font-size:14px;font-weight:700;color:#7aa2f7;padding:6px 10px 14px;border-bottom:1px solid #2a2c3a;margin-bottom:10px}"
        ".nav{display:flex;align-items:center;gap:8px;padding:9px 12px;border-radius:6px;cursor:pointer;font-size:13px;color:#9aa5ce;transition:all .15s}"
        ".nav:hover{background:#24283b;color:#c0caf5}"
        ".nav.active{background:#7aa2f7;color:#1a1b26;font-weight:600}"
        ".main{flex:1;display:flex;flex-direction:column;overflow:hidden}"
        ".topbar{height:50px;background:#24283b;display:flex;align-items:center;gap:12px;padding:0 18px;border-bottom:1px solid #2a2c3a;flex-shrink:0}"
        ".topbar .st{font-size:12px;color:#9ece6a}.topbar .st.off{color:#565f89}"
        ".content{flex:1;overflow-y:auto;padding:18px}"
        ".card{background:#24283b;border-radius:10px;padding:16px;margin-bottom:14px}"
        ".card h3{font-size:13px;color:#7aa2f7;margin-bottom:12px;font-weight:600}"
        "label{font-size:12px;opacity:.7;display:block;margin-bottom:4px}"
        "select,input{background:#1a1b26;color:#c0caf5;border:1px solid #3a3f5a;border-radius:6px;padding:8px;font-size:13px;width:100%}"
        ".row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}"
        ".row>select{flex:1;min-width:140px}.row>input{flex:1;min-width:90px}"
        "button{background:#7aa2f7;color:#1a1b26;border:none;border-radius:6px;padding:9px 16px;cursor:pointer;font-weight:600;font-size:13px}"
        "button.sec{background:#9ece6a}button.warn{background:#e0af68}button.danger{background:#f7768e}"
        "button:disabled{opacity:.4;cursor:not-allowed}"
        "#log{background:#12121a;border-radius:8px;padding:10px;height:300px;overflow:auto;font-family:ui-monospace,'JetBrains Mono',monospace;font-size:12px;white-space:pre-wrap;line-height:1.6}"
        "#term{background:#12121a;border-radius:8px;padding:10px;height:300px;overflow:auto;font-family:ui-monospace,monospace;font-size:12px;white-space:pre-wrap;line-height:1.6;color:#9ece6a}"
        "#termbox{background:#1a1b26;border:1px solid #3a3f5a;border-radius:6px;padding:9px;color:#c0caf5;font-family:monospace;font-size:13px;flex:1}"
        ".page{display:none}.page.active{display:block}"
        "#viz{background:#12121a;border-radius:8px;padding:10px;height:300px;overflow:auto;font-family:monospace;font-size:11px;white-space:pre;line-height:1.3;color:#7aa2f7}"
        ".proto table{width:100%;border-collapse:collapse;font-size:12px}"
        ".proto td{border-bottom:1px solid #2a2c3a;padding:7px 8px;color:#c0caf5}"
        ".proto th{text-align:left;color:#7aa2f7;padding:7px 8px;font-size:12px;border-bottom:1px solid #3a3f5a}"
        ".pill{display:inline-block;background:#1a1b26;border:1px solid #3a3f5a;border-radius:12px;padding:3px 12px;font-size:12px;margin:0 6px 6px 0;cursor:pointer}"
        ".pill.on{background:#7aa2f7;color:#1a1b26;border-color:#7aa2f7}"
        ".hint{font-size:11px;color:#565f89;margin-top:8px}"
        "</style></head><body>"
        "<div class='side'>"
        "<div class='brand'>Serial Debug</div>"
        "<div class='nav active' data-p='basic'>▣ &nbsp;基础</div>"
        "<div class='nav' data-p='term'>⌨ &nbsp;终端交互</div>"
        "<div class='nav' data-p='proto'>≣ &nbsp;协议解析</div>"
        "<div class='nav' data-p='viz'>∿ &nbsp;可视化</div>"
        "</div>"
        "<div class='main'>"
        "<div class='topbar'><span style='font-weight:600;font-size:14px'>串口调试助手</span>"
        "<span id='status' class='st off'>未连接</span>"
        "<span style='margin-left:auto;font-size:12px;color:#565f89'>WebSocket 实时</span></div>"
        "<div class='content'>"
        "<div class='page active' id='pg-basic'>"
        "<div class='card'><h3>串口配置</h3><div class='row'>"
        "<select id='port'></select><input id='baud' value='115200'>"
        "<button id='open'>打开</button><button id='close' class='sec'>关闭</button></div></div>"
        "<div class='card'><h3>发送</h3><div class='row'>"
        "<input id='sendbox' placeholder='输入内容，回车发送'><button id='send'>发送</button>"
        "<label style='display:flex;align-items:center;gap:6px;margin:0'>"
        "<input type='checkbox' id='hex' style='width:auto'> HEX</label></div></div>"
        "<div class='card'><h3>接收</h3><div id='log'></div></div>"
        "</div>"
        "<div class='page' id='pg-term'>"
        "<div class='card'><h3>终端交互（敲命令并回显）</h3><div id='term'></div>"
        "<div class='row' style='margin-top:10px'><input id='termbox' placeholder='输入命令，回车发送'><button id='termsend'>发送</button></div>"
        "<div class='hint'>连接交互式设备（如 Linux shell / 串口命令行），原始字节透传。需先在上方打开串口。</div></div>"
        "</div>"
        "<div class='page' id='pg-proto'>"
        "<div class='card'><h3>协议字段配置</h3>"
        "<div id='protoFields'><div class='hint'>接收一帧后自动显示解析的字段名/类型/长度/值。</div></div>"
        "<div class='proto'><table><tr><th>字段</th><th>类型</th><th>长度</th><th>值</th></tr><tbody id='protoRows'></tbody></table></div>"
        "</div>"
        "</div>"
        "<div class='page' id='pg-viz'>"
        "<div class='card'><h3>波形可视化（字符画）</h3>"
        "<div class='row'><span class='pill on' id='vizWave'>波形</span><span class='pill' id='viz3d'>3D 姿态</span></div>"
        "<div id='viz'></div><div class='hint'>实时绘制接收数据的波形（发送数值数据观察变化）。</div></div>"
        "</div>"
        "</div></div>"
        "<script>"
        "var ws=new WebSocket('ws://'+location.host);"
        "var log=document.getElementById('log'),term=document.getElementById('term'),viz=document.getElementById('viz');"
        "function add(el,t){el.textContent+=t+'\\n';el.scrollTop=el.scrollHeight;}"
        "var vizKind='wave',vizBuf=[];"
        "ws.onmessage=function(e){var m=JSON.parse(e.data);"
        "if(m.type==='rx'){var d=m.data;add(log,'[RX] '+d);add(term,d);"
        "if(vizKind==='wave'){for(var i=0;i<m.bytes.length;i++)vizBuf.push(m.bytes[i]);if(vizBuf.length>200)vizBuf.shift();drawViz();}"
        "if(vizKind==='3d'){draw3d();}}"
        "if(m.type==='status'){var st=document.getElementById('status');st.textContent=m.open?'已连接':'未连接';st.className='st'+(m.open?'':' off');}"
        "if(m.type==='ports'){var p=document.getElementById('port');p.innerHTML='';"
        "m.ports.forEach(function(x){var o=document.createElement('option');o.text=x;p.add(o);});}};"
        "ws.onopen=function(){ws.send(JSON.stringify({cmd:'list'}));};"
        "function drawViz(){var H=8,W=80,h='';var max=255;"
        "for(var r=H-1;r>=0;r--){var line='';var th=(max/H)*(r+1);var tl=(max/H)*r;"
        "for(var c=0;c<W;c++){var idx=Math.floor(c/vizBuf.length>1?c*vizBuf.length/W:c);var v=idx<vizBuf.length?vizBuf[idx]:0;"
        "line+=(v>=tl&&v<th)?'█':(r===0?'_':' ');}"
        "h+=line+'\\n';}"
        "viz.textContent=h;viz.scrollTop=viz.scrollHeight;}"
        "function draw3d(){var t=new Date().getTime()/1000;var s='';"
        "for(var y=0;y<10;y++){var line='';for(var x=0;x<40;x++){var z=Math.sin(x/4+t)*Math.cos(y/3+t);line+=(z>0.5)?'▲':(z<-0.5)?'▼':'.';}s+=line+'\\n';}"
        "viz.textContent='3D 姿态（示意）\\n'+s;}"
        "document.getElementById('open').onclick=function(){ws.send(JSON.stringify({cmd:'open',port:document.getElementById('port').value,baud:parseInt(document.getElementById('baud').value)}));};"
        "document.getElementById('close').onclick=function(){ws.send(JSON.stringify({cmd:'close'}));};"
        "function sendCmd(box){var v=box.value;if(!v)return;"
        "ws.send(JSON.stringify({cmd:'send',data:v,hex:document.getElementById('hex').checked}));"
        "add(log,'[TX] '+v);box.value='';}"
        "document.getElementById('send').onclick=function(){sendCmd(document.getElementById('sendbox'));};"
        "document.getElementById('sendbox').onkeydown=function(e){if(e.key==='Enter')sendCmd(this);};"
        "document.getElementById('termsend').onclick=function(){var b=document.getElementById('termbox');var v=b.value;if(!v)return;"
        "ws.send(JSON.stringify({cmd:'send',data:v,hex:false}));add(term,'$ '+v);b.value='';};"
        "document.getElementById('termbox').onkeydown=function(e){if(e.key==='Enter')document.getElementById('termsend').click();};"
        "var navs=document.querySelectorAll('.nav');"
        "navs.forEach(function(n){n.onclick=function(){navs.forEach(function(x){x.classList.remove('active');});this.classList.add('active');"
        "var pg=this.getAttribute('data-p');document.querySelectorAll('.page').forEach(function(p){p.classList.remove('active');});"
        "document.getElementById('pg-'+pg).classList.add('active');};});"
        "document.getElementById('vizWave').onclick=function(){vizKind='wave';this.classList.add('on');document.getElementById('viz3d').classList.remove('on');};"
        "document.getElementById('viz3d').onclick=function(){vizKind='3d';this.classList.add('on');document.getElementById('vizWave').classList.remove('on');draw3d();};"
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
// openBrowser=true 时启动后自动用系统默认浏览器打开页面。
int runWeb(QCoreApplication& app, int port, bool openBrowser) {
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