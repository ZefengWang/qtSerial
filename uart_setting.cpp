#include "uart_setting.h"
#include "ui_uart_setting.h"
#include <QDebug>

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