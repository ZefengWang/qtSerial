#include "uart_interaction.h"
#include "uart_setting.h"
#include "thememanager.h"
#include "languagemanager.h"
#include "ui_uart_interface.h"
#include "service/ViewManager.hpp"

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QEvent>
#include <QApplication>
#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidgetItem>
#include <QListView>
#include <QTableWidgetItem>
#include <QCheckBox>
#include <QHeaderView>
#include <QScrollArea>
#include <QFrame>
#include <QPolygonF>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <cmath>

// 校验位缩写：0=None,1=Even,2=Odd,3=Space,4=Mark
static QString parityShortName(int parity) {
    switch (parity) {
        case 1: return QLatin1String("E");
        case 2: return QLatin1String("O");
        case 3: return QLatin1String("S");
        case 4: return QLatin1String("M");
        default: return QLatin1String("N");
    }
}

// ============================================================
// 私有辅助：一个可绘制波形/3D 姿态的视图卡片。
// 直接以自定义绘制实现，避免额外依赖 charts 模块（环境无 QtCharts）。
// ============================================================
class VizPlotWidget : public QWidget {
public:
    explicit VizPlotWidget(const QString &typeName, const QStringList &fields, QWidget *parent = nullptr)
        : QWidget(parent), typeName_(typeName), fields_(fields) {
        // 每个字段独立数据缓冲（环形采样），示例数据用于演示。
        for (int i = 0; i < fields_.size(); ++i) {
            series_.append(QVector<double>());
            phase_.append(0.0);
        }
        setMinimumHeight(140);
    }

    void pushSample(const sd::Frame &frame) {
        if (typeName_ == QLatin1String("3D姿态"))
            push3D(frame);
        else
            pushWave(frame);
        update();
    }

    void pushEmpty() {
        // 无数据时也推进演示曲线，便于界面可见。
        for (int i = 0; i < series_.size(); ++i) {
            phase_[i] += 0.35;
            double v = std::sin(phase_[i]) * 30.0 + 50.0;
            series_[i].append(v);
            if (series_[i].size() > 200) series_[i].removeFirst();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#12121a"));

        // 网格
        p.setPen(QPen(QColor(255, 255, 255, 18), 1));
        int step = 20;
        for (int x = 0; x < width(); x += step) p.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += step) p.drawLine(0, y, width(), y);

        if (typeName_ == QLatin1String("3D姿态")) {
            draw3D(p);
        } else {
            drawWave(p);
        }

        // 字段图例
        int lx = 8, ly = 14;
        p.setPen(Qt::white);
        p.drawText(8, height() - 8,
                   QString("%1 · %2").arg(typeName_, fields_.join(" + ")));
        Q_UNUSED(lx); Q_UNUSED(ly);
    }

private:
    void drawWave(QPainter &p) {
        static const QColor palette[] = {
            QColor("#7aa2f7"), QColor("#9ece6a"), QColor("#e0af68"),
            QColor("#f7768e"), QColor("#bb9af7")};
        for (int s = 0; s < series_.size(); ++s) {
            if (series_[s].isEmpty()) continue;
            QColor c = palette[s % 5];
            p.setPen(QPen(c, 1.8));
            QPolygonF poly;
            int n = series_[s].size();
            for (int i = 0; i < n; ++i) {
                double x = width() * i / 200.0;
                double y = height() - height() * (series_[s].at(i) / 100.0);
                poly << QPointF(x, y);
            }
            p.drawPolyline(poly);
        }
    }

    void draw3D(QPainter &p) {
        // 简化 3D 姿态：根据 roll/pitch/yaw 三个字段值绘制一个"姿态方块"。
        double roll = 0, pitch = 0, yaw = 0;
        if (series_.size() >= 1) roll  = series_[0].isEmpty() ? 0 : (series_[0].last() - 50) / 50.0;
        if (series_.size() >= 2) pitch = series_[1].isEmpty() ? 0 : (series_[1].last() - 50) / 50.0;
        if (series_.size() >= 3) yaw   = series_[2].isEmpty() ? 0 : (series_[2].last() - 50) / 50.0;

        int cx = width() / 2, cy = height() / 2;
        int half = qMin(width(), height()) / 6;
        QColor body("#7aa2f7");
        p.setPen(QPen(body, 2));
        QVector<QPointF> face = {
            QPointF(cx - half, cy - half + pitch * 20),
            QPointF(cx + half, cy - half + roll * 20),
            QPointF(cx + half, cy + half - yaw * 20),
            QPointF(cx - half, cy + half)};
        p.setBrush(QColor(122, 162, 247, 40));
        p.drawPolygon(face);

        // 三轴参考
        p.setPen(QPen(QColor(255, 255, 255, 60), 1));
        p.drawLine(cx - half - 20, cy, cx + half + 20, cy);
        p.drawLine(cx, cy - half - 20, cx, cy + half + 20);
    }

    QString typeName_;
    QStringList fields_;
    QVector<QVector<double>> series_;
    QVector<double> phase_;

    void pushWave(const sd::Frame &frame) {
        for (int s = 0; s < fields_.size(); ++s) {
            bool found = false;
            double v = frame.numericValue(fields_.at(s).toStdString(), &found);
            if (found) {
                series_[s].append(v);
                if (series_[s].size() > 200) series_[s].removeFirst();
            }
        }
    }
    void push3D(const sd::Frame &frame) {
        for (int s = 0; s < fields_.size(); ++s) {
            bool found = false;
            double v = frame.numericValue(fields_.at(s).toStdString(), &found);
            if (found) {
                series_[s].append(v);
                if (series_[s].size() > 200) series_[s].removeFirst();
            }
        }
    }
};

// ============================================================
// 表格委托类（协议表：勾选列不可编辑、类型列下拉选择）
// ============================================================
class NoEditDelegate : public QStyledItemDelegate {
public:
    explicit NoEditDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override {
        return nullptr; // 不可编辑
    }
};

class FieldTypeDelegate : public QStyledItemDelegate {
public:
    explicit FieldTypeDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
        auto *combo = new QComboBox(parent);
        combo->addItems({"uint8", "uint16", "uint32", "int8", "int16", "int32",
                         "float", "double", "bool", "padding"});
        return combo;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        auto *combo = qobject_cast<QComboBox*>(editor);
        if (!combo) return;
        QString txt = index.data(Qt::EditRole).toString();
        int idx = combo->findText(txt);
        if (idx >= 0) combo->setCurrentIndex(idx);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        auto *combo = qobject_cast<QComboBox*>(editor);
        if (!combo) return;
        model->setData(index, combo->currentText(), Qt::EditRole);
    }
};

// ============================================================
// 主窗口
// ============================================================
serial::serial(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::serial){
  ui->setupUi(this);

  // 初始化串口（三层架构 UI 门面：Session + SerialSource + EventBus）
  worker_ = new SerialWorker(this);

  // 初始化定时发送定时器
  send_timer_ = new QTimer(this);
  connect(send_timer_, SIGNAL(timeout()), this, SLOT(timerSendData()));

  // RX 速率计时器（每秒刷新一次显示）
  rx_rate_timer_ = new QTimer(this);
  rx_rate_timer_->setInterval(1000);
  connect(rx_rate_timer_, &QTimer::timeout, this, [this](){
      qint64 now = QDateTime::currentMSecsSinceEpoch();
      qint64 elapsed = now - rx_rate_window_start_;
      if (elapsed <= 0) return;
      double rate = (double)rx_rate_bytes_ * 1000.0 / (double)elapsed;
      ui->rxRateValueLabel->setText(formatByteCount((qint64)rate) + "/s");
      // 重置窗口
      rx_rate_bytes_ = 0;
      rx_rate_window_start_ = now;
  });

  // 设置窗口图标
  setWindowIcon(QIcon(":/logo"));

  // 刷新串口列表
  refreshPortList();

  // 默认设置波特率为115200（第5项），并支持自定义输入
  ui->baudComboBox->setEditable(true);
  ui->baudComboBox->setCurrentIndex(5);

  // 初始化左侧数据位/校验/停止（从 worker_->config() 读取，合并为 "8 N 1" 格式）
  const sd::PortConfig &cfg0 = worker_->config();
  {
    QSignalBlocker bf(ui->dataBitsField);
    ui->dataBitsField->setText(QString("%1 %2 %3")
        .arg(QString::number(cfg0.dataBits))
        .arg(parityShortName(cfg0.parity))
        .arg(QString::number(cfg0.stopBits, 'g', 2)));
  }

  // 数据经 EventBus 到达后，由 SerialWorker 转发为本信号
  connect(worker_, &SerialWorker::dataReceived, this, &serial::readSerialData);
  // 协议帧：更新字段池并由可视化视图消费
  connect(worker_, &SerialWorker::frameReceived, this, &serial::onFrameReceived);
  // 连接状态变化（串口被拔出/外部关闭时同步 UI）
  connect(worker_, &SerialWorker::connectionChanged, this, [this](bool connected) {
    is_the_serial_port_open_ = connected;
    updateConnectionStatus(connected);
    if (!connected) {
      appendReceiveData(tr("[System] Serial port disconnected"));
    }
  });

  // 初始化连接状态显示
  updateConnectionStatus(false);

  // 配置导航（QListWidget 四页 -> QStackedWidget）
  setupConnections();

  // 终端输入框回车发送
  connect(ui->termInput, &QLineEdit::returnPressed, this, &serial::onTermInputReturnPressed);

  // 连接主题和语言变更信号（更新设置页下拉框）
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
          this, &serial::onThemeChanged);
  connect(&LanguageManager::instance(), &LanguageManager::languageChanged,
          this, &serial::onLanguageChanged);

  // 初始化协议表头
  ui->protoTable->setColumnCount(4);
  QStringList hdr;
  hdr << tr("勾选") << tr("名称") << tr("类型") << tr("长度");
  ui->protoTable->setHorizontalHeaderLabels(hdr);
  ui->protoTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  ui->protoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui->protoTable->setSelectionMode(QAbstractItemView::SingleSelection);
  // 表格可直接编辑（名称、长度列）
  ui->protoTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                   QAbstractItemView::EditKeyPressed |
                                   QAbstractItemView::AnyKeyPressed);
  // 勾选列不可编辑
  ui->protoTable->setItemDelegateForColumn(0, new NoEditDelegate(this));
  // 类型列用 ComboBox 代理
  ui->protoTable->setItemDelegateForColumn(2, new FieldTypeDelegate(this));

  // 初始化帧布局预览（使用 .ui 中的 framePreviewBar / framePreviewLegend）
  protoLayoutBar_ = ui->framePreviewBar;
  protoLayoutBarLayout_ = qobject_cast<QHBoxLayout*>(ui->framePreviewBar->layout());
  protoLayoutLegend_ = ui->framePreviewLegend;
  protoLayoutLegendLayout_ = qobject_cast<QHBoxLayout*>(ui->framePreviewLegend->layout());
  renderProtoLayoutPreview();

  // 初始化可视化源列表
  ui->vizSourceList->setSelectionMode(QAbstractItemView::MultiSelection);

  // 顶部横向标签导航（对齐新原型：替代原左侧垂直导航）
  ui->navList->setFlow(QListView::LeftToRight);
  ui->navList->setWrapping(false);
  ui->navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->navList->setSpacing(4);
  ui->navList->setContentsMargins(0, 0, 0, 0);

  // 设置页左侧分类导航（软件/高级串口/插件）默认选中"软件设置"
  ui->settingsNavList->setCurrentRow(0);
  ui->settingsStack->setCurrentIndex(0);

  // 默认切换到基础页
  ui->navList->setCurrentRow(0);
  on_navList_currentRowChanged(0);

  // 初始化设置页（主题/语言/串口内联控件）
  initSettingsPage();
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

void serial::setupConnections() {
  // 导航：左侧列表 -> 右侧堆叠页
  connect(ui->navList, &QListWidget::currentRowChanged,
          this, &serial::on_navList_currentRowChanged);

  // 设置页左侧分类导航：切换右侧内容面板
  connect(ui->settingsNavList, &QListWidget::currentRowChanged, this, [this](int row){
      if (row < 0 || row >= ui->settingsStack->count()) return;
      ui->settingsStack->setCurrentIndex(row);
  });

  // 设置页：主题/语言下拉框 + 内联串口参数应用/重置
  connect(ui->themeCombo, &QComboBox::currentTextChanged,
          this, &serial::on_themeCombo_currentTextChanged);
  connect(ui->languageCombo, &QComboBox::currentTextChanged,
          this, &serial::on_languageCombo_currentTextChanged);
  connect(ui->applyInlineSettingsBtn, &QPushButton::clicked,
          this, &serial::on_applyInlineSettingsBtn_clicked);
  connect(ui->resetInlineSettingsBtn, &QPushButton::clicked,
          this, &serial::on_resetInlineSettingsBtn_clicked);

  // 基础页：刷新端口按钮（左侧大按钮，与右侧小刷新按钮同效）
  connect(ui->refreshPortBtn, &QPushButton::clicked,
          this, &serial::on_refreshButton_clicked);

  // 基础页：左侧数据位/校验/停止 合并单输入框("8 N 1")，编辑即写回 config
  connect(ui->dataBitsField, &QLineEdit::editingFinished, this, [this](){
      applyDataBitsFieldToConfig();
  });

  // 波特率变更同步到 config（打开串口时读取），基础页/高级设置页均支持自定义
  auto applyBaud = [this](QComboBox *cb){
    bool ok = false;
    const int v = cb->currentText().trimmed().toInt(&ok);
    if (ok && v > 0) worker_->config().baudRate = v;
  };
  connect(ui->baudComboBox, &QComboBox::currentTextChanged,
          this, [applyBaud, this](){ applyBaud(ui->baudComboBox); });
  connect(ui->advBaudCombo, &QComboBox::currentTextChanged,
          this, [applyBaud, this](){ applyBaud(ui->advBaudCombo); });

  // 高级设置页：可用串口下拉选择 -> 同步到基础页端口与 config
  connect(ui->advPortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int){
    const QString p = ui->advPortCombo->currentText();
    if (p.isEmpty()) return;
    const int idx = ui->portComboBox->findText(p);
    QSignalBlocker b(ui->portComboBox);
    if (idx >= 0) ui->portComboBox->setCurrentIndex(idx);
    worker_->config().name = p.toStdString();
  });

  // 高级设置页：缓冲区策略选择（打开中禁切，见 updateConnectionStatus）
  connect(ui->bufferStrategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx){
    bufferStrategy_ = idx;
    updateHighSpeedInfo(idx);
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

  // 重新填充设置页下拉框（主题/语言文本随语言变化）
  initSettingsPage();

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
  // 同步设置页"主题"下拉框
  int idx = ui->themeCombo->findData(themeName);
  if (idx >= 0 && ui->themeCombo->currentIndex() != idx)
      ui->themeCombo->setCurrentIndex(idx);
  // 重新渲染帧布局预览（颜色随主题变化）
  renderProtoLayoutPreview();
}

void serial::onLanguageChanged(const QString &languageCode) {
  // 同步设置页"语言"下拉框
  int idx = ui->languageCombo->findData(languageCode);
  if (idx >= 0 && ui->languageCombo->currentIndex() != idx)
      ui->languageCombo->setCurrentIndex(idx);
}

void serial::loadStyleSheet() {
  // Now handled by ThemeManager
}

// ============================================================
// 导航
// ============================================================
void serial::on_navList_currentRowChanged(int row) {
  if (row < 0 || row >= ui->stackedWidget->count()) return;
  ui->stackedWidget->setCurrentIndex(row);

  // 切到可视化页时刷新数据源列表
  if (row == 3) refreshVizSources();

  // 切到基础页时，把基础页波特率显示同步为 config 当前值（含高级设置页自定义值）
  if (row == 0) {
    QSignalBlocker b(ui->baudComboBox);
    ui->baudComboBox->setCurrentText(QString::number(worker_->config().baudRate));
  }

  // 切到设置页时初始化（加载主题/语言/串口参数到控件）
  if (row == 4) initSettingsPage();
}

// ============================================================
// 基础页：端口/收发
// ============================================================
void serial::refreshPortList() {
  QStringList serialStrList = worker_->scanPorts();
  QSignalBlocker blocker(ui->advPortCombo);
  ui->portComboBox->clear();
  ui->advPortCombo->clear();
  for (int i = 0; i < serialStrList.size(); i++) {
    ui->portComboBox->addItem(serialStrList[i]);
    ui->advPortCombo->addItem(serialStrList[i]);
  }
  if (serialStrList.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    ui->portComboBox->setPlaceholderText(tr("No ports found"));
    ui->advPortCombo->setPlaceholderText(tr("No ports found"));
#endif
  }
}

void serial::updateConnectionStatus(bool connected) {
  if (connected) {
    ui->statusIndicator->setStyleSheet("background-color: #2ea043; border-radius: 5px;");
    ui->connectionStatusLabel->setText(tr("Connected"));
    QString portInfo = QString("%1 @ %2 baud")
        .arg(ui->portComboBox->currentText())
        .arg(ui->baudComboBox->currentText());
    ui->portInfoLabel->setText(portInfo);

    ui->portComboBox->setEnabled(false);
    ui->baudComboBox->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->refreshPortBtn->setEnabled(false);
    ui->advancedSettingsBtn->setEnabled(false);
    ui->dataBitsField->setEnabled(false);

    // 打开期间：高级串口参数与缓冲区策略不可变更（不支持动态切换）
    ui->advPortCombo->setEnabled(false);
    ui->advBaudCombo->setEnabled(false);
    ui->dataBitsCombo->setEnabled(false);
    ui->stopBitsCombo->setEnabled(false);
    ui->parityCombo->setEnabled(false);
    ui->flowCtrlCombo->setEnabled(false);
    ui->bufferStrategyCombo->setEnabled(false);

    ui->openPortButton->setText(tr("Close Port"));

    // 启动 RX 速率计时器
    rx_rate_bytes_ = 0;
    rx_rate_window_start_ = QDateTime::currentMSecsSinceEpoch();
    rx_rate_timer_->start();

    // 终端页状态
    ui->termStatusLabel->setText(tr("● Connected"));
    ui->termStatusLabel->setStyleSheet("color: #2ea043;");
  } else {
    ui->statusIndicator->setStyleSheet("background-color: #d1242f; border-radius: 5px;");
    ui->connectionStatusLabel->setText(tr("Disconnected"));
    ui->portInfoLabel->setText(tr("No port selected"));

    ui->portComboBox->setEnabled(true);
    ui->baudComboBox->setEnabled(true);
    ui->refreshButton->setEnabled(true);
    ui->refreshPortBtn->setEnabled(true);
    ui->advancedSettingsBtn->setEnabled(true);
    ui->dataBitsField->setEnabled(true);

    // 关闭后恢复高级串口参数与缓冲区策略可编辑
    ui->advPortCombo->setEnabled(true);
    ui->advBaudCombo->setEnabled(true);
    ui->dataBitsCombo->setEnabled(true);
    ui->stopBitsCombo->setEnabled(true);
    ui->parityCombo->setEnabled(true);
    ui->flowCtrlCombo->setEnabled(true);
    ui->bufferStrategyCombo->setEnabled(true);

    ui->openPortButton->setText(tr("Open Port"));

    // 停止 RX 速率计时器
    rx_rate_timer_->stop();
    ui->rxRateValueLabel->setText("0 B/s");

    // 关闭串口后重置收发计数（下次打开重新累计）
    rx_quantity_ = 0;
    tx_quantity_ = 0;
    ui->rxCountLabel->setText("0");
    ui->txCountLabel->setText("0");

    // 终端页状态
    ui->termStatusLabel->setText(tr("● Disconnected"));
    ui->termStatusLabel->setStyleSheet("color: #d1242f;");

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

void serial::appendRecv(const QString &text) {
  appendReceiveData(text);
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
    // 应用所选缓冲区策略（串口打开前生效，打开中不支持动态切换）
    worker_->setBufferStrategy(bufferStrategy_);

    sd::PortConfig cfg = worker_->config();
    cfg.name = ui->portComboBox->currentText().toStdString();
    // 波特率已由基础页/高级设置页共同写回 config，打开时直接采用（支持自定义）
    if (worker_->open(cfg)) {
      is_the_serial_port_open_ = true;
      updateConnectionStatus(true);
      appendReceiveData(tr("[System] Port opened successfully: %1").arg(ui->portComboBox->currentText()));
    } else {
      appendReceiveData(tr("[System] Failed to open port: %1").arg(ui->portComboBox->currentText()));
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

  // 累加 RX 速率
  rx_rate_bytes_ += data.length();
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
  // 高级串口设置已内联到「设置」页：切换到设置页，并选中「高级串口设置」tab（索引1）
  ui->navList->setCurrentRow(4);          // 设置页
  ui->settingsNavList->setCurrentRow(1);  // 高级串口设置 tab（触发 settingsStack 切换到 pageSerial）
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

// ============================================================
// 终端页
// ============================================================
void serial::on_termSendButton_clicked() {
  onTermInputReturnPressed();
}

void serial::onTermInputReturnPressed() {
  QString cmd = ui->termInput->text();
  ui->termInput->clear();
  if (cmd.isEmpty()) return;

  // 命令历史
  cmdHistory_.append(cmd);
  cmdHistoryPos_ = -1;

  // 本地回显
  if (ui->termLocalEchoCheckBox->isChecked()) {
    ui->termOutput->appendPlainText(QString("%1$ %2").arg(ui->termPromptLabel->text(), cmd));
  }

  QByteArray bytes = cmd.toLatin1();
  if (ui->termCrlfCheckBox->isChecked()) {
    bytes += "\r\n";
  } else {
    bytes += "\n";
  }
  if (is_the_serial_port_open_) {
    worker_->send(bytes);
  } else {
    ui->termOutput->appendPlainText(tr("[Terminal] Port not open"));
  }
}

void serial::on_termLocalEchoCheckBox_stateChanged(int) {
  // 本地回显由发送时判断，无需额外处理
}

// ============================================================
// 协议解析页
// ============================================================
sd::FieldType serial::comboToFieldType(const QString &txt) const {
  if (txt == "uint8")  return sd::FieldType::U8;
  if (txt == "uint16") return sd::FieldType::U16;
  if (txt == "uint32") return sd::FieldType::U32;
  if (txt == "int8")   return sd::FieldType::I8;
  if (txt == "int16")  return sd::FieldType::I16;
  if (txt == "int32")  return sd::FieldType::I32;
  if (txt == "float")  return sd::FieldType::F32;
  if (txt == "double") return sd::FieldType::F64;
  if (txt == "bool")   return sd::FieldType::Bool;
  return sd::FieldType::F32;
}

QString serial::fieldTypeToCombo(sd::FieldType t) const {
  switch (t) {
    case sd::FieldType::U8:  return "uint8";
    case sd::FieldType::U16: return "uint16";
    case sd::FieldType::U32: return "uint32";
    case sd::FieldType::I8:  return "int8";
    case sd::FieldType::I16: return "int16";
    case sd::FieldType::I32: return "int32";
    case sd::FieldType::F32: return "float";
    case sd::FieldType::F64: return "double";
    case sd::FieldType::Bool:return "bool";
  }
  return "float";
}

// ============================================================
// 帧布局预览控件已在 .ui 中定义（framePreviewBar / framePreviewLegend），
// 此处仅保留 renderProtoLayoutPreview() 渲染逻辑。

void serial::renderProtoLayoutPreview() {
  if (!protoLayoutBar_ || !protoLayoutLegend_) return;

  // 清空条状容器
  while (auto *item = protoLayoutBarLayout_->takeAt(0)) {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }
  // 清空图例容器
  while (auto *item = protoLayoutLegendLayout_->takeAt(0)) {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }

  // 收集字段及字节数
  QVector<int> lens;
  qint64 total = 0;
  for (const auto &fd : protoFields_) {
    int len = fd.byteLength > 0 ? fd.byteLength : sd::fieldTypeBytes(fd.type);
    lens.append(len);
    total += len;
  }

  bool isDark = (ThemeManager::instance().currentTheme() == "dark");
  QString mutedColor = isDark ? "rgba(192,202,245,.5)" : "rgba(80,80,80,.5)";
  QString legendColor = isDark ? "rgba(192,202,245,.75)" : "rgba(60,60,60,.75)";

  if (protoFields_.isEmpty() || total <= 0) {
    auto *empty = new QLabel(tr("— 空 —"), protoLayoutBar_);
    empty->setAlignment(Qt::AlignCenter);
    empty->setStyleSheet(QString("color:%1;font-size:10px;").arg(mutedColor));
    protoLayoutBarLayout_->addWidget(empty);
    return;
  }

  // 颜色板（与原型/Web 一致）
  static const QString kSegColors[] = {
      "#7aa2f7", "#9ece6a", "#e0af68", "#f7768e",
      "#bb9af7", "#2ac3de", "#73daca", "#ff9e64"};

  for (int i = 0; i < protoFields_.size(); ++i) {
    const sd::FieldDesc &fd = protoFields_.at(i);
    int len = lens.at(i);
    double pct = (double)len * 100.0 / (double)total;
    QString color = kSegColors[i % 8];

    // 分段
    auto *seg = new QFrame(protoLayoutBar_);
    seg->setFixedHeight(28);
    seg->setStyleSheet(QString("background:%1;border:none;border-radius:3px;")
                           .arg(color));
    auto *segLay = new QHBoxLayout(seg);
    segLay->setContentsMargins(2, 0, 2, 0);
    segLay->setAlignment(Qt::AlignCenter);
    auto *segLabel = new QLabel(fd.isPadding ? "pad" : QString::fromStdString(fd.name), seg);
    segLabel->setStyleSheet("color:#1a1b26;font-weight:700;font-size:8px;");
    segLabel->setAlignment(Qt::AlignCenter);
    segLay->addWidget(segLabel);
    // 设置宽度占比
    seg->setFixedHeight(28);
    seg->setMinimumWidth(8);
    protoLayoutBarLayout_->addWidget(seg, static_cast<int>(pct * 10), Qt::AlignVCenter);

    // 图例
    auto *ld = new QLabel(protoLayoutLegend_);
    QString name = fd.isPadding ? "padding" : QString::fromStdString(fd.name);
    ld->setText(QString("<span style='background:%1;'>  </span> %2 · %3B")
                    .arg(color, name)
                    .arg(len));
    ld->setStyleSheet(QString("color:%1;font-size:10px;"
                      "font-family:'JetBrains Mono',monospace;").arg(legendColor));
    protoLayoutLegendLayout_->addWidget(ld);
  }
}

void serial::addProtoRow(const sd::FieldDesc &fd) {
  int row = ui->protoTable->rowCount();
  ui->protoTable->insertRow(row);

  // 勾选列（进入字段池）
  QTableWidgetItem *sel = new QTableWidgetItem();
  sel->setCheckState(fd.isPadding ? Qt::Unchecked : Qt::Checked);
  sel->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
  ui->protoTable->setItem(row, 0, sel);

  // 名称
  QTableWidgetItem *name = new QTableWidgetItem(QString::fromStdString(fd.name));
  ui->protoTable->setItem(row, 1, name);

  // 类型
  QTableWidgetItem *type = new QTableWidgetItem(
      fd.isPadding ? "padding" : fieldTypeToCombo(fd.type));
  ui->protoTable->setItem(row, 2, type);

  // 长度
  int len = fd.byteLength > 0 ? fd.byteLength : sd::fieldTypeBytes(fd.type);
  QTableWidgetItem *ln = new QTableWidgetItem(QString("%1B").arg(len));
  ui->protoTable->setItem(row, 3, ln);
  renderProtoLayoutPreview();
}

void serial::rebuildProtoTable(const std::vector<sd::FieldDesc> &fields) {
  ui->protoTable->setRowCount(0);
  for (const auto &f : fields) addProtoRow(f);
}

void serial::on_addFieldButton_clicked() {
  sd::FieldDesc fd;
  fd.name = QString("field%1").arg(ui->protoTable->rowCount() + 1).toStdString();
  fd.type = sd::FieldType::F32;
  fd.byteLength = -1; // 自动
  protoFields_.append(fd);
  addProtoRow(fd);
}

void serial::on_removeFieldButton_clicked() {
  int row = ui->protoTable->currentRow();
  if (row < 0) return;
  ui->protoTable->removeRow(row);
  if (row < protoFields_.size()) protoFields_.remove(row);
  renderProtoLayoutPreview();
}

void serial::on_protoTable_itemChanged(QTableWidgetItem *item) {
  if (!item) return;
  int row = item->row();
  int col = item->column();
  if (row < 0 || row >= protoFields_.size()) return;

  // 同步表格编辑回 protoFields_
  if (col == 1) {
    // 名称
    protoFields_[row].name = item->text().toStdString();
  } else if (col == 2) {
    // 类型
    QString typeTxt = item->text();
    protoFields_[row].isPadding = (typeTxt == "padding");
    if (!protoFields_[row].isPadding)
      protoFields_[row].type = comboToFieldType(typeTxt);
  } else if (col == 3) {
    // 长度（从 "4B" 格式解析）
    QString lenTxt = item->text();
    lenTxt.remove('B');
    bool ok = false;
    int len = lenTxt.toInt(&ok);
    if (ok && len > 0) protoFields_[row].byteLength = len;
  }
  renderProtoLayoutPreview();
}

void serial::collectSchemaFromTable() {
  protoFields_.clear();
  for (int r = 0; r < ui->protoTable->rowCount(); ++r) {
    sd::FieldDesc fd;
    fd.name = ui->protoTable->item(r, 1)->text().toStdString();
    QString typeTxt = ui->protoTable->item(r, 2)->text();
    fd.isPadding = (typeTxt == "padding");
    if (!fd.isPadding) fd.type = comboToFieldType(typeTxt);
    // 长度从表格文本解析（去掉 "B"）
    QString lenTxt = ui->protoTable->item(r, 3)->text();
    lenTxt.chop(1);
    bool ok = false;
    int len = lenTxt.toInt(&ok);
    int autoLen = fd.isPadding ? 1 : sd::fieldTypeBytes(fd.type);
    fd.byteLength = (ok && len > 0) ? len : autoLen;
    protoFields_.append(fd);
  }
}

void serial::on_applySchemaButton_clicked() {
  collectSchemaFromTable();

  sd::ProtocolSchema schema;
  schema.name = ui->protoNameEdit->text().toStdString();
  schema.defaultBigEndian = (ui->protoEndianCombo->currentText() == "大端");
  schema.fields.clear();
  for (const auto &fd : protoFields_) {
    schema.fields.push_back(fd);
  }

  if (schema.fields.empty()) {
    QMessageBox::warning(this, tr("Protocol"), tr("No fields defined yet."));
    return;
  }

  if (worker_->applyProtocolSchema(schema)) {
    appendReceiveData(tr("[Protocol] Applied schema \"%1\" (%2 fields, %3 bytes/frame)")
        .arg(QString::fromStdString(schema.name))
        .arg(schema.fields.size())
        .arg(schema.totalBytes()));
    refreshVizSources();

    // 协议变更后校验已有视图绑定，清理失效字段
    auto invalid = viewManager_.validateFields(worker_->fieldPool());
    if (!invalid.empty()) {
      QStringList names;
      for (const auto &n : invalid) names << QString::fromStdString(n);
      appendReceiveData(tr("[Protocol] Warning: %1 bound field(s) no longer exist: %2")
          .arg(names.size()).arg(names.join(", ")));
    }
  } else {
    appendReceiveData(tr("[Protocol] Failed to apply schema"));
  }
}

// ============================================================
// 可视化页
// ============================================================
void serial::refreshVizSources() {
  ui->vizSourceList->clear();
  ui->vizProtoLabel->setText(tr("数据源来自协议: %1")
      .arg(QString::fromStdString(worker_->fieldPool().protocolName())));

  const auto &pool = worker_->fieldPool();
  if (!pool.hasSchema()) {
    ui->vizSourceHint->setText(tr("尚未配置协议，请先到「协议解析」页应用协议"));
    return;
  }
  ui->vizSourceHint->setText(tr("勾选数据源 + 选类型 → 新建视图"));

  for (const auto &s : pool.sources()) {
    QListWidgetItem *item = new QListWidgetItem(
        QString("%1  [%2]").arg(QString::fromStdString(s.name),
                                QString::fromStdString(s.unit)));
    item->setData(Qt::UserRole, QString::fromStdString(s.name));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    item->setCheckState(Qt::Unchecked);
    ui->vizSourceList->addItem(item);
  }
}

void serial::on_vizTypeCombo_currentIndexChanged(int index) {
  // 通过服务层 ViewManager 管理视图类型（互斥切换）
  QString typeName = ui->vizTypeCombo->itemText(index);
  if (typeName == tr("波形图")) {
    viewManager_.setType(sd::ViewType::Wave);
  } else if (typeName == tr("数据表格")) {
    viewManager_.setType(sd::ViewType::Table);
  } else {
    viewManager_.setType(sd::ViewType::None);
  }
}

void serial::addVizViewCard(const ViewInstanceUi &vi) {
  // 卡片框架
  QFrame *card = new QFrame();
  card->setFrameShape(QFrame::StyledPanel);
  card->setObjectName("vizCard");
  card->setProperty("viewId", vi.viewId);

  QVBoxLayout *v = new QVBoxLayout(card);
  v->setContentsMargins(8, 8, 8, 8);
  v->setSpacing(6);

  // 标题行：类型 + 字段 + 关闭按钮
  QWidget *head = new QWidget();
  QHBoxLayout *hh = new QHBoxLayout(head);
  hh->setContentsMargins(0, 0, 0, 0);
  QLabel *title = new QLabel(QString("%1 · %2").arg(vi.title, vi.typeName));
  title->setStyleSheet("font-weight: bold; color: #7aa2f7;");
  QLabel *fieldsLabel = new QLabel(vi.fields.join(" + "));
  fieldsLabel->setStyleSheet("color: #8090a0;");
  hh->addWidget(title);
  hh->addWidget(fieldsLabel);
  hh->addStretch();
  QPushButton *closeBtn = new QPushButton("✕");
  closeBtn->setFixedSize(22, 22);
  closeBtn->setCursor(Qt::PointingHandCursor);
  connect(closeBtn, &QPushButton::clicked, this, [this, vi]() {
      removeVizView(vi.viewId);
  });
  hh->addWidget(closeBtn);
  v->addWidget(head);

  // 绘制区
  VizPlotWidget *plot = new VizPlotWidget(vi.typeName, vi.fields);
  // 用动态属性标记本控件类型：VizPlotWidget 是局部类、无 Q_OBJECT，
  // 新版 Qt 不允许对其 findChildren<custom*>()/qobject_cast，故用属性标记 + static_cast。
  plot->setProperty("sdVizPlot", true);
  plot->pushEmpty(); // 初始演示曲线
  plot->setMinimumHeight(140);
  v->addWidget(plot);

  ui->vizCanvasContainer->layout()->addWidget(card);
}

void serial::rebuildVizViews() {
  // 清空容器
  QLayout *lay = ui->vizCanvasContainer->layout();
  while (QLayoutItem *it = lay->takeAt(0)) {
    if (QWidget *w = it->widget()) w->deleteLater();
    delete it;
  }
  for (const auto &vi : vizViews_) addVizViewCard(vi);
}

void serial::on_addViewButton_clicked() {
  // 收集勾选的数据源
  QStringList selected;
  for (int i = 0; i < ui->vizSourceList->count(); ++i) {
    QListWidgetItem *item = ui->vizSourceList->item(i);
    if (item->checkState() == Qt::Checked) {
      selected << item->data(Qt::UserRole).toString();
    }
  }
  if (selected.isEmpty()) {
    QMessageBox::information(this, tr("Visualization"), tr("请先在左侧勾选至少一个数据源"));
    return;
  }

  // 确保视图类型已设置（首次添加时自动设置）
  if (!viewManager_.configured()) {
    viewManager_.setType(sd::ViewType::Wave);
  }

  QString typeName = ui->vizTypeCombo->currentText();

  // 通过服务层 ViewManager 创建视图绑定
  std::vector<std::string> fieldVec;
  for (const auto &s : selected) fieldVec.push_back(s.toStdString());
  std::string viewId = viewManager_.addView(fieldVec, typeName.toStdString());

  ViewInstanceUi vi;
  vi.viewId = QString::fromStdString(viewId);
  vi.typeName = typeName;
  vi.title = typeName;
  vi.fields = selected;
  vizViews_.append(vi);

  addVizViewCard(vi);
}

void serial::removeVizView(const QString &viewId) {
  for (int i = 0; i < vizViews_.size(); ++i) {
    if (vizViews_.at(i).viewId == viewId) {
      vizViews_.remove(i);
      break;
    }
  }
  // 同步移除服务层 ViewManager 中的绑定
  viewManager_.removeView(viewId.toStdString());
  rebuildVizViews();
}

void serial::onFrameReceived(const sd::Frame &frame) {
  // 1) 把帧数据推给所有可视化视图卡片
  QLayout *lay = ui->vizCanvasContainer->layout();
  for (int i = 0; i < lay->count(); ++i) {
    QWidget *w = lay->itemAt(i)->widget();
    if (QFrame *card = qobject_cast<QFrame*>(w)) {
      const auto kidWidgets = card->findChildren<QWidget*>();
      for (QWidget *kid : kidWidgets)
        if (kid->property("sdVizPlot").toBool())
          static_cast<VizPlotWidget*>(kid)->pushSample(frame);
    }
  }

  // 2) 更新协议解析页的实时解析数据显示
  if (ui->parsedDataView) {
    bool isDark = (ThemeManager::instance().currentTheme() == "dark");
    QString valColor = isDark ? "#c0caf5" : "#333333";
    QString html = QString("<div style='color:#7aa2f7;font-weight:bold;'>"
                           "帧 #%1 [%2] · %3 字段</div><br>")
                        .arg(frame.seq)
                        .arg(QString::fromStdString(frame.name))
                        .arg(frame.numeric.size() + frame.text.size());
    for (const auto &kv : frame.numeric) {
      html += QString("<div style='color:#9ece6a;'>%1</div>"
                      "<div style='color:%2;margin-left:12px;'>= %3</div>")
                  .arg(QString::fromStdString(kv.first))
                  .arg(valColor)
                  .arg(kv.second, 0, 'g', 6);
    }
    for (const auto &kv : frame.text) {
      html += QString("<div style='color:#e0af68;'>%1</div>"
                      "<div style='color:%2;margin-left:12px;'>= %3</div>")
                  .arg(QString::fromStdString(kv.first))
                  .arg(valColor)
                  .arg(QString::fromStdString(kv.second));
    }
    ui->parsedDataView->setHtml(html);
  }
}

// ============================================================
// 设置页
// ============================================================
void serial::initSettingsPage() {
  // 主题下拉框（文本 -> ThemeManager 主题名）
  // 阻塞信号：clear()/addItem() 会触发 currentTextChanged，
  // 若不阻塞，第一项"跟随系统"被自动选中会调 applyTheme("system")
  // 从而清空样式表，导致主题被意外重置。
  {
    QSignalBlocker blocker(ui->themeCombo);
    ui->themeCombo->clear();
    ui->themeCombo->addItem(tr("深色"),    QStringLiteral("dark"));
    ui->themeCombo->addItem(tr("浅色"),    QStringLiteral("light"));
    int themeIdx = ui->themeCombo->findData(ThemeManager::instance().currentTheme());
    if (themeIdx >= 0) ui->themeCombo->setCurrentIndex(themeIdx);
  }

  // 语言下拉框（同理阻塞信号）
  {
    QSignalBlocker blocker(ui->languageCombo);
    ui->languageCombo->clear();
    ui->languageCombo->addItem(tr("中文"),    QStringLiteral("zh_CN"));
    ui->languageCombo->addItem(tr("English"), QStringLiteral("en"));
    int langIdx = ui->languageCombo->findData(LanguageManager::instance().currentLanguage());
    if (langIdx >= 0) ui->languageCombo->setCurrentIndex(langIdx);
  }

  // 定时发送间隔：初始取基础页定时输入框当前值
  bool ok = false;
  int interval = ui->timerIntervalEdit->text().toInt(&ok);
  if (ok && interval > 0) ui->timerIntervalSpin->setValue(interval);

  // 串口内联参数：从 worker_->config() 加载
  const sd::PortConfig &cfg = worker_->config();

  int di = ui->dataBitsCombo->findText(QString::number(cfg.dataBits));
  if (di >= 0) ui->dataBitsCombo->setCurrentIndex(di);

  int si = ui->stopBitsCombo->findText(QString::number(cfg.stopBits));
  if (si >= 0) ui->stopBitsCombo->setCurrentIndex(si);

  if (cfg.parity >= 0 && cfg.parity < ui->parityCombo->count())
    ui->parityCombo->setCurrentIndex(cfg.parity);

  if (cfg.flowControl >= 0 && cfg.flowControl < ui->flowCtrlCombo->count())
    ui->flowCtrlCombo->setCurrentIndex(cfg.flowControl);

  // 基础页波特率：同步 config（支持自定义值）
  {
    QSignalBlocker b(ui->baudComboBox);
    ui->baudComboBox->setCurrentText(QString::number(cfg.baudRate));
  }

  // 高级设置页串口下拉：同步到已枚举列表
  {
    QSignalBlocker b(ui->advPortCombo);
    const int pi = ui->advPortCombo->findText(QString::fromStdString(cfg.name));
    if (pi >= 0) ui->advPortCombo->setCurrentIndex(pi);
  }

  // 高级设置页波特率
  {
    QSignalBlocker b(ui->advBaudCombo);
    ui->advBaudCombo->setCurrentText(QString::number(cfg.baudRate));
  }

  // 高级设置页缓冲区策略 + 高速设置区信息
  {
    QSignalBlocker b(ui->bufferStrategyCombo);
    if (bufferStrategy_ >= 0 && bufferStrategy_ < ui->bufferStrategyCombo->count())
      ui->bufferStrategyCombo->setCurrentIndex(bufferStrategy_);
  }
  updateHighSpeedInfo(bufferStrategy_);
}

void serial::on_themeCombo_currentTextChanged(const QString &t) {
  QString theme;
  if (t == tr("深色"))     theme = "dark";
  else if (t == tr("浅色")) theme = "light";
  else theme = t;
  ThemeManager::instance().applyTheme(theme);
}

void serial::on_languageCombo_currentTextChanged(const QString &l) {
  // 界面文本 -> LanguageManager 语言码：中文→zh_CN, English→en
  QString code;
  if (l == tr("中文"))     code = "zh_CN";
  else if (l == tr("English")) code = "en";
  else code = l;
  LanguageManager::instance().setLanguage(code);
}

void serial::on_applyInlineSettingsBtn_clicked() {
  // 把内联控件值写回 worker_->config()
  sd::PortConfig &cfg = worker_->config();
  cfg.dataBits    = ui->dataBitsCombo->currentText().toInt();
  cfg.stopBits    = ui->stopBitsCombo->currentText().toInt();
  cfg.parity      = ui->parityCombo->currentIndex();      // 0=No,1=Even,2=Odd,3=Space,4=Mark
  cfg.flowControl = ui->flowCtrlCombo->currentIndex();    // 0=No,1=Hardware,2=Software

  // 同步到基础页左侧下拉
  syncSerialSettingsSettingsPageToLeft();

  appendReceiveData(tr("[System] Serial settings applied: %1 data bits, %2 stop bits, parity=%3, flow=%4")
      .arg(cfg.dataBits)
      .arg(cfg.stopBits)
      .arg(ui->parityCombo->currentText())
      .arg(ui->flowCtrlCombo->currentText()));
}

void serial::on_resetInlineSettingsBtn_clicked() {
  // 恢复默认：8 数据位 / 1 停止位 / None 校验 / None 流控
  sd::PortConfig &cfg = worker_->config();
  cfg.dataBits    = 8;
  cfg.stopBits    = 1;
  cfg.parity      = 0;   // None
  cfg.flowControl = 0;   // None

  // 同步内联控件
  ui->dataBitsCombo->setCurrentText(QString::number(cfg.dataBits));
  ui->stopBitsCombo->setCurrentText(QString::number(cfg.stopBits));
  ui->parityCombo->setCurrentIndex(cfg.parity);
  ui->flowCtrlCombo->setCurrentIndex(cfg.flowControl);

  // 同步到基础页左侧下拉
  syncSerialSettingsSettingsPageToLeft();

  appendReceiveData(tr("[System] Serial settings reset to defaults (8 1 None None)"));
}

void serial::syncSerialSettingsLeftToSettingsPage() {
  // 左侧 -> 设置页内联
  const sd::PortConfig &cfg = worker_->config();
  int di = ui->dataBitsCombo->findText(QString::number(cfg.dataBits));
  if (di >= 0) ui->dataBitsCombo->setCurrentIndex(di);
  ui->parityCombo->setCurrentIndex(cfg.parity);
  int si = ui->stopBitsCombo->findText(QString::number(cfg.stopBits));
  if (si >= 0) ui->stopBitsCombo->setCurrentIndex(si);
}

void serial::syncSerialSettingsSettingsPageToLeft() {
  // 设置页内联 -> 基础页左侧（合并为 "8 N 1" 形式）
  const sd::PortConfig &cfg = worker_->config();
  QSignalBlocker bf(ui->dataBitsField);
  ui->dataBitsField->setText(QString("%1 %2 %3")
      .arg(QString::number(cfg.dataBits))
      .arg(parityShortName(cfg.parity))
      .arg(QString::number(cfg.stopBits, 'g', 2)));
}

void serial::applyDataBitsFieldToConfig() {
  // 解析 "8 N 1" -> dataBits/parity/stopBits，写回 config
  const QString txt = ui->dataBitsField->text().trimmed();
  const QStringList parts = txt.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.size() < 3) return;
  bool ok = false;
  const int db = parts[0].toInt(&ok);
  if (!ok || db < 5 || db > 8) return;
  const QString p = parts[1].toUpper();
  int parity = 0;
  if (p == QLatin1String("E"))      parity = 1;
  else if (p == QLatin1String("O")) parity = 2;
  else if (p == QLatin1String("S")) parity = 3;
  else if (p == QLatin1String("M")) parity = 4;
  double sb = parts[2].toDouble(&ok);
  if (!ok) return;
  if (sb != 1.0 && sb != 1.5 && sb != 2.0) return;

  sd::PortConfig &cfg = worker_->config();
  cfg.dataBits = db;
  cfg.parity   = parity;
  cfg.stopBits = (int)sb;

  // 回写规范化文本
  QSignalBlocker bf(ui->dataBitsField);
  ui->dataBitsField->setText(QString("%1 %2 %3")
      .arg(db).arg(parityShortName(parity)).arg(QString::number(sb, 'g', 2)));
}

void serial::updateHighSpeedInfo(int strategyIdx) {
  // 最大数据消化能力（估算值，随缓冲策略变化）与缓冲策略说明
  static const char *kCaps[3] = { "约 210 MB/s", "约 180 MB/s", "约 320 MB/s" };
  static const char *kHints[3] = {
      "环形缓冲：固定容量、内存恒定，满则丢弃最旧数据，适合持续高速数据流（如波形采样、持续监控）。",
      "双缓冲：生产者写后备区、消费者读前区，适合跨线程读写（串口线程生产 + UI 线程消费），降低覆盖风险。",
      "追加缓冲：累积完整历史并在超限时裁剪最旧数据，适合需要完整接收日志且带内存上限保护的场景。",
  };
  if (strategyIdx < 0 || strategyIdx > 2) strategyIdx = 0;
  ui->throughputCapLabel->setText(QString::fromUtf8(kCaps[strategyIdx]));
  ui->bufferStrategyHint->setText(QString::fromUtf8(kHints[strategyIdx]));
}