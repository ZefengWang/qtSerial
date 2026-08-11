#include "uart_setting.h"
#include "uart_interaction.h"
#include "ui_uart_setting.h"
#include <QDebug>

setting::setting(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::setting){
  ui->setupUi(this);

  // 设置默认值
  ui->bitComboBox->setCurrentIndex(3);   // 默认8位数据位
  ui->stopComboBox->setCurrentIndex(0);  // 默认1位停止位
  ui->parityComboBox->setCurrentIndex(0);  // 默认无校验
  ui->flowctrComboBox->setCurrentIndex(0); // 默认无流控
}

void setting::find_available_serial_ports_and_add(Uartcore* serial_core) {
  uart_core_ = serial_core;
  QStringList serialStrList;
  serialStrList = uart_core_->serial_port_scanning();
  for (int i = 0; i < serialStrList.size(); i++) {
    ui->comComboBox->addItem(serialStrList[i]);
  }

  // 设置当前值
  if (!uart_core_->serial_name_.isEmpty()) {
    int idx = ui->comComboBox->findText(uart_core_->serial_name_);
    if (idx >= 0) ui->comComboBox->setCurrentIndex(idx);
  }

  if (uart_core_->baud_rate_ > 0) {
    QString baudStr = QString::number(uart_core_->baud_rate_);
    int idx = ui->baudComboBox->findText(baudStr);
    if (idx >= 0) {
      ui->baudComboBox->setCurrentIndex(idx);
    } else {
      ui->baudComboBox->addItem(baudStr);
      ui->baudComboBox->setCurrentText(baudStr);
    }
  }

  // 数据位
  if (uart_core_->data_bits_ >= 5 && uart_core_->data_bits_ <= 8) {
    ui->bitComboBox->setCurrentIndex(uart_core_->data_bits_ - 5);
  }

  // 停止位
  if (uart_core_->stop_bits_ >= 1 && uart_core_->stop_bits_ <= 3) {
    ui->stopComboBox->setCurrentIndex(uart_core_->stop_bits_ - 1);
  }

  // 校验位
  if (uart_core_->parity_bits_ >= 0 && uart_core_->parity_bits_ <= 5) {
    ui->parityComboBox->setCurrentIndex(uart_core_->parity_bits_);
  }

  // 流控
  if (uart_core_->flow_control_ >= 0 && uart_core_->flow_control_ <= 2) {
    ui->flowctrComboBox->setCurrentIndex(uart_core_->flow_control_);
  }
}

setting::~setting(){
  delete ui;
}

void setting::on_buttonBox_accepted(){
  // 兼容旧版 buttonBox 连接（如果有的话）
}

void setting::accept() {
  uart_core_->serial_name_ = ui->comComboBox->currentText();
  uart_core_->baud_rate_ = ui->baudComboBox->currentText().toInt();
  uart_core_->data_bits_ = ui->bitComboBox->currentText().toInt();
  uart_core_->stop_bits_ = ui->stopComboBox->currentText().toInt();

  // 校验位：0=None, 2=Even, 3=Odd, 4=Space, 5=Mark (QSerialPort::Parity 枚举值)
  int parityMap[] = {0, 2, 3, 4, 5};
  int parityIdx = ui->parityComboBox->currentIndex();
  if (parityIdx >= 0 && parityIdx < 5) {
    uart_core_->parity_bits_ = parityMap[parityIdx];
  }

  // 流控：0=None, 1=Hardware, 2=Software (QSerialPort::FlowControl 枚举值)
  uart_core_->flow_control_ = ui->flowctrComboBox->currentIndex();

  qDebug() << "serial_name_:" << uart_core_->serial_name_;
  qDebug() << "baud_rate_:" << uart_core_->baud_rate_;
  qDebug() << "data_bits_:" << uart_core_->data_bits_;
  qDebug() << "parity_bits_:" << uart_core_->parity_bits_;
  qDebug() << "stop_bits_:" << uart_core_->stop_bits_;
  qDebug() << "flow_control_:" << uart_core_->flow_control_;

  QDialog::accept();
}
