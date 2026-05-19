#include "ImageView.h"
#include <QPainter>
#include <QWheelEvent>
#include "plog/Log.h"

ImageView::ImageView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setStyleSheet("background-color: #0A0E14;");
}

void ImageView::setImage(const QImage& img)
{
    image_ = img;
    noSignal_ = false;
    update();
}

void ImageView::setNoSignal()
{
    image_ = QImage();
    noSignal_ = true;
    update();
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(0x1A, 0x1E, 0x24));

    if (noSignal_ || image_.isNull()) {
        // No-signal display
        QColor ringColor(0x4C, 0x8E, 0xF7, 40);
        QColor textColor(0x54, 0x5D, 0x68);

        p.setPen(Qt::NoPen);
        p.setBrush(ringColor);
        int cx = rect().center().x();
        int cy = rect().center().y();
        int radius = 30;
        p.drawEllipse(QPoint(cx, cy), radius, radius);

        p.setPen(QPen(ringColor.lighter(150), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), radius + 4, radius + 4);

        QFont f("Microsoft YaHei UI", 11);
        p.setFont(f);
        p.setPen(textColor);
        p.drawText(QRect(cx - 100, cy + radius + 20, 200, 24),
                   Qt::AlignHCenter | Qt::AlignTop,
                   QString::fromUtf8("等待数据流..."));
        return;
    }

    QSizeF imgSz = image_.size();
    QSizeF viewSz = size();

    double fitScale = qMin(viewSz.width() / imgSz.width(),
                           viewSz.height() / imgSz.height());
    double s = fitScale * scale_;

    double w = imgSz.width() * s;
    double h = imgSz.height() * s;
    double x = (viewSz.width() - w) / 2.0 + offset_.x();
    double y = (viewSz.height() - h) / 2.0 + offset_.y();

    p.drawImage(QRectF(x, y, w, h), image_);

    // corner info
    p.setPen(QColor(0x7D, 0x85, 0x90));
    QFont f("Consolas", 9);
    p.setFont(f);
    QString info = QString("%1x%2  |  %3%").arg(image_.width()).arg(image_.height()).arg(static_cast<int>(scale_ * 100));
    p.drawText(10, height() - 10, info);
}

void ImageView::wheelEvent(QWheelEvent* e)
{
    double delta = e->angleDelta().y() > 0 ? 1.1 : 0.9;
    scale_ = qBound(0.1, scale_ * delta, 20.0);
    update();
}

void ImageView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStart_ = e->pos();
        dragOffset_ = offset_;
    }
}

void ImageView::mouseMoveEvent(QMouseEvent* e)
{
    if (dragging_) {
        offset_ = dragOffset_ + (e->pos() - dragStart_);
        update();
    }
}

void ImageView::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = false;
}

void ImageView::mouseDoubleClickEvent(QMouseEvent*)
{
    scale_ = 1.0;
    offset_ = QPointF(0, 0);
    update();
}
