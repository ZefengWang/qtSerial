#include "languagemanager.h"
#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QDir>
#include <QMap>

LanguageManager &LanguageManager::instance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager()
    : m_currentLanguage("en")
    , m_translator(nullptr)
{
    m_languages << "en" << "zh_CN";
}

QStringList LanguageManager::availableLanguages() const
{
    return m_languages;
}

QString LanguageManager::currentLanguage() const
{
    return m_currentLanguage;
}

QString LanguageManager::languageDisplayName(const QString &languageCode) const
{
    static QMap<QString, QString> displayNames = {
        {"en",    "English"},
        {"zh_CN", "中文"},
    };
    return displayNames.value(languageCode, languageCode);
}

void LanguageManager::initialize()
{
    // If we already have a translator, remove it
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    m_translator = new QTranslator(qApp);

    // Load saved language from settings, default to system locale
    QSettings settings;
    QString savedLang = settings.value("language").toString();
    QString langToLoad;

    if (!savedLang.isEmpty()) {
        langToLoad = savedLang;
    } else {
        // Try system locale
        QString sysLocale = QLocale::system().name(); // e.g. "zh_CN"
        if (m_languages.contains(sysLocale)) {
            langToLoad = sysLocale;
        } else {
            langToLoad = "en";
        }
    }

    setLanguage(langToLoad);
}

bool LanguageManager::setLanguage(const QString &languageCode)
{
    if (!m_languages.contains(languageCode)) {
        return false;
    }

    // "en" doesn't need translation (source language is English)
    if (languageCode == "en") {
        if (m_translator) {
            qApp->removeTranslator(m_translator);
        }
        m_currentLanguage = "en";

        QSettings settings;
        settings.setValue("language", "en");

        emit languageChanged("en");
        return true;
    }

    // Remove existing translator
    if (m_translator) {
        qApp->removeTranslator(m_translator);
    } else {
        m_translator = new QTranslator(qApp);
    }

    // Search paths for .qm file
    QString appDir = QCoreApplication::applicationDirPath();
    QString qmFile = QString("MySerial_%1.qm").arg(languageCode);
    QStringList searchPaths;
    searchPaths << appDir + "/" + qmFile
                << appDir + "/translations/" + qmFile
                << ":/translations/" + qmFile;
    // Shared install location (e.g. .deb packages install to /usr/share/serial-debug)
    searchPaths << "/usr/share/serial-debug/translations/" + qmFile
                << "/usr/local/share/serial-debug/translations/" + qmFile;

    bool loaded = false;
    for (const QString &path : searchPaths) {
        if (m_translator->load(path)) {
            qApp->installTranslator(m_translator);
            loaded = true;
            break;
        }
    }

    if (loaded) {
        m_currentLanguage = languageCode;

        QSettings settings;
        settings.setValue("language", languageCode);

        emit languageChanged(languageCode);
    }

    return loaded;
}
