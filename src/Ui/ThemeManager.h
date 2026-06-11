#pragma once

#include <QVector>
#include <QString>

class QApplication;

class ThemeManager {
public:
    enum class Theme {
        IndustrialDark,
        OutdoorLight
    };

    struct ThemeInfo {
        Theme theme;
        QString id;
        QString displayName;
        QString resourcePath;
    };

    static QVector<ThemeInfo> availableThemes();
    static Theme themeFromId(const QString& id);
    static QString id(Theme theme);
    static QString displayName(Theme theme);
    static QString resourcePath(Theme theme);

    static Theme currentTheme();
    static Theme loadSavedTheme();
    static void saveTheme(Theme theme);
    static bool applyTheme(QApplication& app, Theme theme, QString* errorMessage = nullptr);
    static bool applySavedTheme(QApplication& app, QString* errorMessage = nullptr);
    static void setCurrentThemeForTesting(Theme theme);
};
