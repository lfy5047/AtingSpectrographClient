#include "SpectralSegmentTestPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <cstdlib>

SpectralSegmentTestPanel::SpectralSegmentTestPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("spectralSegmentTestPanel");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(12);

    auto* instruction = new QLabel(
        QString::fromUtf8("在 Raw16 图像中拖动两条竖线，可实时测量它们在 X 轴方向的间距。"),
        this);
    instruction->setObjectName("spectralSegmentInstruction");
    instruction->setWordWrap(true);
    root->addWidget(instruction);

    auto* resultGroup = new QGroupBox(QString::fromUtf8("测量结果"), this);
    auto* resultForm = new QFormLayout(resultGroup);
    resultForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    firstXValue_ = new QLabel(QStringLiteral("--"), resultGroup);
    firstXValue_->setObjectName("spectralSegmentFirstX");
    firstXValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resultForm->addRow(QString::fromUtf8("竖线 1 X 坐标"), firstXValue_);

    secondXValue_ = new QLabel(QStringLiteral("--"), resultGroup);
    secondXValue_->setObjectName("spectralSegmentSecondX");
    secondXValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resultForm->addRow(QString::fromUtf8("竖线 2 X 坐标"), secondXValue_);

    distanceValue_ = new QLabel(QStringLiteral("--"), resultGroup);
    distanceValue_->setObjectName("spectralSegmentDistance");
    distanceValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resultForm->addRow(QString::fromUtf8("X 方向距离"), distanceValue_);

    root->addWidget(resultGroup);
    root->addStretch();
}

void SpectralSegmentTestPanel::setLinePositions(int firstX, int secondX)
{
    firstXValue_->setText(formatPosition(firstX));
    secondXValue_->setText(formatPosition(secondX));
    distanceValue_->setText(firstX >= 0 && secondX >= 0
                                ? QStringLiteral("%1 px").arg(std::abs(firstX - secondX))
                                : QStringLiteral("--"));
}

QString SpectralSegmentTestPanel::formatPosition(int x)
{
    return x >= 0 ? QStringLiteral("%1 px").arg(x) : QStringLiteral("--");
}
