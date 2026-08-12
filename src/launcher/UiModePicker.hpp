#ifndef SRC_LAUNCHER_UI_MODE_PICKER_HPP
#define SRC_LAUNCHER_UI_MODE_PICKER_HPP

#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QCheckBox;
class QLabel;

// ============================================================
// UiModePicker —— 启动时的 UI 模式选择对话框。
//
// 在有图形环境且未通过 --ui 指定模式时弹出，让用户选择本次
// 启动采用哪种界面宿主。可勾选"记住选择"持久化到配置，之后
// 启动不再询问（直到用户主动修改）。
// ============================================================
class UiModePicker : public QDialog {
    Q_OBJECT

public:
    explicit UiModePicker(QWidget* parent = nullptr);

    // 返回用户选择的模式（"qt"/"tui"/"web"/"qml"）。
    QString selectedMode() const;
    // 用户是否勾选了"记住选择"。
    bool rememberChoice() const;

private slots:
    void onItemActivated(QListWidgetItem* item);

private:
    QListWidget* list_;
    QCheckBox*   remember_;
    QLabel*      desc_;
    QString      selected_;
};

#endif // SRC_LAUNCHER_UI_MODE_PICKER_HPP