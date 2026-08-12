#include "PortMonitor.hpp"

#include <QFileSystemWatcher>
#include <QTimer>

#include <functional>

PortMonitor::PortMonitor(Scanner scanner, QObject* parent)
    : QObject(parent), scanner_(std::move(scanner)) {
    // 平台事件驱动：Linux/Unix 上监听 /dev 目录，内核在有设备增删时通知。
    // 监听失败（如 /dev 不存在）则回退到定时扫描。
    watcher_ = new QFileSystemWatcher(this);
    fsWatchSupported_ = watcher_->addPath(QStringLiteral("/dev"));
    if (fsWatchSupported_) {
        connect(watcher_, &QFileSystemWatcher::directoryChanged,
                this, &PortMonitor::onFsWatch);
    }

    // 回退路径：非事件驱动平台（或 /dev 无法监听）用定时比对。
    fallbackTimer_ = new QTimer(this);
    fallbackTimer_->setInterval(1000);
    connect(fallbackTimer_, &QTimer::timeout, this, &PortMonitor::refresh);

    refresh(); // 初始列表
}

PortMonitor::~PortMonitor() = default;

void PortMonitor::setEnabled(bool on) {
    if (enabled_ == on) return;
    enabled_ = on;
    if (!enabled_) {
        fallbackTimer_->stop();
        return;
    }
    if (!fsWatchSupported_) {
        fallbackTimer_->start();
    }
    refresh();
}

void PortMonitor::refresh() {
    if (scanner_) {
        compare(scanner_());
    }
}

void PortMonitor::onFsWatch(const QString& path) {
    Q_UNUSED(path);
    refresh();
}

void PortMonitor::compare(const QStringList& fresh) {
    // 新增：fresh 中存在、旧列表中没有。
    for (const auto& p : fresh) {
        if (!ports_.contains(p)) emit portAdded(p);
    }
    // 移除：旧列表中存在、fresh 中没有。
    for (const auto& p : ports_) {
        if (!fresh.contains(p)) emit portRemoved(p);
    }
    ports_ = fresh;
}