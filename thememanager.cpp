#include "thememanager.h"
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QMap>

ThemeManager &ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager()
    : m_currentTheme("dark")
{
    // Register available themes (system removed — only dark/light)
    m_themes << "dark" << "light";
}

QStringList ThemeManager::availableThemes() const
{
    return m_themes;
}

QString ThemeManager::currentTheme() const
{
    return m_currentTheme;
}

QString ThemeManager::themeDisplayName(const QString &themeName) const
{
    static QMap<QString, QString> displayNames = {
        {"dark",   "Dark"},
        {"light",  "Light"},
        {"system", "System"},
    };
    return displayNames.value(themeName, themeName);
}

void ThemeManager::applyTheme(const QString &themeName)
{
    // 未知主题（如历史残留的 "system"）回退到深色，避免出现无样式裸 UI。
    QString effective = m_themes.contains(themeName) ? themeName : QStringLiteral("dark");
    if (effective != themeName) {
        // 修正持久化值，避免下次仍读到无效主题
        QSettings settings;
        settings.setValue("theme", effective);
    }

    // Load the QSS from embedded resource
    QString resourcePath = QString(":/styles/%1.qss").arg(effective);
    QFile styleFile(resourcePath);

    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        qApp->setStyleSheet(styleSheet);
        styleFile.close();
    }

    m_currentTheme = effective;

    // Persist the choice
    QSettings settings;
    settings.setValue("theme", effective);

    emit themeChanged(effective);
}
