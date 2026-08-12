#include "UiModePicker.hpp"
#include "UiMode.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

struct ModeEntry {
    const char* id;
    const char* title;
    const char* desc;
};

const ModeEntry kEntries[] = {
    {"qt",  "Qt Widgets 桌面", "功能最全的桌面界面：串口收发、终端、协议解析、可视化一体化"},
    {"tui", "终端 (TUI)",      "无图形界面的纯文本交互，适合 SSH / 无头服务器"},
    {"web", "浏览器 (Web)",    "启动本地服务，用浏览器访问 http://localhost:8080"},
#ifdef HAVE_QML
    {"qml", "QML 界面",        "基于 Qt Quick 的现代界面，可选 GPU 加速渲染"},
#endif
};

} // namespace

UiModePicker::UiModePicker(QWidget* parent)
    : QDialog(parent),
      list_(new QListWidget(this)),
      remember_(new QCheckBox(QStringLiteral("记住我的选择，下次不再询问"), this)),
      desc_(new QLabel(this)),
      selected_() {

    setWindowTitle(QStringLiteral("选择界面模式"));
    setMinimumWidth(460);

    auto* title = new QLabel(QStringLiteral("Serial Debug Assistant · 请选择本次界面方式"), this);
    title->setStyleSheet(QStringLiteral("font-size:15px;font-weight:600;padding:2px 0;"));

    for (const ModeEntry& e : kEntries) {
        auto* item = new QListWidgetItem(e.title, list_);
        item->setData(Qt::UserRole, QString::fromLatin1(e.id));
        item->setToolTip(QString::fromUtf8(e.desc));
        item->setSizeHint(QSize(0, 40));
    }
    list_->setCurrentRow(0);

    desc_->setWordWrap(true);
    desc_->setStyleSheet(QStringLiteral("color:#888;font-size:12px;min-height:34px;"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main = new QVBoxLayout(this);
    main->addWidget(title);
    main->addWidget(list_, 1);
    main->addWidget(desc_);
    main->addWidget(remember_);
    main->addWidget(buttons);

    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            auto* item = list_->item(row);
            desc_->setText(item->toolTip());
        }
    });
    connect(list_, &QListWidget::itemActivated, this, &UiModePicker::onItemActivated);
}

QString UiModePicker::selectedMode() const {
    if (auto* item = list_->currentItem())
        return item->data(Qt::UserRole).toString();
    return QStringLiteral("qt");
}

bool UiModePicker::rememberChoice() const {
    return remember_->isChecked();
}

void UiModePicker::onItemActivated(QListWidgetItem*) {
    accept();
}