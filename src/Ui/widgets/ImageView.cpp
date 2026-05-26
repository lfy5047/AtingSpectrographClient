#include "ImageView.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

ImageView::ImageView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setStyleSheet("background-color: #0A0E14;");
    setMouseTracking(true);
}

void ImageView::setImage(const QImage& img)
{
    image_ = img;
    noSignal_ = false;
    update();
    syncCursorFromGlobalPos();
}

void ImageView::setNoSignal()
{
    image_ = QImage();
    noSignal_ = true;
    update();
    emit cursorImagePosChanged(QPoint(-1, -1));
}

QRectF ImageView::imageRect() const
{
    if (noSignal_ || image_.isNull()) return QRectF();

    const QSizeF imgSz = image_.size();
    const QSizeF viewSz = size();
    if (imgSz.width() <= 0.0 || imgSz.height() <= 0.0 ||
        viewSz.width() <= 0.0 || viewSz.height() <= 0.0) {
        return QRectF();
    }

    const double fitScale = qMin(viewSz.width() / imgSz.width(),
                                 viewSz.height() / imgSz.height());
    const double s = fitScale * scale_;
    const double w = imgSz.width() * s;
    const double h = imgSz.height() * s;
    const double x = (viewSz.width() - w) / 2.0 + offset_.x();
    const double y = (viewSz.height() - h) / 2.0 + offset_.y();
    return QRectF(x, y, w, h);
}

QPoint ImageView::imagePosFromWidgetPos(const QPoint& widgetPos) const
{
    if (noSignal_ || image_.isNull()) return QPoint(-1, -1);

    const QRectF r = imageRect();
    if (r.isNull() || r.width() <= 0.0 || r.height() <= 0.0) return QPoint(-1, -1);

    const QPointF p(widgetPos);
    const double right = r.left() + r.width();
    const double bottom = r.top() + r.height();
    if (p.x() < r.left() || p.x() >= right || p.y() < r.top() || p.y() >= bottom) {
        return QPoint(-1, -1);
    }

    const double rx = (p.x() - r.left()) / r.width();
    const double ry = (p.y() - r.top()) / r.height();
    const int ix = qBound(0, static_cast<int>(rx * image_.width()), image_.width() - 1);
    const int iy = qBound(0, static_cast<int>(ry * image_.height()), image_.height() - 1);
    return QPoint(ix, iy);
}

void ImageView::syncCursorFromWidgetPos(const QPoint& widgetPos)
{
    emit cursorImagePosChanged(imagePosFromWidgetPos(widgetPos));
}

void ImageView::syncCursorFromGlobalPos()
{
    if (!underMouse()) {
        emit cursorImagePosChanged(QPoint(-1, -1));
        return;
    }
    syncCursorFromWidgetPos(mapFromGlobal(QCursor::pos()));
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(0x1A, 0x1E, 0x24));

    if (noSignal_ || image_.isNull()) {
        QColor ringColor(0x4C, 0x8E, 0xF7, 40);
        QColor textColor(0x54, 0x5D, 0x68);

        p.setPen(Qt::NoPen);
        p.setBrush(ringColor);
        const int cx = rect().center().x();
        const int cy = rect().center().y();
        const int radius = 30;
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

    const QRectF imageRectF = imageRect();
    p.drawImage(imageRectF, image_);

    p.setPen(QColor(0x7D, 0x85, 0x90));
    QFont f("Consolas", 9);
    p.setFont(f);
    const QString info = QString("%1x%2  |  %3%")
                             .arg(image_.width())
                             .arg(image_.height())
                             .arg(static_cast<int>(scale_ * 100));
    p.drawText(10, height() - 10, info);
}

void ImageView::wheelEvent(QWheelEvent* e)
{
    const double delta = e->angleDelta().y() > 0 ? 1.1 : 0.9;
    scale_ = qBound(0.1, scale_ * delta, 20.0);
    update();
    syncCursorFromWidgetPos(e->position().toPoint());
}

void ImageView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStart_ = e->pos();
        dragOffset_ = offset_;
    }
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseMoveEvent(QMouseEvent* e)
{
    if (dragging_) {
        offset_ = dragOffset_ + (e->pos() - dragStart_);
        update();
    }
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseReleaseEvent(QMouseEvent* e)
{
    dragging_ = false;
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseDoubleClickEvent(QMouseEvent* e)
{
    scale_ = 1.0;
    offset_ = QPointF(0, 0);
    update();
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::leaveEvent(QEvent*)
{
    emit cursorImagePosChanged(QPoint(-1, -1));
}

void ImageView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    syncCursorFromGlobalPos();
}

void ImageView::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    syncCursorFromGlobalPos();
}
