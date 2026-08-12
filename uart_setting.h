#ifndef SETTING_H
#define SETTING_H

#include <QDialog>
#include <QStringList>

#include "core/PortConfig.hpp"

namespace Ui {
class setting;
}

// 高级串口参数设置对话框。
// 与核心层解耦：只读写 sd::PortConfig（不依赖具体硬件/Service 实现）。
class setting : public QDialog
{
    Q_OBJECT

public:
    // cfg 由调用方持有（SerialWorker 的配置），对话框 accept() 时写回。
    explicit setting(sd::PortConfig& cfg,
                     const QStringList& availablePorts,
                     QWidget *parent = nullptr);
    ~setting();

protected:
    void accept() override;

private:
    Ui::setting *ui;
    sd::PortConfig& cfg_;
};

#endif // SETTING_H