#ifndef SRC_UI_SERIAL_WORKER_HPP
#define SRC_UI_SERIAL_WORKER_HPP

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

namespace sd {
struct PortConfig;
class SerialSource;
class RingBuffer;
class IBufferStrategy;
class Session;
class EventBus;
class SteadyClock;
class ProtocolEngine;
class FieldPool;
struct ProtocolSchema;
struct Frame;
} // namespace sd

// ============================================================
// SerialWorker —— UI 层适配器（门面）
//
// 把三层架构（core/service/io）封装成 Qt Widgets 友好的接口，
// 让主窗口不感知底层 Session/EventBus/DataSource 细节。
//
// 数据通路：
//   QSerialPort → SerialSource(read) → Session::poll() → RingBuffer
//              → EventBus(rx) → [订阅者在本对象内] → dataReceived 信号
//
// Session::poll() 由内部 QTimer 驱动，运行在创建本对象的线程（UI 线程），
// 因此 dataReceived 信号天然在 UI 线程发出，无需跨线程同步。
// ============================================================
class SerialWorker : public QObject {
    Q_OBJECT
    // QML 友好：isOpen 作为可读属性（变化时发出 connectionChanged 刷新）。
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY connectionChanged)

public:
    explicit SerialWorker(QObject* parent = nullptr);
    ~SerialWorker() override;

    // 扫描可用串口。
    Q_INVOKABLE QStringList scanPorts();

    // 打开串口（参数从 cfg 读取，成功后写入内部配置）。
    bool open(const sd::PortConfig& cfg);
    Q_INVOKABLE void close();
    bool isOpen() const { return open_; }

    // 发送数据，返回实际写入字节数（未打开时返回 0）。
    qint64 send(const QByteArray& data);

    // 最近一次错误的人类可读描述。
    Q_INVOKABLE QString lastError() const;

    // 串口参数配置（供高级设置对话框读写）。
    sd::PortConfig& config();
    const sd::PortConfig& config() const;

    // 缓冲区策略：0=环形, 1=双缓冲, 2=追加。打开中调用无效，需在 open() 前设置。
    void setBufferStrategy(int strategy);
    int  bufferStrategy() const { return bufferStrategy_; }

    // 十六进制字符串 -> 字节数组（纯工具，供发送框 HEX 模式使用）。
    static QByteArray hexStringToByteArray(const QString& hex);

    // ------ QML 桥接接口 ------
    // QML 无法直接构造 sd::PortConfig（C++ 结构体），用 QVariantMap 桥接 open()。
    Q_INVOKABLE bool openDevice(const QVariantMap& cfg);
    // QML 发送：接受字符串（文本）或 HEX 字节内容，return 实际写入字节数。
    Q_INVOKABLE qint64 sendData(const QVariant& data);
    // 实例版 HEX 工具（静态方法不能 Q_INVOKABLE，QML 只能调用实例方法）。
    Q_INVOKABLE QByteArray hexStringToByteArrayInstance(const QString& hex) const;
    // 把 JSON 描述的协议 schema 应用到协议引擎 + 字段池。
    // 格式: {"name":"cust","fields":[{"name":"f1","type":"float","length":4,"endian":"小端"},...]}
    Q_INVOKABLE bool applySchemaJson(const QString& json);
    // 返回字段池当前可用作可视化数据源的字段名列表。
    Q_INVOKABLE QStringList fieldPoolNames();

    // ------ 协议解析 / 可视化接入 ------
    // 应用一个可配置二进制协议 schema：注册到协议引擎并选中，同时写入字段池。
    // 返回是否成功（schema 需非空且含字段）。
    bool applyProtocolSchema(const sd::ProtocolSchema& schema);

    // 访问字段池（可视化配置界面读取可选数据源）。
    sd::FieldPool& fieldPool();
    const sd::FieldPool& fieldPool() const;

    // 协议引擎访问（供协议化发送等）。
    sd::ProtocolEngine& protocolEngine();

signals:
    // 收到一帧数据（已按攒批阈值切块）。UI 线程发出。
    void dataReceived(const QByteArray& data);
    // 连接状态变化（false=关闭，true=打开）。
    void connectionChanged(bool open);
    // 串口配置已被 service 成功应用（open 成功后发出），UI 应据此刷新参数显示。
    void configApplied(const sd::PortConfig& cfg);
    // 协议解析出一帧（帧数据到达字段池后发出，供可视化视图刷新）。
    void frameReceived(const sd::Frame& frame);

private:
    void poll();

    sd::SteadyClock* clock_ = nullptr;
    sd::SerialSource* source_ = nullptr;
    sd::IBufferStrategy* buffer_ = nullptr;
    sd::Session*      session_ = nullptr;
    sd::EventBus*     bus_ = nullptr;
    sd::ProtocolEngine* protoEngine_ = nullptr;
    sd::FieldPool*    fieldPool_ = nullptr;
    QTimer            pollTimer_;
    sd::PortConfig*   config_ = nullptr;
    bool              open_ = false;
    int               bufferStrategy_ = 0; // 0=环形, 1=双缓冲, 2=追加
};

#endif // SRC_UI_SERIAL_WORKER_HPP