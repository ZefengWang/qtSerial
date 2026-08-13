#include "ui/SerialWorker.hpp"

#include "core/IClock.hpp"
#include "core/PortConfig.hpp"
#include "core/Frame.hpp"
#include "core/buffer/RingBuffer.hpp"
#include "core/buffer/DoubleBuffer.hpp"
#include "core/buffer/AppendBuffer.hpp"
#include "io/SerialSource.hpp"
#include "service/EventBus.hpp"
#include "service/FieldPool.hpp"
#include "service/ProtocolEngine.hpp"
#include "service/Session.hpp"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cstddef>
#include <vector>

SerialWorker::SerialWorker(QObject* parent)
    : QObject(parent),
      clock_(new sd::SteadyClock),
      source_(new sd::SerialSource),
      buffer_(new sd::RingBuffer(1 << 20)),          // 1MB 环形缓冲，内存恒定
      bus_(new sd::EventBus),
      session_(new sd::Session(source_, buffer_, clock_, *bus_)),
      protoEngine_(new sd::ProtocolEngine(*bus_, sd::ProtocolRegistry{})),
      fieldPool_(new sd::FieldPool),
      config_(new sd::PortConfig) {
    // 订阅接收主题：数据经 EventBus 投递，这里转发为 Qt 信号。
    // 该回调在 Session::poll() 内同步执行（UI 线程），无跨线程问题。
    bus_->subscribe("rx", [this](const std::vector<std::uint8_t>& data) {
        if (data.empty()) return;
        QByteArray ba(reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()));
        emit dataReceived(ba);
    });

    // 订阅帧主题：协议解析出一帧后更新字段池，并转发为 Qt 信号（可视化刷新）。
    bus_->subscribeFrame("frames", [this](const sd::Frame& frame) {
        fieldPool_->onFrame(frame);
        emit frameReceived(frame);
    });

    // 10ms 轮询：把串口数据拉入缓冲并发布到 EventBus。
    pollTimer_.setInterval(10);
    pollTimer_.setTimerType(Qt::PreciseTimer);
    connect(&pollTimer_, &QTimer::timeout, this, &SerialWorker::poll);
}

SerialWorker::~SerialWorker() {
    pollTimer_.stop();
    close();
    // 依赖 bus_ 的对象必须先于 bus_ 删除（它们析构时会访问 bus_/unsubscribe）。
    delete protoEngine_;
    delete session_;
    delete fieldPool_;
    delete bus_;
    delete config_;
    delete buffer_;
    delete source_;
    delete clock_;
}

QStringList SerialWorker::scanPorts() {
    QStringList list;
    for (const auto& s : source_->scanPorts()) {
        list << QString::fromStdString(s);
    }
    return list;
}

bool SerialWorker::open(const sd::PortConfig& cfg) {
    if (open_) return true;
    if (!session_->open(cfg)) return false;
    *config_ = cfg; // 记下成功打开的配置
    open_ = true;
    pollTimer_.start();
    emit connectionChanged(true);
    emit configApplied(cfg); // service 已成功应用配置，通知 UI 刷新
    return true;
}

void SerialWorker::close() {
    if (!open_) return;
    pollTimer_.stop();
    session_->close();
    open_ = false;
    emit connectionChanged(false);
}

qint64 SerialWorker::send(const QByteArray& data) {
    if (!open_ || data.isEmpty()) return 0;
    std::vector<std::uint8_t> v(static_cast<std::size_t>(data.size()));
    std::copy(data.constBegin(), data.constEnd(), v.begin());
    return static_cast<qint64>(session_->send(v));
}

QString SerialWorker::lastError() const {
    return QString::fromStdString(source_->lastError());
}

sd::PortConfig& SerialWorker::config() { return *config_; }
const sd::PortConfig& SerialWorker::config() const { return *config_; }

void SerialWorker::setBufferStrategy(int strategy) {
    if (open_) return; // 打开中不支持动态切换
    const int s = (strategy < 0 || strategy > 2) ? 0 : strategy;
    if (s == bufferStrategy_) return;
    sd::IBufferStrategy* nb = nullptr;
    switch (s) {
        case 1: nb = new sd::DoubleBuffer(); break;   // 2MB 双缓冲
        case 2: nb = new sd::AppendBuffer(); break;   // 4MB 追加缓冲
        default: nb = new sd::RingBuffer(1 << 20);    // 1MB 环形缓冲
    }
    bufferStrategy_ = s;
    delete buffer_;
    buffer_ = nb;
    session_->setBuffer(buffer_);
}

void SerialWorker::poll() {
    if (!open_) return;
    session_->poll(); // read + buffer write + EventBus publish
}

QByteArray SerialWorker::hexStringToByteArray(const QString& hex) {
    QByteArray data;
    const QString cleaned = hex.trimmed().simplified();
    const QStringList tokens = cleaned.split(QLatin1Char(' '));
    bool ok = false;
    for (const QString& s : tokens) {
        if (s.isEmpty()) continue;
        const int value = s.toInt(&ok, 16);
        if (!ok) {
            qWarning() << "Illegal hex string:" << s;
            continue;
        }
        data.append(static_cast<char>(value & 0xFF));
    }
    return data;
}

bool SerialWorker::applyProtocolSchema(const sd::ProtocolSchema& schema) {
    if (schema.name.empty() || schema.fields.empty()) return false;
    // 注册并选中协议，同时把 schema 装载进字段池（供可视化列出数据源）。
    if (!protoEngine_->applySchema(schema)) return false;
    fieldPool_->setSchema(schema);
    return true;
}

sd::FieldPool& SerialWorker::fieldPool() { return *fieldPool_; }
const sd::FieldPool& SerialWorker::fieldPool() const { return *fieldPool_; }

sd::ProtocolEngine& SerialWorker::protocolEngine() { return *protoEngine_; }

// ============================================================
// QML 桥接接口实现
// ============================================================

bool SerialWorker::openDevice(const QVariantMap& cfg) {
    sd::PortConfig c = *config_; // 保留其余默认/已配置参数
    bool ok = false;
    c.name = cfg.value("name").toString().toStdString();
    int v = cfg.value("baudRate").toInt(&ok); if (ok) c.baudRate = v;
    v = cfg.value("dataBits").toInt(&ok);     if (ok) c.dataBits = v;
    v = cfg.value("parity").toInt(&ok);       if (ok) c.parity = v;
    v = cfg.value("stopBits").toInt(&ok);     if (ok) c.stopBits = v;
    v = cfg.value("flowControl").toInt(&ok);  if (ok) c.flowControl = v;
    return open(c);
}

QByteArray SerialWorker::hexStringToByteArrayInstance(const QString& hex) const {
    return hexStringToByteArray(hex);
}

qint64 SerialWorker::sendData(const QVariant& data) {
    if (data.canConvert<QByteArray>())
        return send(data.toByteArray());
    // 字符串按 UTF-8 编码发送。
    return send(data.toString().toUtf8());
}

// 把字段类型字符串映射为 FieldType（未知/空 -> F32）。
namespace {
sd::FieldType qmlFieldType(const QString& t) {
    QString s = t.toLower();
    if (s == "uint8")  return sd::FieldType::U8;
    if (s == "uint16") return sd::FieldType::U16;
    if (s == "uint32") return sd::FieldType::U32;
    if (s == "int8")   return sd::FieldType::I8;
    if (s == "int16")  return sd::FieldType::I16;
    if (s == "int32")  return sd::FieldType::I32;
    if (s == "double") return sd::FieldType::F64;
    if (s == "bool")   return sd::FieldType::Bool;
    return sd::FieldType::F32; // float / 未知
}
} // namespace

bool SerialWorker::applySchemaJson(const QString& json) {
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError) return false;
    QJsonObject root = doc.object();

    sd::ProtocolSchema schema;
    schema.name = root.value("name").toString("cust").toStdString();

    QJsonArray fields = root.value("fields").toArray();
    for (const auto& v : fields) {
        QJsonObject fo = v.toObject();
        sd::FieldDesc fd;
        fd.name = fo.value("name").toString(QStringLiteral("field")).toStdString();
        QString type = fo.value("type").toString();
        fd.type = qmlFieldType(type);
        fd.byteLength = fo.value("length").toInt(-1);
        fd.isPadding = (type.compare("padding", Qt::CaseInsensitive) == 0);
        fd.bigEndian = fo.value("endian").toString() == QLatin1String("大端");
        // dataSource=false 表示"不作为可视化数据源"；缺省视为 true。
        // （QML 协议页的 selected 仅用于"选中编辑"，不是数据源开关。）
        fd.dataSource = fo.value("dataSource").toBool(true);
        schema.fields.push_back(fd);
    }
    return applyProtocolSchema(schema);
}

QStringList SerialWorker::fieldPoolNames() {
    QStringList names;
    for (const auto& s : fieldPool_->sources())
        names << QString::fromStdString(s.name);
    return names;
}