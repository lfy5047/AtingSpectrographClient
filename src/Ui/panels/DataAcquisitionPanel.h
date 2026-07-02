#pragma once

#include <QWidget>

class CollectPanel;
class DeviceClient;
class MirrorPanel;
class SpectralPanel;
class StreamPanel;

class DataAcquisitionPanel : public QWidget {
    Q_OBJECT
public:
    explicit DataAcquisitionPanel(DeviceClient* dev, QWidget* parent = nullptr);

    StreamPanel* stream() const { return streamPanel_; }
    CollectPanel* collect() const { return collectPanel_; }
    SpectralPanel* spectral() const { return spectralPanel_; }
    MirrorPanel* mirror() const { return mirrorPanel_; }

private:
    StreamPanel* streamPanel_ = nullptr;
    CollectPanel* collectPanel_ = nullptr;
    SpectralPanel* spectralPanel_ = nullptr;
    MirrorPanel* mirrorPanel_ = nullptr;
};
