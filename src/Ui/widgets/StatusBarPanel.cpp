#include "StatusBarPanel.h"
#include <QHBoxLayout>

StatusBarPanel::StatusBarPanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 2, 6, 2);
    lay->setSpacing(14);

    led_ = new LedIndicator(this);
    connLabel_   = new QLabel(QString::fromUtf8("未连接"), this);
    fpsLabel_    = new QLabel("FPS: -", this);
    frameLabel_  = new QLabel(QString::fromUtf8("帧: 0"), this);
    dropLabel_   = new QLabel(QString::fromUtf8("丢帧: 0"), this);
    mirrorLabel_ = new QLabel(QString::fromUtf8("转镜: -"), this);

    QString style = "font-family: Consolas; font-size: 9pt; color: #9AA3AD;";
    for (auto* l : {connLabel_, fpsLabel_, frameLabel_, dropLabel_, mirrorLabel_})
        l->setStyleSheet(style);

    lay->addWidget(led_);
    lay->addWidget(connLabel_);
    lay->addWidget(fpsLabel_);
    lay->addWidget(frameLabel_);
    lay->addWidget(dropLabel_);
    lay->addStretch();
    lay->addWidget(mirrorLabel_);
}

void StatusBarPanel::setConnected(bool c)
{
    led_->setState(c ? LedIndicator::Green : LedIndicator::Red);
    connLabel_->setText(c ? QString::fromUtf8("已连接")
                          : QString::fromUtf8("未连接"));
}

void StatusBarPanel::setFps(double fps)
{
    fpsLabel_->setText(QString("FPS: %1").arg(fps, 0, 'f', 0));
}

void StatusBarPanel::setFrames(quint64 n)
{
    frameLabel_->setText(QString::fromUtf8("帧: %1").arg(n));
}

void StatusBarPanel::setDropped(quint64 n)
{
    dropLabel_->setText(QString::fromUtf8("丢帧: %1").arg(n));
}

void StatusBarPanel::setMirrorAngle(double deg)
{
    mirrorLabel_->setText(QString::fromUtf8("转镜: %1°").arg(deg, 0, 'f', 2));
}
