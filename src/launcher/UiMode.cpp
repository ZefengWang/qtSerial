#include "UiMode.hpp"

#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QDebug>

namespace ui_mode {

namespace {
constexpr const char* kSettingKey = "uiMode";
}

bool qmlAvailable() {
#ifdef HAVE_QML
    return true;
#else
    return false;
#endif
}

QString toString(UiMode m) {
    switch (m) {
        case UiMode::Auto:       return QStringLiteral("auto");
        case UiMode::QtWidgets:  return QStringLiteral("qt");
        case UiMode::TUI:        return QStringLiteral("tui");
        case UiMode::Web:        return QStringLiteral("web");
        case UiMode::QML:        return QStringLiteral("qml");
    }
    return QStringLiteral("auto");
}

UiMode fromString(const QString& s) {
    QString v = s.toLower();
    if (v == QLatin1String("qt") || v == QLatin1String("widgets") || v == QLatin1String("qtwidgets"))
        return UiMode::QtWidgets;
    if (v == QLatin1String("tui"))
        return UiMode::TUI;
    if (v == QLatin1String("web"))
        return UiMode::Web;
    if (v == QLatin1String("qml"))
        return UiMode::QML;
    return UiMode::Auto;
}

bool hasGraphicsEnvironment() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return true;
#else
    // 显式指定了平台插件（offscreen/xcb/wayland/vnc 等）——这些都能创建 QApplication。
    const QString platform = QString::fromLocal8Bit(qgetenv("QT_QPA_PLATFORM")).toLower();
    if (!platform.isEmpty())
        return true;
    // 有 DISPLAY 或 Wayland 会话。
    if (qEnvironmentVariableIsSet("DISPLAY") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
        return true;
    // 完全无图形信标：视为无头，默认 TUI。
    return false;
#endif
}

UiMode parseUiArg(int argc, char* argv[], QStringList* rest) {
    UiMode mode = UiMode::Auto;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a.startsWith(QStringLiteral("--ui="))) {
            mode = fromString(a.mid(5));
        } else if (a == QLatin1String("--ui") && i + 1 < argc) {
            mode = fromString(QString::fromLocal8Bit(argv[++i]));
        } else if (rest) {
            *rest << a;
        }
    }
    return mode;
}

UiMode savedMode() {
    QSettings s;
    const QString v = s.value(QString::fromLatin1(kSettingKey)).toString();
    return fromString(v);
}

void saveMode(UiMode m) {
    QSettings s;
    s.setValue(QString::fromLatin1(kSettingKey), toString(m));
    s.sync();
}

void clearSavedMode() {
    QSettings s;
    s.remove(QString::fromLatin1(kSettingKey));
    s.sync();
}

void restartWithMode(UiMode m, const QStringList& args) {
    QStringList newArgs;
    // 保留传入参数（如 --port），但去掉已有的 --ui 项避免重复。
    for (const QString& a : args)
        if (!a.startsWith(QStringLiteral("--ui")) && a != QLatin1String("--ui"))
            newArgs << a;
    newArgs << QStringLiteral("--ui=") + toString(m);

    const QString app = QCoreApplication::applicationFilePath();
    qInfo().noquote() << "Restarting as" << toString(m) << "->" << app << newArgs.join(QLatin1Char(' '));
    QProcess::startDetached(app, newArgs);
    QCoreApplication::quit();
}

QString displayName(UiMode m) {
    switch (m) {
        case UiMode::Auto:       return QStringLiteral("自动");
        case UiMode::QtWidgets:  return QStringLiteral("Qt Widgets 桌面");
        case UiMode::TUI:        return QStringLiteral("终端(TUI)");
        case UiMode::Web:        return QStringLiteral("浏览器(Web)");
        case UiMode::QML:        return QStringLiteral("QML 界面");
    }
    return QStringLiteral("自动");
}

QStringList modeNames(bool includeQml) {
    QStringList names;
    names << QStringLiteral("qt")
          << QStringLiteral("tui")
          << QStringLiteral("web");
    if (includeQml) names << QStringLiteral("qml");
    return names;
}

} // namespace ui_mode