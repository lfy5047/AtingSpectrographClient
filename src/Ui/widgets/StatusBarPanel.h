#pragma once

#include <QWidget>
#include <QLabel>
#include "LedIndicator.h"

class StatusBarPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatusBarPanel(QWidget* parent = nullptr);

    void setConnected(bool c);
    void setFps(double fps);
    void setFrames(quint64 n);
    void setDropped(quint64 n);
    void setMirrorAngle(double deg);

private:
    LedIndicator* led_;
    QLabel* connLabel_;
    QLabel* fpsLabel_;
    QLabel* frameLabel_;
    QLabel* dropLabel_;
    QLabel* mirrorLabel_;
};
