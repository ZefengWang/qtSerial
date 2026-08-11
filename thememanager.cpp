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
    // Register available themes
    m_themes << "dark" << "light" << "system";
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
    if (!m_themes.contains(themeName)) {
        return;
    }

    // "system" theme uses the native platform styling (no QSS override)
    if (themeName == "system") {
        qApp->setStyleSheet(QString());
    } else {
        // Load the QSS from embedded resource
        QString resourcePath = QString(":/styles/%1.qss").arg(themeName);
        QFile styleFile(resourcePath);

        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            QString styleSheet = QString::fromUtf8(styleFile.readAll());
            qApp->setStyleSheet(styleSheet);
            styleFile.close();
        }
    }

    m_currentTheme = themeName;

    // Persist the choice
    QSettings settings;
    settings.setValue("theme", themeName);

    emit themeChanged(themeName);
}
