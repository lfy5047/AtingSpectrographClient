#pragma once

#include <QPoint>
#include <QWidget>
#include <array>

#include "ImageFrameUtils.h"
#include "SpectrumAnalysisTypes.h"

class ImageView;
class BinningCompareWidget;
class RoiCompareWidget;
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;

class ViewerAreaWidget : public QWidget {
    Q_OBJECT
public:
    enum Channel {
        Raw16View = 0,
        SliceStitch16View = 1,
        SpectralView = 2,
        SpectralPreviewView = 3,
        PlaybackView = 4,
        BinningCompareView = 5,
        RoiCompareView = 6,
        ChannelCount = 7,
    };

    explicit ViewerAreaWidget(QWidget* parent = nullptr);

    int currentChannel() const { return currentChannel_; }
    void setCurrentChannel(int channel);
    bool hasChannelImage(int channel) const;
    ImageView* imageView(int channel) const;
    BinningCompareWidget* binningCompareWidget() const { return binningCompareWidget_; }
    RoiCompareWidget* roiCompareWidget() const { return roiCompareWidget_; }
    void renderFrame(int channel, int width, int height, int pixfmt, const QByteArray& data);
    void setChannelImage(int channel, const QImage& image);
    void setImageStats(int channel, const ChannelImageStats& stats);
    void setSpectralImage(const QImage& image);
    void setSpectralNoSignal();
    void setSpectralProgressVisible(bool visible, int percent, const QString& text);
    void setSliceAnalysisOverlay(bool enabled, const QVector<SpectrumSampleLine>& lines);
    void setSpectralSegmentTestEnabled(bool enabled);

signals:
    void channelChanged(int channel);
    void sliceAnalysisLineAddRequested(int y);
    void sliceAnalysisLineMoveRequested(int index, int y);
    void sliceAnalysisLineDeleteRequested(int index);
    void spectralSegmentPositionsChanged(int firstX, int secondX);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void updateChannelTabStyle();
    void refreshImageStatsOverlay();
    void positionImageStatsOverlay();
    void positionSpectralProgressOverlay();
    void syncSpectralSegmentLinesWithRawImage();
    QString formatImageStatsText(const QPoint& cursorPos, const ChannelImageStats* stats) const;

    QWidget* channelTabBar_ = nullptr;
    QPushButton* chTabRaw16_ = nullptr;
    QPushButton* chTabSlice_ = nullptr;
    QPushButton* chTabSpectral_ = nullptr;
    QPushButton* chTabSpectralPreview_ = nullptr;
    QPushButton* chTabPlayback_ = nullptr;
    QPushButton* chTabBinningCompare_ = nullptr;
    QPushButton* chTabRoiCompare_ = nullptr;
    QStackedWidget* viewerStack_ = nullptr;
    ImageView* imageViewRaw_ = nullptr;
    ImageView* imageViewSlice_ = nullptr;
    ImageView* imageViewSpectral_ = nullptr;
    ImageView* imageViewSpectralPreview_ = nullptr;
    ImageView* imageViewPlayback_ = nullptr;
    BinningCompareWidget* binningCompareWidget_ = nullptr;
    RoiCompareWidget* roiCompareWidget_ = nullptr;

    QWidget* zoomBar_ = nullptr;
    QWidget* imageStatsOverlay_ = nullptr;
    QLabel* imageStatsLabel_ = nullptr;
    QWidget* spectralProgressOverlay_ = nullptr;
    QLabel* spectralProgressLabel_ = nullptr;
    QProgressBar* spectralProgressBar_ = nullptr;

    int currentChannel_ = Raw16View;
    std::array<QPoint, ChannelCount> cursorImagePos_ = {
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
    };
    ChannelImageStats rawImageStats_;
    ChannelImageStats sliceImageStats_;
    ChannelImageStats playbackImageStats_;
    bool spectralSegmentTestEnabled_ = false;
    int spectralSegmentImageWidth_ = 0;
    std::array<int, 2> spectralSegmentLineXs_ = {{-1, -1}};
};
