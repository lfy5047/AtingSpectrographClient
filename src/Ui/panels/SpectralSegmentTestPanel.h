#pragma once

#include <QWidget>

class QLabel;

class SpectralSegmentTestPanel : public QWidget {
    Q_OBJECT
public:
    explicit SpectralSegmentTestPanel(QWidget* parent = nullptr);

    void setLinePositions(int firstX, int secondX);

private:
    static QString formatPosition(int x);

    QLabel* firstXValue_ = nullptr;
    QLabel* secondXValue_ = nullptr;
    QLabel* distanceValue_ = nullptr;
};
