#include "uart_interaction.h"
#include "uart_setting.h"
#include "thememanager.h"
#include "languagemanager.h"
#include "ui_uart_interface.h"
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <QEvent>
#include <QApplication>

serial::serial(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::serial){
  ui->setupUi(this);

  // 初始化串口（三层架构 UI 门面：Session + SerialSource + EventBus）
  worker_ = new SerialWorker(this);

  // 初始化定时发送定时器
  send_timer_ = new QTimer(this);
  connect(send_timer_, SIGNAL(timeout()), this, SLOT(timerSendData()));

  // 设置窗口图标
  setWindowIcon(QIcon(":/logo"));

  // 刷新串口列表
  refreshPortList();

  // 默认设置波特率为115200（第5项）
  ui->baudComboBox->setCurrentIndex(5);

  // 数据经 EventBus 到达后，由 SerialWorker 转发为本信号
  connect(worker_, &SerialWorker::dataReceived, this, &serial::readSerialData);

  // 初始化连接状态显示
  updateConnectionStatus(false);

  // 设置菜单（主题 + 语言 + 退出/关于）
  setupMenus();

  // 连接主题和语言变更信号
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
          this, &serial::onThemeChanged);
  connect(&LanguageManager::instance(), &LanguageManager::languageChanged,
          this, &serial::onLanguageChanged);
}

serial::~serial(){
  if (send_timer_->isActive()) {
    send_timer_->stop();
  }
  if (worker_) {
    worker_->close();
    // worker_ 由 Qt 父子机制释放（this 为 parent）
  }
  delete ui;
}

void serial::setupMenus() {
  QMenuBar *menuBar = this->menuBar();

  // --- View Menu (Theme + Language) ---
  QMenu *viewMenu = menuBar->addMenu(tr("View"));

  // Theme submenu
  m_themeMenu = viewMenu->addMenu(tr("Theme"));
  m_themeGroup = new QActionGroup(this);
  m_themeGroup->setExclusive(true);

  QString currentTheme = ThemeManager::instance().currentTheme();
  for (const QString &theme : ThemeManager::instance().availableThemes()) {
      QAction *act = m_themeMenu->addAction(ThemeManager::instance().themeDisplayName(theme));
      act->setCheckable(true);
      act->setChecked(theme == currentTheme);
      act->setData(theme);
      m_themeGroup->addAction(act);
      connect(act, &QAction::triggered, this, [this, theme]() {
          ThemeManager::instance().applyTheme(theme);
      });
  }

  viewMenu->addSeparator();

  // Language submenu
  m_languageMenu = viewMenu->addMenu(tr("Language"));
  m_languageGroup = new QActionGroup(this);
  m_languageGroup->setExclusive(true);

  QString currentLang = LanguageManager::instance().currentLanguage();
  for (const QString &lang : LanguageManager::instance().availableLanguages()) {
      QAction *act = m_languageMenu->addAction(LanguageManager::instance().languageDisplayName(lang));
      act->setCheckable(true);
      act->setChecked(lang == currentLang);
      act->setData(lang);
      m_languageGroup->addAction(act);
      connect(act, &QAction::triggered, this, [this, lang]() {
          LanguageManager::instance().setLanguage(lang);
      });
  }

  // --- Settings Menu ---
  QMenu *settingsMenu = menuBar->addMenu(tr("Settings"));
  QAction *m_settingsAction = settingsMenu->addAction(tr("Advanced Settings..."));
  connect(m_settingsAction, &QAction::triggered, this, &serial::on_advancedSettingsBtn_clicked);

  // --- Help Menu ---
  QMenu *helpMenu = menuBar->addMenu(tr("Help"));
  m_actionAbout = helpMenu->addAction(tr("About"));
  connect(m_actionAbout, &QAction::triggered, this, [this]() {
      QMessageBox::about(this, tr("About"),
          tr("<h3>Serial Debug Assistant</h3>"
             "<p>Version 2.0</p>"
             "<p>A modern cross-platform serial port debug tool.</p>"
             "<p>Built with Qt5/Qt6. Supports Linux (X11/Wayland) and Windows.</p>"));
  });
}

void serial::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
      retranslateUi();
  }
  QMainWindow::changeEvent(event);
}

void serial::retranslateUi() {
  // Retranslate all static UI labels from the .ui file
  ui->retranslateUi(this);

  // Retranslate menu titles
  menuBar()->clear();
  setupMenus();

  // Retranslate dynamic UI elements
  updateConnectionStatus(is_the_serial_port_open_);

  // Retranslate port combo placeholder
  if (ui->portComboBox->count() == 0) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      ui->portComboBox->setPlaceholderText(tr("No ports found"));
#endif
  }
}

void serial::onThemeChanged(const QString &themeName) {
  // Update checked state in theme menu
  for (QAction *act : m_themeGroup->actions()) {
      act->setChecked(act->data().toString() == themeName);
  }
}

void serial::onLanguageChanged(const QString &languageCode) {
  // Update checked state in language menu
  for (QAction *act : m_languageGroup->actions()) {
      act->setChecked(act->data().toString() == languageCode);
  }
}

void serial::loadStyleSheet() {
  // Now handled by ThemeManager
}

void serial::refreshPortList() {
  QStringList serialStrList = worker_->scanPorts();
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
    // 使用中性的通用色，在系统原生亮/暗风格下都可读
    ui->statusIndicator->setStyleSheet("background-color: #2ea043; border-radius: 5px;");
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
    ui->statusIndicator->setStyleSheet("background-color: #d1242f; border-radius: 5px;");
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
    appendReceiveData(tr("No Available Serial Port"));
    return;
  }

  if (!is_the_serial_port_open_) {
    sd::PortConfig cfg = worker_->config();
    cfg.name = ui->portComboBox->currentText().toStdString();
    cfg.baudRate = ui->baudComboBox->currentText().toInt();
    if (worker_->open(cfg)) {
      is_the_serial_port_open_ = true;
      updateConnectionStatus(true);
      appendReceiveData(tr("[System] Port opened successfully: %1").arg(ui->portComboBox->currentText()));
    } else {
      appendReceiveData(tr("[System] Failed to open port: %1").arg(ui->portComboBox->currentText()));
      // Show detailed error (permission, etc.) in the receive panel
      QString errMsg = worker_->lastError();
      if (!errMsg.isEmpty()) {
          appendReceiveData(errMsg);
      }
    }
  } else {
    worker_->close();
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
    send_data = SerialWorker::hexStringToByteArray(ui->sendTextEdit->toPlainText());
  }

  if (ui->newlineCheckBox->isChecked()) {
    send_data += "\r\n";
  }

  if (send_data.length() <= 0) {
    return;
  }

  tx_quantity_ += send_data.length();
  ui->txCountLabel->setText(formatByteCount(tx_quantity_));
  worker_->send(send_data);
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

// 处理经 EventBus 到达的数据（由 SerialWorker::dataReceived 触发）
void serial::readSerialData(const QByteArray &data) {
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
  // 创建模态对话框，传入 SerialWorker 的配置引用与可用端口列表
  QStringList availablePorts = worker_->scanPorts();
  setting param(worker_->config(), availablePorts, this);

  // 保留标题栏/关闭按钮，去掉最小化/最大化
  param.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

  // 应用级模态：阻塞整个应用的所有窗口，焦点不会穿透到主窗口
  param.setModal(true);
  param.setWindowModality(Qt::ApplicationModal);

  // 调整为实际内容大小
  param.adjustSize();

  // 居中到主窗口（this 是顶层窗口，parentWidget() 为 null，需以主窗口几何为中心）
  QRect parentGeometry = this->geometry();
  QSize dlgSize = param.size();
  // 防止对话框比主窗口还大导致偏移越界
  int x = parentGeometry.center().x() - dlgSize.width() / 2;
  int y = parentGeometry.center().y() - dlgSize.height() / 2;
  x = qMax(x, parentGeometry.left());
  y = qMax(y, parentGeometry.top());
  param.move(x, y);

  // 模态执行（accept() 会把对话框选择写回 worker_->config()）
  param.exec();

  const sd::PortConfig& cfg = worker_->config();
  if (!cfg.name.empty()) {
    ui->portComboBox->setCurrentText(QString::fromStdString(cfg.name));
  }

  if (cfg.baudRate > 0) {
    int index = ui->baudComboBox->findText(QString::number(cfg.baudRate));
    if (index >= 0) {
      ui->baudComboBox->setCurrentIndex(index);
    } else {
      ui->baudComboBox->addItem(QString::number(cfg.baudRate));
      ui->baudComboBox->setCurrentText(QString::number(cfg.baudRate));
    }
  }
}

void serial::on_portComboBox_activated(const QString &arg1) {
  worker_->config().name = arg1.toStdString();
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
