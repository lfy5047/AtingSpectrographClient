#pragma once

#include <QWidget>

class LedIndicator : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
public:
    enum State { Off, Green, Yellow, Red };

    explicit LedIndicator(QWidget* parent = nullptr);

    void setState(State s);
    State state() const { return state_; }
    QColor color() const { return color_; }
    void setColor(const QColor& c);

    QSize sizeHint() const override { return QSize(14, 14); }
    QSize minimumSizeHint() const override { return QSize(10, 10); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    State  state_ = Off;
    QColor color_ = QColor(0x55, 0x5D, 0x67);
};
