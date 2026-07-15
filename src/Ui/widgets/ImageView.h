#pragma once

#include <QByteArray>
#include <QImage>
#include <QEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QVector>

#include "SpectrumAnalysisTypes.h"

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setImage(const QImage& img, int pixelFormat, const QByteArray& pixelData);
    void setNoSignal();
    void resetView();
    QImage currentImage() const { return image_; }
    bool pixelDnAt(const QPoint& pos, quint16* value) const;
    void setAnalysisOverlayEnabled(bool enabled);
    void setAnalysisLines(const QVector<SpectrumSampleLine>& lines);
    void setSpectralSegmentTestEnabled(bool enabled);
    void setSpectralSegmentLines(int firstX, int secondX);
    void setPixelMeasureEnabled(bool enabled);
    void setPixelMeasureOrientation(Qt::Orientation orientation);
    void setPixelMeasureLines(int first, int second);

signals:
    void cursorImagePosChanged(const QPoint& pos);
    void analysisLineAddRequested(int y);
    void analysisLineMoveRequested(int index, int y);
    void analysisLineDeleteRequested(int index);
    void spectralSegmentLineMoveRequested(int index, int x);
    void pixelMeasureLineMoveRequested(int index, int position);

protected:
    void changeEvent(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    QRectF imageRect() const;
    QPoint imagePosFromWidgetPos(const QPoint& widgetPos) const;
    double widgetYForImageY(int y) const;
    double widgetXForImageX(int x) const;
    int hitAnalysisLine(const QPoint& widgetPos) const;
    int hitSpectralSegmentLine(const QPoint& widgetPos) const;
    void drawAnalysisOverlay(QPainter& p, const QRectF& r);
    void drawSpectralSegmentOverlay(QPainter& p, const QRectF& r);
    void updateSpectralSegmentCursor(const QPoint& widgetPos);
    void beginPan(const QPoint& pos);
    void syncCursorFromWidgetPos(const QPoint& widgetPos);
    void syncCursorFromGlobalPos();
    bool outdoorThemeActive() const;

    enum class InteractionMode {
        Normal,
        LineDragging,
        SegmentLineDragging,
        PendingClick,
        Panning,
    };

    QImage  image_;
    QByteArray pixelData_;
    int     pixelFormat_ = 0;
    double  scale_   = 1.0;
    QPointF offset_;
    QPoint  dragStart_;
    QPointF dragOffset_;
    bool    dragging_ = false;
    bool    noSignal_ = true;
    bool    analysisOverlayEnabled_ = false;
    QVector<SpectrumSampleLine> analysisLines_;
    bool    spectralSegmentTestEnabled_ = false;
    int     spectralSegmentLineXs_[2] = {-1, -1};
    Qt::Orientation pixelMeasureOrientation_ = Qt::Horizontal;
    InteractionMode interactionMode_ = InteractionMode::Normal;
    int     activeLineIndex_ = -1;
};
