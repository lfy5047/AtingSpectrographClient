#include "ViewerAreaWidget.h"

#include "ImageView.h"

#include <QEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include "plog/Log.h"

ViewerAreaWidget::ViewerAreaWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("viewerContainer");
    setupUi();
    installEventFilter(this);
}

void ViewerAreaWidget::setupUi()
{
    auto* viewerLayout = new QVBoxLayout(this);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);

    channelTabBar_ = new QWidget(this);
    channelTabBar_->setObjectName("channelTabBar");
    channelTabBar_->setFixedHeight(36);
    auto* chTabLayout = new QHBoxLayout(channelTabBar_);
    chTabLayout->setContentsMargins(0, 0, 0, 0);
    chTabLayout->setSpacing(2);
    chTabLayout->addStretch();

    chTabRaw16_ = new QPushButton("Raw16", channelTabBar_);
    chTabSlice_ = new QPushButton("SliceStitch16", channelTabBar_);
    chTabSpectral_ = new QPushButton("Spectral", channelTabBar_);
    chTabPlayback_ = new QPushButton("Playback", channelTabBar_);
    const QList<QPushButton*> tabs = {chTabRaw16_, chTabSlice_, chTabSpectral_, chTabPlayback_};
    for (auto* tab : tabs) {
        tab->setObjectName("channelTab");
        tab->setFixedHeight(28);
        tab->setCursor(Qt::PointingHandCursor);
        chTabLayout->addWidget(tab);
    }
    chTabLayout->addStretch();
    viewerLayout->addWidget(channelTabBar_);
    viewerLayout->addSpacing(10);

    viewerStack_ = new QStackedWidget(this);
    imageViewRaw_ = new ImageView(viewerStack_);
    imageViewSlice_ = new ImageView(viewerStack_);
    imageViewSpectral_ = new ImageView(viewerStack_);
    imageViewPlayback_ = new ImageView(viewerStack_);
    viewerStack_->addWidget(imageViewRaw_);
    viewerStack_->addWidget(imageViewSlice_);
    viewerStack_->addWidget(imageViewSpectral_);
    viewerStack_->addWidget(imageViewPlayback_);
    viewerLayout->addWidget(viewerStack_, 1);

    connect(chTabRaw16_, &QPushButton::clicked, this, [this]() { setCurrentChannel(Raw16View); });
    connect(chTabSlice_, &QPushButton::clicked, this, [this]() { setCurrentChannel(SliceStitch16View); });
    connect(chTabSpectral_, &QPushButton::clicked, this, [this]() { setCurrentChannel(SpectralView); });
    connect(chTabPlayback_, &QPushButton::clicked, this, [this]() { setCurrentChannel(PlaybackView); });

    zoomBar_ = new QWidget(this);
    zoomBar_->setObjectName("zoomBar");
    zoomBar_->setFixedHeight(38);
    zoomBar_->setAttribute(Qt::WA_TransparentForMouseEvents);
    zoomBar_->hide();
    auto* zoomLayout = new QHBoxLayout(zoomBar_);
    zoomLayout->setContentsMargins(8, 0, 8, 0);
    zoomLayout->setSpacing(3);
    zoomLayout->addStretch();

    imageStatsOverlay_ = new QWidget(this);
    imageStatsOverlay_->setObjectName("imageStatsOverlay");
    imageStatsOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* statsLayout = new QVBoxLayout(imageStatsOverlay_);
    statsLayout->setContentsMargins(10, 8, 10, 8);
    statsLayout->setSpacing(0);
    imageStatsLabel_ = new QLabel(imageStatsOverlay_);
    imageStatsLabel_->setObjectName("imgInfoLabel");
    imageStatsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    imageStatsLabel_->setText(QString::fromUtf8("x: --  y: --\nmin: --  max: --  avg: --"));
    statsLayout->addWidget(imageStatsLabel_);
    imageStatsOverlay_->adjustSize();
    imageStatsOverlay_->raise();

    spectralProgressOverlay_ = new QWidget(this);
    spectralProgressOverlay_->setObjectName("spectralProgressOverlay");
    spectralProgressOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    spectralProgressOverlay_->setMinimumWidth(300);
    auto* progressLayout = new QVBoxLayout(spectralProgressOverlay_);
    progressLayout->setContentsMargins(12, 8, 12, 8);
    progressLayout->setSpacing(6);
    spectralProgressLabel_ = new QLabel(spectralProgressOverlay_);
    spectralProgressLabel_->setObjectName("spectralProgressLabel");
    spectralProgressLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    spectralProgressLabel_->setText(QString::fromUtf8("等待整帧完整数据"));
    spectralProgressBar_ = new QProgressBar(spectralProgressOverlay_);
    spectralProgressBar_->setObjectName("spectralProgressBar");
    spectralProgressBar_->setMinimumWidth(276);
    spectralProgressBar_->setRange(0, 100);
    spectralProgressBar_->setValue(0);
    spectralProgressBar_->setTextVisible(false);
    progressLayout->addWidget(spectralProgressLabel_);
    progressLayout->addWidget(spectralProgressBar_);
    spectralProgressOverlay_->adjustSize();
    spectralProgressOverlay_->hide();
    spectralProgressOverlay_->raise();

    connect(imageViewRaw_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[Raw16View] = pos;
        refreshImageStatsOverlay();
    });
    connect(imageViewSlice_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[SliceStitch16View] = pos;
        refreshImageStatsOverlay();
    });
    connect(imageViewSpectral_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[SpectralView] = pos;
    });
    connect(imageViewPlayback_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[PlaybackView] = pos;
        refreshImageStatsOverlay();
    });
    connect(imageViewSlice_, &ImageView::analysisLineAddRequested,
            this, &ViewerAreaWidget::sliceAnalysisLineAddRequested);
    connect(imageViewSlice_, &ImageView::analysisLineMoveRequested,
            this, &ViewerAreaWidget::sliceAnalysisLineMoveRequested);
    connect(imageViewSlice_, &ImageView::analysisLineDeleteRequested,
            this, &ViewerAreaWidget::sliceAnalysisLineDeleteRequested);

    updateChannelTabStyle();
    refreshImageStatsOverlay();
}

void ViewerAreaWidget::setCurrentChannel(int channel)
{
    if (channel < Raw16View || channel > PlaybackView) return;
    if (currentChannel_ == channel) {
        refreshImageStatsOverlay();
        return;
    }

    currentChannel_ = channel;
    viewerStack_->setCurrentIndex(channel);
    updateChannelTabStyle();
    refreshImageStatsOverlay();
    emit channelChanged(channel);
}

bool ViewerAreaWidget::hasChannelImage(int channel) const
{
    const ImageView* target = imageView(channel);
    return target && !target->currentImage().isNull();
}

ImageView* ViewerAreaWidget::imageView(int channel) const
{
    switch (channel) {
    case Raw16View: return imageViewRaw_;
    case SliceStitch16View: return imageViewSlice_;
    case SpectralView: return imageViewSpectral_;
    case PlaybackView: return imageViewPlayback_;
    default: return nullptr;
    }
}

void ViewerAreaWidget::renderFrame(int channel, int width, int height, int pixfmt, const QByteArray& data)
{
    ImageView* target = imageView(channel);
    if (!target) return;

    const QImage img = makeDisplayImage(width, height, pixfmt, data);
    if (!img.isNull()) {
        target->setImage(img);
    } else {
        static qint64 lastWarnMs = 0;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (lastWarnMs == 0 || nowMs - lastWarnMs >= 1000) {
            PLOGW << "ViewerAreaWidget: renderFrame produced null image"
                  << " channel=" << channel
                  << " width=" << width
                  << " height=" << height
                  << " pixfmt=" << pixfmt
                  << " dataBytes=" << data.size();
            lastWarnMs = nowMs;
        }
    }
}

void ViewerAreaWidget::setChannelImage(int channel, const QImage& image)
{
    ImageView* target = imageView(channel);
    if (!target || image.isNull()) return;
    target->setImage(image);
}

void ViewerAreaWidget::setImageStats(int channel, const ChannelImageStats& stats)
{
    if (channel == Raw16View) {
        rawImageStats_ = stats;
    } else if (channel == SliceStitch16View) {
        sliceImageStats_ = stats;
    } else if (channel == PlaybackView) {
        playbackImageStats_ = stats;
    }
    refreshImageStatsOverlay();
}

void ViewerAreaWidget::setSpectralImage(const QImage& image)
{
    if (imageViewSpectral_) {
        imageViewSpectral_->setImage(image);
    }
}

void ViewerAreaWidget::setSpectralNoSignal()
{
    if (imageViewSpectral_) {
        imageViewSpectral_->setNoSignal();
    }
}

void ViewerAreaWidget::setSpectralProgressVisible(bool visible, int percent, const QString& text)
{
    if (!spectralProgressOverlay_ || !spectralProgressLabel_ || !spectralProgressBar_) return;
    if (!visible || currentChannel_ != SpectralView) {
        spectralProgressOverlay_->hide();
        return;
    }

    spectralProgressLabel_->setText(text);
    spectralProgressBar_->setValue(qBound(0, percent, 100));
    positionSpectralProgressOverlay();
    spectralProgressOverlay_->show();
}

void ViewerAreaWidget::setSliceAnalysisOverlay(bool enabled, const QVector<SpectrumSampleLine>& lines)
{
    if (!imageViewSlice_) return;
    imageViewSlice_->setAnalysisOverlayEnabled(enabled);
    imageViewSlice_->setAnalysisLines(lines);
}

bool ViewerAreaWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == this && event->type() == QEvent::Resize) {
        positionImageStatsOverlay();
        positionSpectralProgressOverlay();
    }
    return QWidget::eventFilter(obj, event);
}

void ViewerAreaWidget::updateChannelTabStyle()
{
    chTabRaw16_->setProperty("active", currentChannel_ == Raw16View);
    chTabRaw16_->style()->polish(chTabRaw16_);
    chTabSlice_->setProperty("active", currentChannel_ == SliceStitch16View);
    chTabSlice_->style()->polish(chTabSlice_);
    chTabSpectral_->setProperty("active", currentChannel_ == SpectralView);
    chTabSpectral_->style()->polish(chTabSpectral_);
    chTabPlayback_->setProperty("active", currentChannel_ == PlaybackView);
    chTabPlayback_->style()->polish(chTabPlayback_);
}

QString ViewerAreaWidget::formatImageStatsText(const QPoint& cursorPos, const ChannelImageStats* stats) const
{
    const QString xText = cursorPos.x() >= 0 ? QString::number(cursorPos.x()) : QString::fromUtf8("--");
    const QString yText = cursorPos.y() >= 0 ? QString::number(cursorPos.y()) : QString::fromUtf8("--");

    QString minText = QString::fromUtf8("--");
    QString maxText = QString::fromUtf8("--");
    QString avgText = QString::fromUtf8("--");
    if (stats && stats->valid) {
        minText = QString::number(stats->min);
        maxText = QString::number(stats->max);
        avgText = QString::number(stats->avg, 'f', 1);
    }

    return QString::fromUtf8("x: %1  y: %2\nmin: %3  max: %4  avg: %5")
        .arg(xText, yText, minText, maxText, avgText);
}

void ViewerAreaWidget::positionImageStatsOverlay()
{
    if (!imageStatsOverlay_) return;
    imageStatsOverlay_->adjustSize();
    const int margin = 12;
    const int x = qMax(margin, width() - imageStatsOverlay_->width() - margin);
    const int y = qMax(margin, height() - imageStatsOverlay_->height() - margin);
    imageStatsOverlay_->move(x, y);
    imageStatsOverlay_->raise();
}

void ViewerAreaWidget::refreshImageStatsOverlay()
{
    if (!imageStatsOverlay_ || !imageStatsLabel_) return;

    const bool showOverlay = currentChannel_ == Raw16View
        || currentChannel_ == SliceStitch16View
        || currentChannel_ == PlaybackView;
    if (!showOverlay) {
        imageStatsOverlay_->hide();
        return;
    }

    const QPoint cursorPos = cursorImagePos_[static_cast<std::size_t>(currentChannel_)];
    const ChannelImageStats* stats = &rawImageStats_;
    if (currentChannel_ == SliceStitch16View) stats = &sliceImageStats_;
    else if (currentChannel_ == PlaybackView) stats = &playbackImageStats_;
    imageStatsLabel_->setText(formatImageStatsText(cursorPos, stats));
    imageStatsOverlay_->adjustSize();
    positionImageStatsOverlay();
    imageStatsOverlay_->show();
}

void ViewerAreaWidget::positionSpectralProgressOverlay()
{
    if (!spectralProgressOverlay_) return;
    spectralProgressOverlay_->adjustSize();
    const int margin = 12;
    int x = (width() - spectralProgressOverlay_->width()) / 2;
    if (x < margin) x = margin;
    if (x + spectralProgressOverlay_->width() + margin > width()) {
        x = qMax(margin, width() - spectralProgressOverlay_->width() - margin);
    }
    int y = height() - spectralProgressOverlay_->height() - margin;
    if (y < margin) y = margin;
    spectralProgressOverlay_->move(x, y);
    spectralProgressOverlay_->raise();
}
