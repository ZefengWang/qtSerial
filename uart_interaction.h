#ifndef SERIAL_H
#define SERIAL_H

#include <QMainWindow>
#include <QTimer>
#include <QDateTime>
#include "uart_core.h"
#include "qserialport.h"

namespace Ui {
class serial;
}

class serial : public QMainWindow
{
  Q_OBJECT

  #define NO_SERIAL_PORT tr("No Available Serial Port")

public:
  explicit serial(QWidget *parent = 0);
  ~serial();

private slots:
  void on_refreshButton_clicked();
  void on_openPortButton_clicked();
  void on_sendButton_clicked();
  void readSerialData();
  void on_clearTextButton_clicked();
  void on_clearRecvButton_clicked();
  void on_advancedSettingsBtn_clicked();
  void on_portComboBox_activated(const QString &arg1);
  void on_timerCheckBox_stateChanged(int state);
  void on_saveLogButton_clicked();
  void timerSendData();

private:
  void loadStyleSheet();
  void setupConnections();
  void refreshPortList();
  void updateConnectionStatus(bool connected);
  void appendReceiveData(const QString &text);
  void formatHexDisplay(QByteArray &data);
  QString formatByteCount(qint64 bytes);

private:
  Ui::serial *ui;
  Uartcore *uart_core_;
  QTimer *send_timer_;
  qint64 rx_quantity_ = 0;
  qint64 tx_quantity_ = 0;
  bool is_the_serial_port_open_ = false;
};

#endif // SERIAL_H
