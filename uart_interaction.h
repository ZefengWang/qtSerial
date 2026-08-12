#ifndef SERIAL_H
#define SERIAL_H

#include <QMainWindow>
#include <QTimer>
#include <QDateTime>
#include <QActionGroup>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include "ui/SerialWorker.hpp"
#include "core/FieldSchema.hpp"

namespace Ui {
class serial;
}

// 可视化视图实例（Qt Widgets 侧维护，绑定字段 + 类型）
struct ViewInstanceUi {
    QString viewId;         // ViewManager 的实例 id
    QString typeName;       // 波形图 / 3D姿态
    QStringList fields;     // 绑定的数据源字段
    QString title;
};

class serial : public QMainWindow
{
  Q_OBJECT

public:
  explicit serial(QWidget *parent = 0);
  ~serial();

protected:
  void changeEvent(QEvent *event) override;

private slots:
  // ---- 基础页 ----
  void on_refreshButton_clicked();
  void on_openPortButton_clicked();
  void on_sendButton_clicked();
  void readSerialData(const QByteArray &data);
  void on_clearTextButton_clicked();
  void on_clearRecvButton_clicked();
  void on_advancedSettingsBtn_clicked();
  void on_portComboBox_activated(const QString &arg1);
  void on_timerCheckBox_stateChanged(int state);
  void on_saveLogButton_clicked();
  void timerSendData();

  // ---- 导航 ----
  void on_navList_currentRowChanged(int row);

  // ---- 终端页 ----
  void on_termSendButton_clicked();
  void onTermInputReturnPressed();
  void on_termLocalEchoCheckBox_stateChanged(int /*state*/);

  // ---- 协议解析页 ----
  void on_addFieldButton_clicked();
  void on_removeFieldButton_clicked();
  void on_protoTable_itemSelectionChanged();
  void on_applyFieldButton_clicked();
  void on_applySchemaButton_clicked();

  // ---- 可视化页 ----
  void on_vizTypeCombo_currentIndexChanged(int index);
  void on_addViewButton_clicked();

  // ---- 帧/数据 ----
  void onFrameReceived(const sd::Frame &frame);

  // ---- Theme & Language ----
  void onThemeChanged(const QString &themeName);
  void onLanguageChanged(const QString &languageCode);

private:
  void loadStyleSheet();
  void setupConnections();
  void refreshPortList();
  void updateConnectionStatus(bool connected);
  void appendReceiveData(const QString &text);
  void appendRecv(const QString &text);
  void formatHexDisplay(QByteArray &data);
  QString formatByteCount(qint64 bytes);

  void setupMenus();
  void retranslateUi();

  // 协议解析辅助
  void addProtoRow(const sd::FieldDesc &fd);
  void rebuildProtoTable(const std::vector<sd::FieldDesc> &fields);
  void collectSchemaFromTable();
  sd::FieldType comboToFieldType(const QString &txt) const;
  QString fieldTypeToCombo(sd::FieldType t) const;

  // 可视化辅助
  void refreshVizSources();
  void rebuildVizViews();
  void addVizViewCard(const ViewInstanceUi &vi);
  void removeVizView(const QString &viewId);

  Ui::serial *ui;
  SerialWorker *worker_;
  QTimer *send_timer_;
  qint64 rx_quantity_ = 0;
  qint64 tx_quantity_ = 0;
  bool is_the_serial_port_open_ = false;

  // 终端命令历史
  QStringList cmdHistory_;
  int cmdHistoryPos_ = -1;

  // 协议模型（当前编辑中的字段）
  QVector<sd::FieldDesc> protoFields_;

  // 可视化实例（Qt 侧展示）
  QVector<ViewInstanceUi> vizViews_;

  // Menu actions
  QAction *m_actionExit;
  QAction *m_actionAbout;
  QMenu   *m_themeMenu;
  QMenu   *m_languageMenu;
  QActionGroup *m_themeGroup;
  QActionGroup *m_languageGroup;
};

#endif // SERIAL_H