#include "uart_interaction.h"
#include "uart_setting.h"
#include "ui_uart_interface.h"
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>

serial::serial(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::serial){
  ui->setupUi(this);

  // 加载样式表
  loadStyleSheet();

  // 初始化串口
  uart_core_ = new Uartcore;

  // 初始化定时发送定时器
  send_timer_ = new QTimer(this);
  connect(send_timer_, SIGNAL(timeout()), this, SLOT(timerSendData()));

  // 设置窗口图标
  setWindowIcon(QIcon(":/logo"));

  // 刷新串口列表
  refreshPortList();

  // 默认设置波特率为115200（第5项）
  ui->baudComboBox->setCurrentIndex(5);

  // 连接串口读取信号
  connect(uart_core_, SIGNAL(read_signal()), this, SLOT(readSerialData()));

  // 初始化连接状态显示
  updateConnectionStatus(false);
}

serial::~serial(){
  if (send_timer_->isActive()) {
    send_timer_->stop();
  }
  if (uart_core_) {
    uart_core_->close();
    delete uart_core_;
  }
  delete ui;
}

void serial::loadStyleSheet() {
  QFile styleFile(":/dark_style");
  if (styleFile.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(styleFile.readAll());
    qApp->setStyleSheet(styleSheet);
    styleFile.close();
  }
}

void serial::refreshPortList() {
  QStringList serialStrList;
  serialStrList = uart_core_->serial_port_scanning();
  ui->portComboBox->clear();
  for (int i = 0; i < serialStrList.size(); i++) {
    ui->portComboBox->addItem(serialStrList[i]);
  }
  if (serialStrList.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    ui->portComboBox->setPlaceholderText(tr("No ports found"));
#endif
  }
}

void serial::updateConnectionStatus(bool connected) {
  if (connected) {
    ui->statusIndicator->setStyleSheet("background-color: #9ece6a; border-radius: 5px;");
    ui->connectionStatusLabel->setText(tr("Connected"));
    QString portInfo = QString("%1 @ %2 baud")
        .arg(ui->portComboBox->currentText())
        .arg(ui->baudComboBox->currentText());
    ui->portInfoLabel->setText(portInfo);

    ui->portComboBox->setEnabled(false);
    ui->baudComboBox->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->advancedSettingsBtn->setEnabled(false);

    ui->openPortButton->setText(tr("Close Port"));
  } else {
    ui->statusIndicator->setStyleSheet("background-color: #f7768e; border-radius: 5px;");
    ui->connectionStatusLabel->setText(tr("Disconnected"));
    ui->portInfoLabel->setText(tr("No port selected"));

    ui->portComboBox->setEnabled(true);
    ui->baudComboBox->setEnabled(true);
    ui->refreshButton->setEnabled(true);
    ui->advancedSettingsBtn->setEnabled(true);

    ui->openPortButton->setText(tr("Open Port"));

    // 停止定时发送
    if (send_timer_->isActive()) {
      send_timer_->stop();
      ui->timerCheckBox->setChecked(false);
    }
  }
}

QString serial::formatByteCount(qint64 bytes) {
  if (bytes < 1024) {
    return QString::number(bytes);
  } else if (bytes < 1024 * 1024) {
    return QString::number(bytes / 1024.0, 'f', 1) + " K";
  } else {
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " M";
  }
}

void serial::appendReceiveData(const QString &text) {
  if (ui->timestampCheckBox->isChecked()) {
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    ui->recvBrowser->append(QString("[%1] %2").arg(timestamp, text));
  } else {
    ui->recvBrowser->append(text);
  }

  if (ui->autoScrollCheckBox->isChecked()) {
    QTextCursor cursor = ui->recvBrowser->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->recvBrowser->setTextCursor(cursor);
  }
}

void serial::on_refreshButton_clicked() {
  refreshPortList();
}

void serial::on_openPortButton_clicked() {
  if (ui->portComboBox->currentText().isEmpty()) {
    appendReceiveData(NO_SERIAL_PORT);
    return;
  }

  if (!is_the_serial_port_open_) {
    if (uart_core_->open(ui->portComboBox->currentText(),
                         ui->baudComboBox->currentText().toInt(),
                         uart_core_->data_bits_,
                         uart_core_->parity_bits_,
                         uart_core_->stop_bits_,
                         uart_core_->flow_control_)) {
      is_the_serial_port_open_ = true;
      updateConnectionStatus(true);
      appendReceiveData(tr("[System] Port opened successfully: %1").arg(ui->portComboBox->currentText()));
    } else {
      appendReceiveData(tr("[System] Failed to open port: %1").arg(ui->portComboBox->currentText()));
    }
  } else {
    uart_core_->close();
    is_the_serial_port_open_ = false;
    updateConnectionStatus(false);
    appendReceiveData(tr("[System] Port closed"));
  }
}

void serial::on_sendButton_clicked() {
  if (!is_the_serial_port_open_) {
    appendReceiveData(tr("[System] Please open the serial port first"));
    return;
  }

  QByteArray send_data;
  send_data = ui->sendTextEdit->toPlainText().toLatin1();

  if (ui->hexSendCheckBox->isChecked()) {
    send_data = uart_core_->hex_string_to_bytearray(ui->sendTextEdit->toPlainText());
  }

  if (ui->newlineCheckBox->isChecked()) {
    send_data += "\r\n";
  }

  if (send_data.length() <= 0) {
    return;
  }

  tx_quantity_ += send_data.length();
  ui->txCountLabel->setText(formatByteCount(tx_quantity_));
  uart_core_->send_data(send_data);
}

void serial::timerSendData() {
  if (is_the_serial_port_open_ && !ui->sendTextEdit->toPlainText().isEmpty()) {
    on_sendButton_clicked();
  }
}

void serial::on_timerCheckBox_stateChanged(int state) {
  if (state == Qt::Checked) {
    if (!is_the_serial_port_open_) {
      ui->timerCheckBox->setChecked(false);
      appendReceiveData(tr("[System] Please open the serial port first"));
      return;
    }
    int interval = ui->timerIntervalEdit->text().toInt();
    if (interval < 10) interval = 10;
    send_timer_->start(interval);
    appendReceiveData(tr("[System] Timer send started: %1 ms").arg(interval));
  } else {
    if (send_timer_->isActive()) {
      send_timer_->stop();
      appendReceiveData(tr("[System] Timer send stopped"));
    }
  }
}

// 读取从自定义串口类获得的数据
void serial::readSerialData() {
  QByteArray data = uart_core_->get_data_buffer_content();
  if (data.isEmpty()) return;

  if (ui->hexRecvCheckBox->isChecked()) {
    QString hexStr;
    for (int i = 0; i < data.size(); i++) {
      hexStr += QString("%1 ").arg((unsigned char)data.at(i), 2, 16, QChar('0')).toUpper();
    }
    appendReceiveData(hexStr.trimmed());
  } else {
    appendReceiveData(QString::fromLatin1(data));
  }

  rx_quantity_ += data.length();
  ui->rxCountLabel->setText(formatByteCount(rx_quantity_));

  uart_core_->clear_data_buffer_content();
}

void serial::on_clearTextButton_clicked() {
  ui->recvBrowser->clear();
  ui->sendTextEdit->clear();
  rx_quantity_ = 0;
  tx_quantity_ = 0;
  ui->rxCountLabel->setText("0");
  ui->txCountLabel->setText("0");
}

void serial::on_clearRecvButton_clicked() {
  ui->recvBrowser->clear();
  rx_quantity_ = 0;
  ui->rxCountLabel->setText("0");
}

void serial::on_advancedSettingsBtn_clicked() {
  setting param;
  param.find_available_serial_ports_and_add(uart_core_);
  param.exec();

  if (!uart_core_->serial_name_.isEmpty()) {
    ui->portComboBox->setCurrentText(uart_core_->serial_name_);
  }

  if (uart_core_->baud_rate_ > 0) {
    int index = ui->baudComboBox->findText(QString::number(uart_core_->baud_rate_));
    if (index >= 0) {
      ui->baudComboBox->setCurrentIndex(index);
    } else {
      ui->baudComboBox->addItem(QString::number(uart_core_->baud_rate_));
      ui->baudComboBox->setCurrentText(QString::number(uart_core_->baud_rate_));
    }
  }
}

void serial::on_portComboBox_activated(const QString &arg1) {
  uart_core_->serial_name_ = arg1;
}

void serial::on_saveLogButton_clicked() {
  if (ui->recvBrowser->toPlainText().isEmpty()) {
    appendReceiveData(tr("[System] No data to save"));
    return;
  }

  QString fileName = QFileDialog::getSaveFileName(
      this,
      tr("Save Log File"),
      QString("serial_log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
      tr("Text Files (*.txt);;All Files (*)")
  );

  if (fileName.isEmpty()) return;

  QFile file(fileName);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out << "Serial Port Debug Log\n";
    out << "========================\n";
    out << QString("Port: %1\n").arg(ui->portComboBox->currentText());
    out << QString("Baud Rate: %1\n").arg(ui->baudComboBox->currentText());
    out << QString("Generated: %1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    out << QString("Total RX: %1 bytes\n").arg(rx_quantity_);
    out << QString("Total TX: %1 bytes\n").arg(tx_quantity_);
    out << "========================\n\n";
    out << ui->recvBrowser->toPlainText();
    file.close();
    appendReceiveData(tr("[System] Log saved to: %1").arg(fileName));
  } else {
    appendReceiveData(tr("[System] Failed to save log file"));
  }
}
