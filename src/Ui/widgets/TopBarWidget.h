#pragma once

#include <QWidget>
#include <QLabel>
#include <QFrame>

#include "ThemeManager.h"

class QComboBox;

class MetricCard : public QFrame {
    Q_OBJECT
public:
    explicit MetricCard(int iconType, const QString& label, QWidget* parent = nullptr);
    void setValue(const QString& text);
    void setSub(const QString& text);
    void setOnline(bool online);
private:
    QLabel* iconLabel_  = nullptr;
    QLabel* labelLabel_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QLabel* subLabel_   = nullptr;
    int iconType_;
};

class TopBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit TopBarWidget(QWidget* parent = nullptr);

    void setTheme(ThemeManager::Theme theme);
    void setConnected(bool connected, const QString& ip = QString());
    void setFps(double fps);
    void setFrames(quint64 n);
    void setDropped(quint64 n);
    void setMirrorAngle(double deg);

signals:
    void themeChanged(ThemeManager::Theme theme);

private:
    MetricCard* connCard_  = nullptr;
    MetricCard* fpsCard_   = nullptr;
    MetricCard* frameCard_ = nullptr;
    MetricCard* dropCard_  = nullptr;
    MetricCard* angleCard_ = nullptr;
    MetricCard* ipCard_    = nullptr;
    QFrame* themeCard_     = nullptr;
    QComboBox* themeCombo_ = nullptr;
    bool updatingThemeCombo_ = false;
};
