#include "LedIndicator.h"
#include <QPainter>

LedIndicator::LedIndicator(QWidget* parent) : QWidget(parent)
{
    setFixedSize(14, 14);
}

void LedIndicator::setState(State s)
{
    state_ = s;
    switch (s) {
    case Green:  color_ = QColor(0x3F, 0xB9, 0x50); break;
    case Yellow: color_ = QColor(0xE9, 0xB9, 0x49); break;
    case Red:    color_ = QColor(0xE5, 0x48, 0x4D); break;
    default:     color_ = QColor(0x55, 0x5D, 0x67); break;
    }
    update();
}

void LedIndicator::setColor(const QColor& c)
{
    color_ = c;
    update();
}

void LedIndicator::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int d = qMin(width(), height()) - 2;
    int x = (width() - d) / 2;
    int y = (height() - d) / 2;
    p.setBrush(color_);
    p.setPen(Qt::NoPen);
    p.drawEllipse(x, y, d, d);
}
