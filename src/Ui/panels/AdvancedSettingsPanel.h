#pragma once

#include <QWidget>

class CameraPanel;
class CollectPanel;
class ConnectionPanel;
class DeviceClient;

class AdvancedSettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit AdvancedSettingsPanel(DeviceClient* dev, QWidget* parent = nullptr);

    ConnectionPanel* connection() const { return connectionPanel_; }
    CameraPanel* camera() const { return cameraPanel_; }
    CollectPanel* collect() const { return collectPanel_; }

private:
    ConnectionPanel* connectionPanel_ = nullptr;
    CameraPanel* cameraPanel_ = nullptr;
    CollectPanel* collectPanel_ = nullptr;
};
