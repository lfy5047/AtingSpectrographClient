#pragma once

#include <QWidget>
#include <QLabel>
#include <QFrame>

#include "ThemeManager.h"

class QComboBox;
class QHBoxLayout;
class QPushButton;

class MetricCard : public QFrame {
    Q_OBJECT
public:
    explicit MetricCard(int iconType, const QString& label, QWidget* parent = nullptr);
    void setValue(const QString& text);
    void setSub(const QString& text);
    void setOnline(bool online);
    void addHeaderAction(QWidget* widget);
private:
    QHBoxLayout* headerLayout_ = nullptr;
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
    void connectToggleRequested();

private:
    void updateFpsDropped();

    MetricCard* connCard_  = nullptr;
    MetricCard* fpsCard_   = nullptr;
    MetricCard* frameCard_ = nullptr;
    MetricCard* angleCard_ = nullptr;
    MetricCard* ipCard_    = nullptr;
    QFrame* themeCard_     = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QPushButton* connectionButton_ = nullptr;
    double currentFps_ = 0.0;
    quint64 currentDropped_ = 0;
    bool connected_ = false;
    bool updatingThemeCombo_ = false;
};
