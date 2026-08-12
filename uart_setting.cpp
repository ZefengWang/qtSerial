#include "uart_setting.h"
#include "ui_uart_setting.h"
#include "launcher/UiMode.hpp"
#include <QDebug>
#include <QMessageBox>

setting::setting(sd::PortConfig& cfg,
                 const QStringList& availablePorts,
                 QWidget* parent) :
    QDialog(parent),
    ui(new Ui::setting),
    cfg_(cfg) {
  ui->setupUi(this);

  // 填端口列表
  for (const QString& p : availablePorts) {
    ui->comComboBox->addItem(p);
  }
  if (!cfg_.name.empty()) {
    int idx = ui->comComboBox->findText(QString::fromStdString(cfg_.name));
    if (idx >= 0) ui->comComboBox->setCurrentIndex(idx);
  }

  // 波特率
  if (cfg_.baudRate > 0) {
    QString baudStr = QString::number(cfg_.baudRate);
    int idx = ui->baudComboBox->findText(baudStr);
    if (idx >= 0) {
      ui->baudComboBox->setCurrentIndex(idx);
    } else {
      ui->baudComboBox->addItem(baudStr);
      ui->baudComboBox->setCurrentText(baudStr);
    }
  }

  // 数据位（5/6/7/8 -> 索引 0..3）
  if (cfg_.dataBits >= 5 && cfg_.dataBits <= 8) {
    ui->bitComboBox->setCurrentIndex(cfg_.dataBits - 5);
  }

  // 停止位（1/2/3 -> 索引，兼容旧版 OneAndHalfStop=3 的传递）
  if (cfg_.stopBits >= 1 && cfg_.stopBits <= 3) {
    ui->stopComboBox->setCurrentIndex(cfg_.stopBits - 1);
  }

  // 校验位（0=No,1=Even,2=Odd,3=Space,4=Mark -> 索引 0..4）
  if (cfg_.parity >= 0 && cfg_.parity <= 4) {
    ui->parityComboBox->setCurrentIndex(cfg_.parity);
  }

  // 流控（0=No,1=Hardware,2=Software -> 索引 0..2）
  if (cfg_.flowControl >= 0 && cfg_.flowControl <= 2) {
    ui->flowctrComboBox->setCurrentIndex(cfg_.flowControl);
  }

  // UI 模式下拉框：列出可选宿主，当前已保存模式选中。
  ui->uiModeComboBox->clear();
  ui->uiModeComboBox->addItem(ui_mode::displayName(UiMode::QtWidgets), QStringLiteral("qt"));
  ui->uiModeComboBox->addItem(ui_mode::displayName(UiMode::TUI),       QStringLiteral("tui"));
  ui->uiModeComboBox->addItem(ui_mode::displayName(UiMode::Web),       QStringLiteral("web"));
  if (ui_mode::qmlAvailable())
    ui->uiModeComboBox->addItem(ui_mode::displayName(UiMode::QML),     QStringLiteral("qml"));

  UiMode current = ui_mode::savedMode();
  if (current == UiMode::Auto) current = UiMode::QtWidgets;
  int curIdx = ui->uiModeComboBox->findData(ui_mode::toString(current));
  if (curIdx >= 0) ui->uiModeComboBox->setCurrentIndex(curIdx);
}

// 切换 UI 模式并重启：保存新模式到配置，提示后重启进程。
void setting::on_restartUiButton_clicked() {
  const QString modeStr = ui->uiModeComboBox->currentData().toString();
  UiMode target = ui_mode::fromString(modeStr);
  if (target == UiMode::Auto) return;

  const QString targetName = ui_mode::displayName(target);
  QMessageBox::StandardButton r = QMessageBox::question(
      this, tr("切换界面模式"),
      tr("将界面切换为「%1」，软件需要重启后生效。\n是否立即重启？").arg(targetName),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
  if (r != QMessageBox::Yes) return;

  ui_mode::saveMode(target);
  ui_mode::restartWithMode(target);
}

setting::~setting(){
  delete ui;
}

void setting::accept() {
  cfg_.name = ui->comComboBox->currentText().toStdString();
  cfg_.baudRate = ui->baudComboBox->currentText().toInt();
  cfg_.dataBits = ui->bitComboBox->currentText().toInt();
  cfg_.stopBits = ui->stopComboBox->currentText().toInt();

  // 校验位：索引 0..4 即 PortConfig/SerialSource 约定（0=No,1=Even,2=Odd,3=Space,4=Mark）
  cfg_.parity = ui->parityComboBox->currentIndex();

  // 流控：索引 0..2（0=No,1=Hardware,2=Software）
  cfg_.flowControl = ui->flowctrComboBox->currentIndex();

  qDebug() << "serial_name_:" << QString::fromStdString(cfg_.name);
  qDebug() << "baud_rate_:" << cfg_.baudRate;
  qDebug() << "data_bits_:" << cfg_.dataBits;
  qDebug() << "parity_bits_:" << cfg_.parity;
  qDebug() << "stop_bits_:" << cfg_.stopBits;
  qDebug() << "flow_control_:" << cfg_.flowControl;

  QDialog::accept();
}