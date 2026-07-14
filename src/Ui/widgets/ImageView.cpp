#include "ImageView.h"

#include "ThemeManager.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

ImageView::ImageView(QWidget* parent) : QWidget(parent)
{
    setObjectName("imageView");
    setMinimumSize(320, 240);
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

void ImageView::resetView()
{
    scale_ = 1.0;
    offset_ = QPointF(0, 0);
    dragging_ = false;
    interactionMode_ = InteractionMode::Normal;
    activeLineIndex_ = -1;
    update();
}

void ImageView::setAnalysisOverlayEnabled(bool enabled)
{
    if (analysisOverlayEnabled_ == enabled) return;
    analysisOverlayEnabled_ = enabled;
    interactionMode_ = InteractionMode::Normal;
    dragging_ = false;
    activeLineIndex_ = -1;
    update();
}

void ImageView::setAnalysisLines(const QVector<SpectrumSampleLine>& lines)
{
    analysisLines_ = lines;
    update();
}

void ImageView::setSpectralSegmentTestEnabled(bool enabled)
{
    pixelMeasureOrientation_ = Qt::Horizontal;
    setPixelMeasureEnabled(enabled);
}

void ImageView::setPixelMeasureEnabled(bool enabled)
{
    if (spectralSegmentTestEnabled_ == enabled) return;
    spectralSegmentTestEnabled_ = enabled;
    interactionMode_ = InteractionMode::Normal;
    dragging_ = false;
    activeLineIndex_ = -1;
    if (!enabled) {
        unsetCursor();
    }
    update();
}

void ImageView::setSpectralSegmentLines(int firstX, int secondX)
{
    setPixelMeasureLines(firstX, secondX);
}

void ImageView::setPixelMeasureOrientation(Qt::Orientation orientation)
{
    if (pixelMeasureOrientation_ == orientation) return;
    pixelMeasureOrientation_ = orientation;
    update();
}

void ImageView::setPixelMeasureLines(int first, int second)
{
    spectralSegmentLineXs_[0] = first;
    spectralSegmentLineXs_[1] = second;
    update();
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

double ImageView::widgetYForImageY(int y) const
{
    const QRectF r = imageRect();
    if (r.isNull() || image_.height() <= 0) return -1.0;
    return r.top() + (static_cast<double>(y) + 0.5) * r.height() / image_.height();
}

double ImageView::widgetXForImageX(int x) const
{
    const QRectF r = imageRect();
    if (r.isNull() || image_.width() <= 0) return -1.0;
    return r.left() + (static_cast<double>(x) + 0.5) * r.width() / image_.width();
}

int ImageView::hitAnalysisLine(const QPoint& widgetPos) const
{
    if (!analysisOverlayEnabled_ || noSignal_ || image_.isNull()) return -1;
    const QRectF r = imageRect();
    if (r.isNull()) return -1;
    const QPointF p(widgetPos);
    if (p.x() < r.left() || p.x() >= r.right() || p.y() < r.top() || p.y() >= r.bottom()) {
        return -1;
    }

    int bestIndex = -1;
    double bestDistance = 6.0;
    for (int i = 0; i < analysisLines_.size(); ++i) {
        const double lineY = widgetYForImageY(analysisLines_[i].y);
        const double distance = std::abs(lineY - p.y());
        if (distance <= 5.0 && distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

int ImageView::hitSpectralSegmentLine(const QPoint& widgetPos) const
{
    if (!spectralSegmentTestEnabled_ || noSignal_ || image_.isNull()) return -1;
    const QRectF r = imageRect();
    if (r.isNull()) return -1;
    const QPointF p(widgetPos);
    if (p.x() < r.left() || p.x() >= r.right() || p.y() < r.top() || p.y() >= r.bottom()) {
        return -1;
    }

    int bestIndex = -1;
    double bestDistance = 7.0;
    const int extent = pixelMeasureOrientation_ == Qt::Horizontal ? image_.width() : image_.height();
    for (int i = 0; i < 2; ++i) {
        if (spectralSegmentLineXs_[i] < 0 || spectralSegmentLineXs_[i] >= extent) continue;
        const double linePosition = pixelMeasureOrientation_ == Qt::Horizontal
            ? widgetXForImageX(spectralSegmentLineXs_[i])
            : widgetYForImageY(spectralSegmentLineXs_[i]);
        const double distance = std::abs(linePosition - (pixelMeasureOrientation_ == Qt::Horizontal ? p.x() : p.y()));
        if (distance <= 6.0 && distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

void ImageView::updateSpectralSegmentCursor(const QPoint& widgetPos)
{
    const bool overMovableLine = spectralSegmentTestEnabled_
        && (interactionMode_ == InteractionMode::SegmentLineDragging
            || (interactionMode_ == InteractionMode::Normal
                && hitSpectralSegmentLine(widgetPos) >= 0));
    if (overMovableLine) {
        setCursor(pixelMeasureOrientation_ == Qt::Horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void ImageView::beginPan(const QPoint& pos)
{
    dragging_ = true;
    dragStart_ = pos;
    dragOffset_ = offset_;
}

static int niceTickStep(int size)
{
    if (size <= 0) return 1;
    const int target = qMax(1, size / 8);
    const int steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    for (int step : steps) {
        if (step >= target) return step;
    }
    return 10000;
}

void ImageView::drawAnalysisOverlay(QPainter& p, const QRectF& r)
{
    if (!analysisOverlayEnabled_ || noSignal_ || image_.isNull() || r.isNull()) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(rect());

    const QColor axisColor(0xD0, 0xD7, 0xDE, 210);
    const QColor tickTextColor(0xF0, 0xF6, 0xFC, 235);
    const QColor tickShadowColor(0x0A, 0x0E, 0x14, 220);
    QFont tickFont("Consolas", 8);
    p.setFont(tickFont);

    p.setPen(QPen(axisColor, 1));
    p.drawRect(r);

    const int xStep = niceTickStep(image_.width());
    for (int x = 0; x < image_.width(); x += xStep) {
        const double wx = r.left() + (static_cast<double>(x) + 0.5) * r.width() / image_.width();
        p.setPen(axisColor);
        p.drawLine(QPointF(wx, r.bottom()), QPointF(wx, r.bottom() + 5));
        const QPointF labelPos(wx + 3, qMin<double>(height() - 4, r.bottom() + 15));
        p.setPen(tickShadowColor);
        p.drawText(labelPos + QPointF(1, 1), QString::number(x));
        p.setPen(tickTextColor);
        p.drawText(labelPos, QString::number(x));
    }

    const int yStep = niceTickStep(image_.height());
    for (int y = 0; y < image_.height(); y += yStep) {
        const double wy = widgetYForImageY(y);
        p.setPen(axisColor);
        p.drawLine(QPointF(r.left() - 5, wy), QPointF(r.left(), wy));
        QRectF labelRect(r.left() - 52, wy - 8, 46, 16);
        if (labelRect.left() < 2) {
            labelRect.moveLeft(2);
        }
        p.setPen(tickShadowColor);
        p.drawText(labelRect.translated(1, 1), Qt::AlignRight | Qt::AlignVCenter, QString::number(y));
        p.setPen(tickTextColor);
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(y));
    }

    for (int i = 0; i < analysisLines_.size(); ++i) {
        const SpectrumSampleLine& line = analysisLines_[i];
        if (line.y < 0 || line.y >= image_.height()) continue;

        const double wy = widgetYForImageY(line.y);
        QColor color = line.color.isValid() ? line.color : QColor(0x4C, 0x8E, 0xF7);
        p.setPen(QPen(color, i == activeLineIndex_ ? 3 : 2));
        p.drawLine(QPointF(r.left(), wy), QPointF(r.right(), wy));

        QRectF labelRect(r.left() + 8, wy - 18, 72, 16);
        if (labelRect.top() < r.top()) labelRect.moveTop(r.top() + 2);
        if (labelRect.bottom() > r.bottom()) labelRect.moveBottom(r.bottom() - 2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x0A, 0x0E, 0x14, 210));
        p.drawRoundedRect(labelRect, 4, 4);
        p.setPen(color);
        p.drawText(labelRect, Qt::AlignCenter, line.name());
    }

    p.restore();
}

void ImageView::drawSpectralSegmentOverlay(QPainter& p, const QRectF& r)
{
    if (!spectralSegmentTestEnabled_ || noSignal_ || image_.isNull() || r.isNull()) return;

    static const QColor colors[] = {QColor(0xF9, 0x73, 0x16), QColor(0x22, 0xC5, 0x5E)};
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(r);

    for (int i = 0; i < 2; ++i) {
        const int position = spectralSegmentLineXs_[i];
        const int extent = pixelMeasureOrientation_ == Qt::Horizontal ? image_.width() : image_.height();
        if (position < 0 || position >= extent) continue;

        const QColor color = colors[i];
        p.setPen(QPen(color, i == activeLineIndex_ ? 3 : 2));
        QRectF labelRect;
        if (pixelMeasureOrientation_ == Qt::Horizontal) {
            const double wx = widgetXForImageX(position);
            p.drawLine(QPointF(wx, r.top()), QPointF(wx, r.bottom()));
            labelRect = QRectF(wx + 6, r.top() + 8, 76, 22);
            if (labelRect.right() > r.right() - 2) {
                labelRect.moveRight(wx - 6);
            }
        } else {
            const double wy = widgetYForImageY(position);
            p.drawLine(QPointF(r.left(), wy), QPointF(r.right(), wy));
            labelRect = QRectF(r.left() + 8, wy + 6, 76, 22);
            if (labelRect.bottom() > r.bottom() - 2) {
                labelRect.moveBottom(wy - 6);
            }
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x0A, 0x0E, 0x14, 210));
        p.drawRoundedRect(labelRect, 4, 4);
        p.setPen(color);
        p.drawText(labelRect, Qt::AlignCenter,
                   QStringLiteral("%1%2: %3")
                       .arg(pixelMeasureOrientation_ == Qt::Horizontal ? QStringLiteral("X")
                                                                      : QStringLiteral("Y"))
                       .arg(i + 1)
                       .arg(position));
    }

    p.restore();
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

bool ImageView::outdoorThemeActive() const
{
    return ThemeManager::currentTheme() == ThemeManager::Theme::OutdoorLight;
}

void ImageView::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);
    if (e && e->type() == QEvent::StyleChange) {
        update();
    }
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const bool outdoor = outdoorThemeActive();
    const QColor backgroundColor = outdoor ? QColor(0xF3, 0xF6, 0xFA) : QColor(0x1A, 0x1E, 0x24);
    p.fillRect(rect(), backgroundColor);

    if (noSignal_ || image_.isNull()) {
        QColor ringColor = outdoor ? QColor(0x0B, 0x68, 0xD8, 34) : QColor(0x4C, 0x8E, 0xF7, 40);
        QColor textColor = outdoor ? QColor(0x4B, 0x55, 0x63) : QColor(0x54, 0x5D, 0x68);

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
    drawAnalysisOverlay(p, imageRectF);
    drawSpectralSegmentOverlay(p, imageRectF);

    p.setPen(outdoor ? QColor(0x4B, 0x55, 0x63) : QColor(0x7D, 0x85, 0x90));
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
    if (analysisOverlayEnabled_ && e->button() == Qt::RightButton) {
        const int hit = hitAnalysisLine(e->pos());
        if (hit >= 0) {
            emit analysisLineDeleteRequested(hit);
            syncCursorFromWidgetPos(e->pos());
            return;
        }
    }

    if (e->button() == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::Panning;
        beginPan(e->pos());
    } else if (e->button() == Qt::LeftButton) {
        const int segmentHit = hitSpectralSegmentLine(e->pos());
        if (segmentHit >= 0) {
            interactionMode_ = InteractionMode::SegmentLineDragging;
            activeLineIndex_ = segmentHit;
            updateSpectralSegmentCursor(e->pos());
            update();
        } else if (analysisOverlayEnabled_) {
            const int hit = hitAnalysisLine(e->pos());
            if (hit >= 0) {
                interactionMode_ = InteractionMode::LineDragging;
                activeLineIndex_ = hit;
                update();
            } else {
                interactionMode_ = InteractionMode::PendingClick;
                dragStart_ = e->pos();
                dragOffset_ = offset_;
                dragging_ = false;
            }
        } else {
            interactionMode_ = InteractionMode::Panning;
            beginPan(e->pos());
        }
    }
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseMoveEvent(QMouseEvent* e)
{
    if (interactionMode_ == InteractionMode::SegmentLineDragging && activeLineIndex_ >= 0) {
        const QPoint pos = imagePosFromWidgetPos(e->pos());
        const int position = pixelMeasureOrientation_ == Qt::Horizontal ? pos.x() : pos.y();
        if (position >= 0) {
            emit pixelMeasureLineMoveRequested(activeLineIndex_, position);
            emit spectralSegmentLineMoveRequested(activeLineIndex_, position);
        }
    } else if (interactionMode_ == InteractionMode::LineDragging && activeLineIndex_ >= 0) {
        const QPoint pos = imagePosFromWidgetPos(e->pos());
        if (pos.y() >= 0) {
            emit analysisLineMoveRequested(activeLineIndex_, pos.y());
        }
    } else if (interactionMode_ == InteractionMode::PendingClick) {
        if ((e->pos() - dragStart_).manhattanLength() > 4) {
            interactionMode_ = InteractionMode::Panning;
            beginPan(dragStart_);
        }
    }

    if (interactionMode_ == InteractionMode::Panning && dragging_) {
        offset_ = dragOffset_ + (e->pos() - dragStart_);
        update();
    }
    updateSpectralSegmentCursor(e->pos());
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseReleaseEvent(QMouseEvent* e)
{
    if (interactionMode_ == InteractionMode::PendingClick && e->button() == Qt::LeftButton) {
        const QPoint pos = imagePosFromWidgetPos(e->pos());
        if (pos.y() >= 0) {
            emit analysisLineAddRequested(pos.y());
        }
    }

    dragging_ = false;
    interactionMode_ = InteractionMode::Normal;
    activeLineIndex_ = -1;
    updateSpectralSegmentCursor(e->pos());
    update();
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::mouseDoubleClickEvent(QMouseEvent* e)
{
    resetView();
    syncCursorFromWidgetPos(e->pos());
}

void ImageView::leaveEvent(QEvent*)
{
    unsetCursor();
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
