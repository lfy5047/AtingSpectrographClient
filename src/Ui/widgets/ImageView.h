#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>

class ImageView : public QWidget {
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setNoSignal();
    QImage currentImage() const { return image_; }

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    QImage  image_;
    double  scale_   = 1.0;
    QPointF offset_;
    QPoint  dragStart_;
    QPointF dragOffset_;
    bool    dragging_ = false;
    bool    noSignal_ = true;
};
