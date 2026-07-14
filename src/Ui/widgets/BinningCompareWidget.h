#pragma once

#include "Ui/BinningTestTypes.h"

#include <QWidget>
#include <array>

class ImageView;
class QCheckBox;
class QLabel;

class BinningCompareWidget : public QWidget {
    Q_OBJECT
public:
    explicit BinningCompareWidget(QWidget* parent = nullptr);

    void clearSnapshots();
    void setSnapshot(const BinningSnapshot& snapshot);
    void setMeasurementOrientation(Qt::Orientation orientation);
    ImageView* imageViewForFactor(int factor) const;
    int measurementWidth(int factor) const;

signals:
    void measurementChanged(int factor, int pixels);

private:
    static int indexForFactor(int factor);
    void initializeMeasurement(int index);
    void renderSnapshots();
    void updateCardLabels(int index);

    std::array<BinningSnapshot, 3> snapshots_;
    std::array<bool, 3> snapshotValid_ = {{false, false, false}};
    std::array<ImageView*, 3> imageViews_ = {{nullptr, nullptr, nullptr}};
    std::array<QLabel*, 3> sizeLabels_ = {{nullptr, nullptr, nullptr}};
    std::array<QLabel*, 3> pixelRatioLabels_ = {{nullptr, nullptr, nullptr}};
    std::array<QLabel*, 3> dnStatsLabels_ = {{nullptr, nullptr, nullptr}};
    std::array<std::array<int, 2>, 3> measurementLines_ = {{{{-1, -1}}, {{-1, -1}}, {{-1, -1}}}};
    QCheckBox* commonRangeCheck_ = nullptr;
    Qt::Orientation measurementOrientation_ = Qt::Horizontal;
};
