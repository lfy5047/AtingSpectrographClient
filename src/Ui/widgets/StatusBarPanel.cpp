#include "StatusBarPanel.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QStyle>
#include <QVariant>

static QFrame* makeSep(QWidget* parent)
{
    auto* sep = new QFrame(parent);
    sep->setObjectName("statusSep");
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedWidth(1);
    return sep;
}

StatusBarPanel::StatusBarPanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(10);

    led_ = new LedIndicator(this);

    connLabel_   = new QLabel(QString::fromUtf8("未连接"), this);
    connLabel_->setProperty("connState", "err");

    fpsLabel_    = new QLabel("FPS: -", this);
    fpsLabel_->setProperty("secondary", true);

    frameLabel_  = new QLabel(QString::fromUtf8("帧: 0"), this);
    frameLabel_->setProperty("secondary", true);

    dropLabel_   = new QLabel(QString::fromUtf8("丢帧: 0"), this);
    dropLabel_->setProperty("secondary", true);

    mirrorLabel_ = new QLabel(QString::fromUtf8("转镜: -"), this);
    mirrorLabel_->setProperty("secondary", true);

    lay->addWidget(led_);
    lay->addWidget(connLabel_);
    lay->addWidget(makeSep(this));
    lay->addWidget(fpsLabel_);
    lay->addWidget(makeSep(this));
    lay->addWidget(frameLabel_);
    lay->addWidget(makeSep(this));
    lay->addWidget(dropLabel_);
    lay->addStretch();
    lay->addWidget(makeSep(this));
    lay->addWidget(mirrorLabel_);
}

void StatusBarPanel::setConnected(bool c)
{
    led_->setState(c ? LedIndicator::Green : LedIndicator::Red);
    connLabel_->setText(c ? QString::fromUtf8("已连接") : QString::fromUtf8("未连接"));
    connLabel_->setProperty("connState", c ? "ok" : "err");
    connLabel_->style()->polish(connLabel_);
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
