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

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setNoSignal();
    QImage currentImage() const { return image_; }

signals:
    void cursorImagePosChanged(const QPoint& pos);

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
    void syncCursorFromWidgetPos(const QPoint& widgetPos);
    void syncCursorFromGlobalPos();

    QImage  image_;
    double  scale_   = 1.0;
    QPointF offset_;
    QPoint  dragStart_;
    QPointF dragOffset_;
    bool    dragging_ = false;
    bool    noSignal_ = true;
};
