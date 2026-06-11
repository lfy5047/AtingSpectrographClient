#include "ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QSettings>

namespace {

const char* kThemeSettingsKey = "ui/theme";

ThemeManager::Theme& currentThemeStorage()
{
    static ThemeManager::Theme theme = ThemeManager::Theme::IndustrialDark;
    return theme;
}

const QVector<ThemeManager::ThemeInfo>& themeCatalog()
{
    static const QVector<ThemeManager::ThemeInfo> themes = {
        { ThemeManager::Theme::IndustrialDark,
          QStringLiteral("industrial"),
          QString::fromUtf8("深色"),
          QStringLiteral(":/style/industrial.qss") },
        { ThemeManager::Theme::OutdoorLight,
          QStringLiteral("outdoor_light"),
          QString::fromUtf8("户外亮色"),
          QStringLiteral(":/style/outdoor_light.qss") }
    };
    return themes;
}

const ThemeManager::ThemeInfo& infoForTheme(ThemeManager::Theme theme)
{
    const QVector<ThemeManager::ThemeInfo>& themes = themeCatalog();
    for (const ThemeManager::ThemeInfo& info : themes) {
        if (info.theme == theme) return info;
    }
    return themes.front();
}

} // namespace

QVector<ThemeManager::ThemeInfo> ThemeManager::availableThemes()
{
    return themeCatalog();
}

ThemeManager::Theme ThemeManager::themeFromId(const QString& id)
{
    const QVector<ThemeInfo>& themes = themeCatalog();
    for (const ThemeInfo& info : themes) {
        if (info.id == id) return info.theme;
    }
    return Theme::IndustrialDark;
}

QString ThemeManager::id(Theme theme)
{
    return infoForTheme(theme).id;
}

QString ThemeManager::displayName(Theme theme)
{
    return infoForTheme(theme).displayName;
}

QString ThemeManager::resourcePath(Theme theme)
{
    return infoForTheme(theme).resourcePath;
}

ThemeManager::Theme ThemeManager::loadSavedTheme()
{
    QSettings settings;
    const Theme theme = themeFromId(settings.value(QString::fromLatin1(kThemeSettingsKey),
                                                   id(Theme::IndustrialDark)).toString());
    currentThemeStorage() = theme;
    return theme;
}

void ThemeManager::saveTheme(Theme theme)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kThemeSettingsKey), id(theme));
}

ThemeManager::Theme ThemeManager::currentTheme()
{
    return currentThemeStorage();
}

bool ThemeManager::applyTheme(QApplication& app, Theme theme, QString* errorMessage)
{
    QFile qss(resourcePath(theme));
    if (!qss.open(QFile::ReadOnly | QFile::Text)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("无法加载主题样式: %1").arg(resourcePath(theme));
        }
        return false;
    }

    currentThemeStorage() = theme;
    saveTheme(theme);
    app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    if (errorMessage) errorMessage->clear();
    return true;
}

bool ThemeManager::applySavedTheme(QApplication& app, QString* errorMessage)
{
    return applyTheme(app, loadSavedTheme(), errorMessage);
}

void ThemeManager::setCurrentThemeForTesting(Theme theme)
{
    currentThemeStorage() = theme;
}
