#include "ui/SerialWorker.hpp"

#include "core/IClock.hpp"
#include "core/PortConfig.hpp"
#include "core/buffer/RingBuffer.hpp"
#include "io/SerialSource.hpp"
#include "service/EventBus.hpp"
#include "service/Session.hpp"

#include <QMessageBox>

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
      session_(new sd::Session(source_, buffer_, clock_, *bus_)) {
    // 订阅接收主题：数据经 EventBus 投递，这里转发为 Qt 信号。
    // 该回调在 Session::poll() 内同步执行（UI 线程），无跨线程问题。
    bus_->subscribe("rx", [this](const std::vector<std::uint8_t>& data) {
        if (data.empty()) return;
        QByteArray ba(reinterpret_cast<const char*>(data.data()),
                      static_cast<int>(data.size()));
        emit dataReceived(ba);
    });

    // 10ms 轮询：把串口数据拉入缓冲并发布到 EventBus。
    pollTimer_.setInterval(10);
    pollTimer_.setTimerType(Qt::PreciseTimer);
    connect(&pollTimer_, &QTimer::timeout, this, &SerialWorker::poll);
}

SerialWorker::~SerialWorker() {
    pollTimer_.stop();
    close();
    delete session_;
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
            QMessageBox::warning(nullptr, QObject::tr("Error"),
                                 QObject::tr("Illegal hex string: \"%1\"").arg(s));
            continue;
        }
        data.append(static_cast<char>(value & 0xFF));
    }
    return data;
}