#pragma once

#include <QWidget>

class CameraPanel;
class CollectPanel;
class ConnectionPanel;
class DeviceClient;
class QSpinBox;

class AdvancedSettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit AdvancedSettingsPanel(DeviceClient* dev, QWidget* parent = nullptr);

    ConnectionPanel* connection() const { return connectionPanel_; }
    CameraPanel* camera() const { return cameraPanel_; }
    CollectPanel* collect() const { return collectPanel_; }
    int rawStretchWidth() const;
    int rawCropLeftColumns() const;

signals:
    void rawStretchCropChanged(int stretchWidth, int cropLeftColumns);

private:
    ConnectionPanel* connectionPanel_ = nullptr;
    CameraPanel* cameraPanel_ = nullptr;
    CollectPanel* collectPanel_ = nullptr;
    QSpinBox* rawStretchWidthSpin_ = nullptr;
    QSpinBox* rawCropLeftColumnsSpin_ = nullptr;
};
