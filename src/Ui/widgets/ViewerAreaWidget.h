#pragma once

#include <QPoint>
#include <QWidget>
#include <array>

#include "ImageFrameUtils.h"
#include "SpectrumAnalysisTypes.h"

class ImageView;
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
        PlaybackView = 3,
    };

    explicit ViewerAreaWidget(QWidget* parent = nullptr);

    int currentChannel() const { return currentChannel_; }
    void setCurrentChannel(int channel);
    ImageView* imageView(int channel) const;
    void renderFrame(int channel, int width, int height, int pixfmt, const QByteArray& data);
    void setChannelImage(int channel, const QImage& image);
    void setImageStats(int channel, const ChannelImageStats& stats);
    void setSpectralImage(const QImage& image);
    void setSpectralNoSignal();
    void setSpectralProgressVisible(bool visible, int percent, const QString& text);
    void setSliceAnalysisOverlay(bool enabled, const QVector<SpectrumSampleLine>& lines);

signals:
    void channelChanged(int channel);
    void sliceAnalysisLineAddRequested(int y);
    void sliceAnalysisLineMoveRequested(int index, int y);
    void sliceAnalysisLineDeleteRequested(int index);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void updateChannelTabStyle();
    void refreshImageStatsOverlay();
    void positionImageStatsOverlay();
    void positionSpectralProgressOverlay();
    QString formatImageStatsText(const QPoint& cursorPos, const ChannelImageStats* stats) const;

    QWidget* channelTabBar_ = nullptr;
    QPushButton* chTabRaw16_ = nullptr;
    QPushButton* chTabSlice_ = nullptr;
    QPushButton* chTabSpectral_ = nullptr;
    QPushButton* chTabPlayback_ = nullptr;
    QStackedWidget* viewerStack_ = nullptr;
    ImageView* imageViewRaw_ = nullptr;
    ImageView* imageViewSlice_ = nullptr;
    ImageView* imageViewSpectral_ = nullptr;
    ImageView* imageViewPlayback_ = nullptr;

    QWidget* zoomBar_ = nullptr;
    QWidget* imageStatsOverlay_ = nullptr;
    QLabel* imageStatsLabel_ = nullptr;
    QWidget* spectralProgressOverlay_ = nullptr;
    QLabel* spectralProgressLabel_ = nullptr;
    QProgressBar* spectralProgressBar_ = nullptr;

    int currentChannel_ = Raw16View;
    std::array<QPoint, 4> cursorImagePos_ = {
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
    };
    ChannelImageStats rawImageStats_;
    ChannelImageStats sliceImageStats_;
};
