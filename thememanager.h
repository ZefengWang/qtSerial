#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Theme manager singleton for switching between QSS themes.
 *
 * Themes are loaded from embedded resources (the styles directory in res.qrc).
 * The current theme is persisted via QSettings.
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager &instance();

    /// Apply the named theme (e.g. "dark", "light", "system").
    void applyTheme(const QString &themeName);

    /// Get the list of available theme names.
    QStringList availableThemes() const;

    /// Get the currently active theme name.
    QString currentTheme() const;

    /// Get a human-readable display name for a theme.
    QString themeDisplayName(const QString &themeName) const;

signals:
    void themeChanged(const QString &themeName);

private:
    ThemeManager();
    ThemeManager(const ThemeManager &) = delete;
    ThemeManager &operator=(const ThemeManager &) = delete;

    QString m_currentTheme;
    QStringList m_themes;
};

#endif // THEMEMANAGER_H
