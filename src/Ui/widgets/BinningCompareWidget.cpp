#include "BinningCompareWidget.h"

#include "ImageView.h"
#include "Protocol.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <cstdlib>

namespace {

const int kFactors[] = {1, 2, 4};

QString modeText(int factor)
{
    return QStringLiteral("%1x%1").arg(factor);
}

} // namespace

BinningCompareWidget::BinningCompareWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("binningCompareWidget"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* toolbar = new QHBoxLayout();
    commonRangeCheck_ = new QCheckBox(QString::fromUtf8("统一灰度范围"), this);
    commonRangeCheck_->setObjectName(QStringLiteral("binningCommonRangeCheck"));
    commonRangeCheck_->setChecked(true);
    toolbar->addWidget(commonRangeCheck_);
    auto* measurementHint = new QLabel(
        QString::fromUtf8("拖动两条测量线，使其贴合标准靶标特征边缘"), this);
    measurementHint->setObjectName(QStringLiteral("binningMeasurementHintLabel"));
    measurementHint->setWordWrap(true);
    measurementHint->setMinimumWidth(0);
    measurementHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    measurementHint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    toolbar->addWidget(measurementHint, 1);
    root->addLayout(toolbar);

    auto* cards = new QHBoxLayout();
    cards->setSpacing(8);
    for (int index = 0; index < 3; ++index) {
        auto* group = new QGroupBox(modeText(kFactors[index]), this);
        group->setObjectName(QStringLiteral("binningCompareCard%1").arg(kFactors[index]));
        auto* layout = new QVBoxLayout(group);
        layout->setContentsMargins(6, 8, 6, 6);
        layout->setSpacing(4);

        sizeLabels_[static_cast<std::size_t>(index)] = new QLabel(QString::fromUtf8("尺寸: --"), group);
        sizeLabels_[static_cast<std::size_t>(index)]->setObjectName(
            QStringLiteral("binningSizeLabel%1").arg(kFactors[index]));
        sizeLabels_[static_cast<std::size_t>(index)]->setAlignment(Qt::AlignCenter);
        pixelRatioLabels_[static_cast<std::size_t>(index)] = new QLabel(
            QString::fromUtf8("总像素比: --"), group);
        pixelRatioLabels_[static_cast<std::size_t>(index)]->setObjectName(
            QStringLiteral("binningPixelRatioLabel%1").arg(kFactors[index]));
        pixelRatioLabels_[static_cast<std::size_t>(index)]->setAlignment(Qt::AlignCenter);
        pixelRatioLabels_[static_cast<std::size_t>(index)]->setProperty("readoutSm", true);
        dnStatsLabels_[static_cast<std::size_t>(index)] = new QLabel(
            QStringLiteral("DN  Min: --  Avg: --  Max: --"), group);
        dnStatsLabels_[static_cast<std::size_t>(index)]->setObjectName(
            QStringLiteral("binningDnStatsLabel%1").arg(kFactors[index]));
        dnStatsLabels_[static_cast<std::size_t>(index)]->setAlignment(Qt::AlignCenter);
        dnStatsLabels_[static_cast<std::size_t>(index)]->setWordWrap(true);
        dnStatsLabels_[static_cast<std::size_t>(index)]->setProperty("readoutSm", true);

        auto* view = new ImageView(group);
        view->setObjectName(QStringLiteral("binningImageView%1").arg(kFactors[index]));
        view->setMinimumSize(160, 180);
        view->setPixelMeasureOrientation(measurementOrientation_);
        view->setPixelMeasureEnabled(true);
        imageViews_[static_cast<std::size_t>(index)] = view;

        layout->addWidget(sizeLabels_[static_cast<std::size_t>(index)]);
        layout->addWidget(pixelRatioLabels_[static_cast<std::size_t>(index)]);
        layout->addWidget(dnStatsLabels_[static_cast<std::size_t>(index)]);
        layout->addWidget(view, 1);
        cards->addWidget(group, 1);

        connect(view, &ImageView::pixelMeasureLineMoveRequested,
                this, [this, index](int lineIndex, int position) {
            if (lineIndex < 0 || lineIndex >= 2) return;
            measurementLines_[static_cast<std::size_t>(index)][static_cast<std::size_t>(lineIndex)] = position;
            imageViews_[static_cast<std::size_t>(index)]->setPixelMeasureLines(
                measurementLines_[static_cast<std::size_t>(index)][0],
                measurementLines_[static_cast<std::size_t>(index)][1]);
            emit measurementChanged(kFactors[index], measurementWidth(kFactors[index]));
        });
    }
    root->addLayout(cards, 1);

    connect(commonRangeCheck_, &QCheckBox::toggled,
            this, [this](bool) { renderSnapshots(); });
}

void BinningCompareWidget::clearSnapshots()
{
    snapshotValid_.fill(false);
    measurementLines_ = {{{{-1, -1}}, {{-1, -1}}, {{-1, -1}}}};
    for (int index = 0; index < 3; ++index) {
        snapshots_[static_cast<std::size_t>(index)] = BinningSnapshot();
        imageViews_[static_cast<std::size_t>(index)]->resetView();
        imageViews_[static_cast<std::size_t>(index)]->setNoSignal();
        imageViews_[static_cast<std::size_t>(index)]->setPixelMeasureLines(-1, -1);
        updateCardLabels(index);
    }
}

void BinningCompareWidget::setSnapshot(const BinningSnapshot& snapshot)
{
    const int index = indexForFactor(snapshot.factor);
    if (index < 0 || snapshot.width <= 0 || snapshot.height <= 0 || !snapshot.stats.valid) return;
    if (snapshot.data.size() < snapshot.width * snapshot.height * 2) return;

    snapshots_[static_cast<std::size_t>(index)] = snapshot;
    snapshotValid_[static_cast<std::size_t>(index)] = true;
    initializeMeasurement(index);
    updateCardLabels(index);
    renderSnapshots();
}

void BinningCompareWidget::setMeasurementOrientation(Qt::Orientation orientation)
{
    if (measurementOrientation_ == orientation) return;
    measurementOrientation_ = orientation;
    for (int index = 0; index < 3; ++index) {
        imageViews_[static_cast<std::size_t>(index)]->setPixelMeasureOrientation(orientation);
        measurementLines_[static_cast<std::size_t>(index)] = {{-1, -1}};
        if (snapshotValid_[static_cast<std::size_t>(index)]) initializeMeasurement(index);
    }
}

ImageView* BinningCompareWidget::imageViewForFactor(int factor) const
{
    const int index = indexForFactor(factor);
    return index >= 0 ? imageViews_[static_cast<std::size_t>(index)] : nullptr;
}

int BinningCompareWidget::measurementWidth(int factor) const
{
    const int index = indexForFactor(factor);
    if (index < 0) return -1;
    const auto& lines = measurementLines_[static_cast<std::size_t>(index)];
    return lines[0] >= 0 && lines[1] >= 0 ? std::abs(lines[0] - lines[1]) : -1;
}

int BinningCompareWidget::indexForFactor(int factor)
{
    if (factor == 1) return 0;
    if (factor == 2) return 1;
    if (factor == 4) return 2;
    return -1;
}

void BinningCompareWidget::initializeMeasurement(int index)
{
    if (index < 0 || index >= 3 || !snapshotValid_[static_cast<std::size_t>(index)]) return;
    const BinningSnapshot& snapshot = snapshots_[static_cast<std::size_t>(index)];
    const int extent = measurementOrientation_ == Qt::Horizontal ? snapshot.width : snapshot.height;
    if (extent <= 0) return;
    const int last = extent - 1;
    measurementLines_[static_cast<std::size_t>(index)] = {{last / 3, (last * 2) / 3}};
    imageViews_[static_cast<std::size_t>(index)]->setPixelMeasureOrientation(measurementOrientation_);
    imageViews_[static_cast<std::size_t>(index)]->setPixelMeasureLines(
        measurementLines_[static_cast<std::size_t>(index)][0],
        measurementLines_[static_cast<std::size_t>(index)][1]);
}

void BinningCompareWidget::renderSnapshots()
{
    quint16 commonMin = 0xFFFF;
    quint16 commonMax = 0;
    bool any = false;
    for (int index = 0; index < 3; ++index) {
        if (!snapshotValid_[static_cast<std::size_t>(index)]) continue;
        const ChannelImageStats& stats = snapshots_[static_cast<std::size_t>(index)].stats;
        commonMin = qMin(commonMin, stats.min);
        commonMax = qMax(commonMax, stats.max);
        any = true;
    }
    if (!any) return;

    for (int index = 0; index < 3; ++index) {
        if (!snapshotValid_[static_cast<std::size_t>(index)]) continue;
        const BinningSnapshot& snapshot = snapshots_[static_cast<std::size_t>(index)];
        const quint16 displayMin = commonRangeCheck_->isChecked() ? commonMin : snapshot.stats.min;
        const quint16 displayMax = commonRangeCheck_->isChecked() ? commonMax : snapshot.stats.max;
        const QImage image = makeMono16DisplayImage(snapshot.width, snapshot.height,
                                                    snapshot.data, displayMin, displayMax);
        if (!image.isNull()) imageViews_[static_cast<std::size_t>(index)]->setImage(image);
    }
}

void BinningCompareWidget::updateCardLabels(int index)
{
    if (index < 0 || index >= 3) return;
    if (!snapshotValid_[static_cast<std::size_t>(index)]) {
        sizeLabels_[static_cast<std::size_t>(index)]->setText(QString::fromUtf8("尺寸: --"));
        pixelRatioLabels_[static_cast<std::size_t>(index)]->setText(
            QString::fromUtf8("总像素比: --"));
        dnStatsLabels_[static_cast<std::size_t>(index)]->setText(
            QStringLiteral("DN  Min: --  Avg: --  Max: --"));
        return;
    }

    const BinningSnapshot& snapshot = snapshots_[static_cast<std::size_t>(index)];
    double pixelRatio = 100.0 / (snapshot.factor * snapshot.factor);
    if (snapshotValid_[0] && snapshots_[0].width > 0 && snapshots_[0].height > 0) {
        const double baselinePixels = static_cast<double>(snapshots_[0].width) * snapshots_[0].height;
        pixelRatio = baselinePixels > 0.0
            ? 100.0 * snapshot.width * snapshot.height / baselinePixels : pixelRatio;
    }
    sizeLabels_[static_cast<std::size_t>(index)]->setText(
        QString::fromUtf8("尺寸: %1x%2").arg(snapshot.width).arg(snapshot.height));
    pixelRatioLabels_[static_cast<std::size_t>(index)]->setText(
        QString::fromUtf8("总像素比: %1%").arg(pixelRatio, 0, 'f', 2));
    dnStatsLabels_[static_cast<std::size_t>(index)]->setText(
        QStringLiteral("DN  Min: %1  Avg: %2  Max: %3")
            .arg(snapshot.stats.min)
            .arg(snapshot.stats.avg, 0, 'f', 1)
            .arg(snapshot.stats.max));
}
