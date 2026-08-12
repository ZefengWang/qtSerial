#include "io/SerialSource.hpp"

#include <QSerialPortInfo>

namespace sd {

SerialSource::SerialSource() : port_(new QSerialPort) {}

SerialSource::~SerialSource() {
    if (port_->isOpen()) port_->close();
    delete port_;
}

static QSerialPort::DataBits toDataBits(int v) {
    switch (v) {
        case 5: return QSerialPort::Data5;
        case 6: return QSerialPort::Data6;
        case 7: return QSerialPort::Data7;
        default: return QSerialPort::Data8;
    }
}

static QSerialPort::Parity toParity(int v) {
    switch (v) {
        case 1: return QSerialPort::EvenParity;
        case 2: return QSerialPort::OddParity;
        case 3: return QSerialPort::SpaceParity;
        case 4: return QSerialPort::MarkParity;
        default: return QSerialPort::NoParity;
    }
}

static QSerialPort::StopBits toStopBits(int v) {
    switch (v) {
        case 1: return QSerialPort::OneStop;
        case 2: return QSerialPort::TwoStop;
        default: return QSerialPort::OneStop;
    }
}

static QSerialPort::FlowControl toFlowControl(int v) {
    switch (v) {
        case 1: return QSerialPort::HardwareControl;
        case 2: return QSerialPort::SoftwareControl;
        default: return QSerialPort::NoFlowControl;
    }
}

bool SerialSource::open(const PortConfig& cfg) {
    port_->setPortName(QString::fromStdString(cfg.name));
    port_->setBaudRate(cfg.baudRate);
    port_->setDataBits(toDataBits(cfg.dataBits));
    port_->setParity(toParity(cfg.parity));
    port_->setStopBits(toStopBits(cfg.stopBits));
    port_->setFlowControl(toFlowControl(cfg.flowControl));
    port_->setReadBufferSize(0); // 缓冲区大小设为无限，由上层缓冲策略管理

    if (port_->open(QIODevice::ReadWrite)) {
        lastError_.clear();
        return true;
    }
    lastError_ = port_->errorString().toStdString();
    return false;
}

void SerialSource::close() {
    if (port_->isOpen()) port_->close();
}

std::vector<std::uint8_t> SerialSource::read() {
    if (!port_->isOpen()) return {};
    QByteArray raw = port_->readAll();
    const auto* p   = reinterpret_cast<const std::uint8_t*>(raw.constData());
    return {p, p + static_cast<std::size_t>(raw.size())};
}

std::size_t SerialSource::write(const std::vector<std::uint8_t>& data) {
    if (!port_->isOpen()) {
        lastError_ = "port not open";
        return 0;
    }
    if (data.empty()) return 0;
    qint64 n = port_->write(reinterpret_cast<const char*>(data.data()),
                            static_cast<qint64>(data.size()));
    if (n < 0) {
        lastError_ = port_->errorString().toStdString();
        return 0;
    }
    return static_cast<std::size_t>(n);
}

std::vector<std::string> SerialSource::scanPorts() {
    std::vector<std::string> out;
    const auto ports = QSerialPortInfo::availablePorts();
    out.reserve(static_cast<std::size_t>(ports.size()));
    for (const auto& info : ports) {
        out.push_back(info.portName().toStdString());
    }
    return out;
}

std::string SerialSource::lastError() const { return lastError_; }

} // namespace sd