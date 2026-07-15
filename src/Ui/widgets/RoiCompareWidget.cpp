#include "RoiCompareWidget.h"

#include "ImageView.h"
#include "Protocol.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QString sizeText(const RoiSnapshot& snapshot, bool valid)
{
    return valid ? QString::fromUtf8("尺寸: %1x%2").arg(snapshot.width).arg(snapshot.height)
                 : QString::fromUtf8("尺寸: --");
}

QString statsText(const RoiSnapshot& snapshot, bool valid)
{
    if (!valid) return QStringLiteral("DN  Min: --  Avg: --  Max: --");
    return QStringLiteral("DN  Min: %1  Avg: %2  Max: %3")
        .arg(snapshot.stats.min)
        .arg(snapshot.stats.avg, 0, 'f', 1)
        .arg(snapshot.stats.max);
}

QString coordinateText(const QPoint& pos, const ImageView* view)
{
    quint16 dn = 0;
    if (pos.x() < 0 || pos.y() < 0 || !view || !view->pixelDnAt(pos, &dn)) {
        return QString::fromUtf8("坐标: --  DN: --");
    }
    return QStringLiteral("X: %1  Y: %2  DN: %3").arg(pos.x()).arg(pos.y()).arg(dn);
}

} // namespace

RoiCompareWidget::RoiCompareWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("roiCompareWidget"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    commonRangeCheck_ = new QCheckBox(QString::fromUtf8("统一灰度范围"), this);
    commonRangeCheck_->setObjectName(QStringLiteral("roiCommonRangeCheck"));
    commonRangeCheck_->setChecked(true);
    toolbar->addWidget(commonRangeCheck_);
    auto* hint = new QLabel(
        QString::fromUtf8("对照全幅图与 ROI 图的尺寸和内容；移动鼠标可查看以各图左上角为原点的像素坐标。"),
        this);
    hint->setWordWrap(true);
    toolbar->addWidget(hint, 1);
    root->addLayout(toolbar);

    auto* cards = new QHBoxLayout();
    cards->setSpacing(8);

    auto createCard = [this, cards](const QString& title, const QString& prefix,
                                    QLabel** sizeLabel, QLabel** statsLabel,
                                    QLabel** coordinateLabel,
                                    ImageView** view) {
        auto* group = new QGroupBox(title, this);
        group->setObjectName(prefix + QStringLiteral("Card"));
        auto* layout = new QVBoxLayout(group);
        layout->setContentsMargins(6, 8, 6, 6);
        layout->setSpacing(4);

        *sizeLabel = new QLabel(QString::fromUtf8("尺寸: --"), group);
        (*sizeLabel)->setObjectName(prefix + QStringLiteral("SizeLabel"));
        (*sizeLabel)->setAlignment(Qt::AlignCenter);
        *statsLabel = new QLabel(QStringLiteral("DN  Min: --  Avg: --  Max: --"), group);
        (*statsLabel)->setObjectName(prefix + QStringLiteral("StatsLabel"));
        (*statsLabel)->setAlignment(Qt::AlignCenter);
        (*statsLabel)->setWordWrap(true);
        (*statsLabel)->setProperty("readoutSm", true);
        *coordinateLabel = new QLabel(QString::fromUtf8("坐标: --  DN: --"), group);
        (*coordinateLabel)->setObjectName(prefix + QStringLiteral("CoordinateLabel"));
        (*coordinateLabel)->setAlignment(Qt::AlignCenter);
        (*coordinateLabel)->setProperty("readoutSm", true);
        *view = new ImageView(group);
        (*view)->setObjectName(prefix + QStringLiteral("ImageView"));
        (*view)->setMinimumSize(220, 220);

        layout->addWidget(*sizeLabel);
        layout->addWidget(*statsLabel);
        layout->addWidget(*coordinateLabel);
        layout->addWidget(*view, 1);
        cards->addWidget(group, 1);
    };

    createCard(QString::fromUtf8("全幅基准"), QStringLiteral("roiFullFrame"),
               &fullFrameSizeLabel_, &fullFrameStatsLabel_, &fullFrameCoordinateLabel_,
               &fullFrameView_);
    createCard(QString::fromUtf8("ROI 区域"), QStringLiteral("roiTarget"),
               &roiSizeLabel_, &roiStatsLabel_, &roiCoordinateLabel_, &roiView_);
    root->addLayout(cards, 1);

    connect(commonRangeCheck_, &QCheckBox::toggled,
            this, [this](bool) { renderSnapshots(); });
    connect(fullFrameView_, &ImageView::cursorImagePosChanged,
            this, [this](const QPoint& pos) {
                fullFrameCoordinateLabel_->setText(coordinateText(pos, fullFrameView_));
            });
    connect(roiView_, &ImageView::cursorImagePosChanged,
            this, [this](const QPoint& pos) {
                roiCoordinateLabel_->setText(coordinateText(pos, roiView_));
            });
    clearSnapshots();
}

void RoiCompareWidget::clearSnapshots()
{
    fullFrameSnapshot_ = RoiSnapshot();
    roiSnapshot_ = RoiSnapshot();
    fullFrameValid_ = false;
    roiValid_ = false;
    fullFrameView_->resetView();
    roiView_->resetView();
    fullFrameView_->setNoSignal();
    roiView_->setNoSignal();
    updateLabels();
}

void RoiCompareWidget::setFullFrameSnapshot(const RoiSnapshot& snapshot)
{
    if (!snapshotIsValid(snapshot)) return;
    fullFrameSnapshot_ = snapshot;
    fullFrameValid_ = true;
    updateLabels();
    renderSnapshots();
}

void RoiCompareWidget::setRoiSnapshot(const RoiSnapshot& snapshot)
{
    if (!snapshotIsValid(snapshot)) return;
    roiSnapshot_ = snapshot;
    roiValid_ = true;
    updateLabels();
    renderSnapshots();
}

bool RoiCompareWidget::snapshotIsValid(const RoiSnapshot& snapshot)
{
    return snapshot.width > 0 && snapshot.height > 0 && snapshot.stats.valid
        && snapshot.data.size() >= snapshot.width * snapshot.height * 2;
}

void RoiCompareWidget::renderSnapshots()
{
    if (!fullFrameValid_ && !roiValid_) return;

    quint16 commonMin = 0xFFFF;
    quint16 commonMax = 0;
    if (fullFrameValid_) {
        commonMin = qMin(commonMin, fullFrameSnapshot_.stats.min);
        commonMax = qMax(commonMax, fullFrameSnapshot_.stats.max);
    }
    if (roiValid_) {
        commonMin = qMin(commonMin, roiSnapshot_.stats.min);
        commonMax = qMax(commonMax, roiSnapshot_.stats.max);
    }

    auto render = [this, commonMin, commonMax](const RoiSnapshot& snapshot,
                                               bool valid, ImageView* view) {
        if (!valid) return;
        const quint16 displayMin = commonRangeCheck_->isChecked()
            ? commonMin : snapshot.stats.min;
        const quint16 displayMax = commonRangeCheck_->isChecked()
            ? commonMax : snapshot.stats.max;
        const QImage image = makeMono16DisplayImage(snapshot.width, snapshot.height,
                                                    snapshot.data, displayMin, displayMax);
        if (!image.isNull()) view->setImage(image, cli::proto::Mono16, snapshot.data);
    };
    render(fullFrameSnapshot_, fullFrameValid_, fullFrameView_);
    render(roiSnapshot_, roiValid_, roiView_);
}

void RoiCompareWidget::updateLabels()
{
    fullFrameSizeLabel_->setText(sizeText(fullFrameSnapshot_, fullFrameValid_));
    fullFrameStatsLabel_->setText(statsText(fullFrameSnapshot_, fullFrameValid_));
    roiSizeLabel_->setText(sizeText(roiSnapshot_, roiValid_));
    roiStatsLabel_->setText(statsText(roiSnapshot_, roiValid_));
}
