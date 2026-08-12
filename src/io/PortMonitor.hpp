#ifndef SRC_IO_PORT_MONITOR_HPP
#define SRC_IO_PORT_MONITOR_HPP

// 串口热插拔监测器（可选功能）。
//
// 设计要点（对应修改清单 P5）：
//   - 不进 core 层：依赖 Qt，作为 io/UI 层可选组件；
//   - 平台事件驱动优先：Linux 上使用 QFileSystemWatcher 监听 /dev，
//     有设备增删时由内核事件通知，而非周期性轮询；
//   - 其他平台回退：周期性比对串口列表（可显式关闭以省开销）；
//   - 提供开关（enable/disable），并暴露添加/移除信号，供 UI 实时刷新。
//
// 用法：
//   PortMonitor m(worker);          // 关联扫描函数
//   QObject::connect(&m, &PortMonitor::portAdded, ...);
//   m.setEnabled(true);

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QFileSystemWatcher;
class QTimer;

namespace sd {
class SerialSource;
}

class PortMonitor : public QObject {
    Q_OBJECT

public:
    // watcher：可传入真实 SerialSource（调用其 scanPorts()）。
    // 用 std::function 拆掉对具体类型的强依赖，便于测试注入。
    using Scanner = std::function<QStringList()>;

    explicit PortMonitor(Scanner scanner, QObject* parent = nullptr);
    ~PortMonitor() override;

    // 开关：启用后开始监听，关闭即停止（省资源）。
    void setEnabled(bool on);
    bool isEnabled() const { return enabled_; }

    // 手动触发一次扫描（内部也用于回退路径）。
    void refresh();

    // 当前串口列表。
    QStringList ports() const { return ports_; }

signals:
    // 新插入串口（含设备名）。
    void portAdded(const QString& port);
    // 被移除的串口。
    void portRemoved(const QString& port);

private:
    void onFsWatch(const QString& path);
    void compare(const QStringList& fresh);

    Scanner               scanner_;
    QStringList           ports_;
    QFileSystemWatcher*   watcher_ = nullptr;
    QTimer*               fallbackTimer_ = nullptr;
    bool                  enabled_ = false;
    bool                  fsWatchSupported_ = false;
};

#endif // SRC_IO_PORT_MONITOR_HPP