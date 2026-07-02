#include "DataAcquisitionPanel.h"

#include "CollectPanel.h"
#include "MirrorPanel.h"
#include "SpectralPanel.h"
#include "StreamPanel.h"

#include <QVBoxLayout>

DataAcquisitionPanel::DataAcquisitionPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    streamPanel_ = new StreamPanel(dev, this);
    streamPanel_->setObjectName(QStringLiteral("dataAcquisitionStreamPanel"));
    collectPanel_ = new CollectPanel(dev, CollectPanel::OperationOnly, this);
    collectPanel_->setObjectName(QStringLiteral("dataAcquisitionCollectPanel"));
    spectralPanel_ = new SpectralPanel(this);
    spectralPanel_->setObjectName(QStringLiteral("dataAcquisitionSpectralPanel"));
    mirrorPanel_ = new MirrorPanel(dev, this);
    mirrorPanel_->setObjectName(QStringLiteral("dataAcquisitionMirrorPanel"));

    root->addWidget(streamPanel_);
    root->addWidget(collectPanel_);
    root->addWidget(spectralPanel_);
    root->addWidget(mirrorPanel_);
    root->addStretch();
}
