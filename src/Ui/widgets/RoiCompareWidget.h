#pragma once

#include "Ui/RoiTestTypes.h"

#include <QWidget>

class ImageView;
class QCheckBox;
class QLabel;

class RoiCompareWidget : public QWidget {
    Q_OBJECT
public:
    explicit RoiCompareWidget(QWidget* parent = nullptr);

    void clearSnapshots();
    void setFullFrameSnapshot(const RoiSnapshot& snapshot);
    void setRoiSnapshot(const RoiSnapshot& snapshot);
    ImageView* fullFrameView() const { return fullFrameView_; }
    ImageView* roiView() const { return roiView_; }

private:
    static bool snapshotIsValid(const RoiSnapshot& snapshot);
    void renderSnapshots();
    void updateLabels();

    RoiSnapshot fullFrameSnapshot_;
    RoiSnapshot roiSnapshot_;
    bool fullFrameValid_ = false;
    bool roiValid_ = false;
    QCheckBox* commonRangeCheck_ = nullptr;
    QLabel* fullFrameSizeLabel_ = nullptr;
    QLabel* fullFrameStatsLabel_ = nullptr;
    QLabel* roiSizeLabel_ = nullptr;
    QLabel* roiStatsLabel_ = nullptr;
    ImageView* fullFrameView_ = nullptr;
    ImageView* roiView_ = nullptr;
};
