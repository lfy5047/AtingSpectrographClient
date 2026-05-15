#include "ImageView.h"
#include <QPainter>
#include <QWheelEvent>

ImageView::ImageView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setStyleSheet("background-color: #1A1E24;");
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
        p.setPen(QColor(0x55, 0x5D, 0x67));
        QFont f = p.font();
        f.setPointSize(16);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, "No Signal");
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
    p.setPen(QColor(0x9A, 0xA3, 0xAD));
    QFont f("Consolas", 8);
    p.setFont(f);
    QString info = QString("%1x%2").arg(image_.width()).arg(image_.height());
    p.drawText(10, 18, info);
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
