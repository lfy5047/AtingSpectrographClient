#pragma once

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
    void setNoSignal();
    QImage currentImage() const { return image_; }
    void setAnalysisOverlayEnabled(bool enabled);
    void setAnalysisLines(const QVector<SpectrumSampleLine>& lines);

signals:
    void cursorImagePosChanged(const QPoint& pos);
    void analysisLineAddRequested(int y);
    void analysisLineMoveRequested(int index, int y);
    void analysisLineDeleteRequested(int index);

protected:
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
    int hitAnalysisLine(const QPoint& widgetPos) const;
    void drawAnalysisOverlay(QPainter& p, const QRectF& r);
    void beginPan(const QPoint& pos);
    void syncCursorFromWidgetPos(const QPoint& widgetPos);
    void syncCursorFromGlobalPos();

    enum class InteractionMode {
        Normal,
        LineDragging,
        PendingClick,
        Panning,
    };

    QImage  image_;
    double  scale_   = 1.0;
    QPointF offset_;
    QPoint  dragStart_;
    QPointF dragOffset_;
    bool    dragging_ = false;
    bool    noSignal_ = true;
    bool    analysisOverlayEnabled_ = false;
    QVector<SpectrumSampleLine> analysisLines_;
    InteractionMode interactionMode_ = InteractionMode::Normal;
    int     activeLineIndex_ = -1;
};
