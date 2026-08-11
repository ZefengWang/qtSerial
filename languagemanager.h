#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Language manager singleton for i18n switching.
 *
 * Loads .qm translation files from app directory or embedded resources.
 * The current language is persisted via QSettings.
 */
class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager &instance();

    /// Initialize and load the saved language (call early in main()).
    void initialize();

    /// Switch to the named language (e.g. "en", "zh_CN").
    /// Returns true if the translation was loaded successfully.
    bool setLanguage(const QString &languageCode);

    /// Get the currently active language code.
    QString currentLanguage() const;

    /// Get list of available language codes.
    QStringList availableLanguages() const;

    /// Get a human-readable display name for a language.
    QString languageDisplayName(const QString &languageCode) const;

signals:
    void languageChanged(const QString &languageCode);

private:
    LanguageManager();
    LanguageManager(const LanguageManager &) = delete;
    LanguageManager &operator=(const LanguageManager &) = delete;

    QString m_currentLanguage;
    QStringList m_languages;
    class QTranslator *m_translator;
};

#endif // LANGUAGEMANAGER_H
