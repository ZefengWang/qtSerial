#include "ui/SerialWorker.hpp"

#include "core/IClock.hpp"
#include "core/PortConfig.hpp"
#include "core/Frame.hpp"
#include "core/buffer/RingBuffer.hpp"
#include "io/SerialSource.hpp"
#include "service/EventBus.hpp"
#include "service/FieldPool.hpp"
#include "service/ProtocolEngine.hpp"
#include "service/Session.hpp"

#include <QDebug>

#include <algorithm>
#include <cstddef>
#include <vector>

SerialWorker::SerialWorker(QObject* parent)
    : QObject(parent),
      clock_(new sd::SteadyClock),
      source_(new sd::SerialSource),
      buffer_(new sd::RingBuffer(1 << 20)),          // 1MB 环形缓冲，内存恒定
      config_(new sd::PortConfig),
      bus_(new sd::EventBus),
      session_(new sd::Session(source_, buffer_, clock_, *bus_)),
      protoEngine_(new sd::ProtocolEngine(*bus_, sd::ProtocolRegistry{})),
      fieldPool_(new sd::FieldPool) {
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