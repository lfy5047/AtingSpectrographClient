#pragma once

#include <QObject>
#include <QByteArray>
#include <QtGlobal>

class SpectrumAnalysisPanel;
class SpectrumCurveDialog;
class ViewerAreaWidget;
class QWidget;

class SpectrumAnalysisCoordinator : public QObject {
    Q_OBJECT
public:
    SpectrumAnalysisCoordinator(SpectrumAnalysisPanel* panel,
                                ViewerAreaWidget* viewerArea,
                                QWidget* dialogParent,
                                QObject* parent = nullptr);

    void setActive(bool active);
    void setCurrentChannel(int channel);
    void setLatestSliceFrame(int width, int height, quint64 frameId, const QByteArray& data);
    void openDialog();
    void refreshOverlay();
    void forceRefreshCurves();

private:
    void setupConnections();
    void updateCurveData(bool force);

    SpectrumAnalysisPanel* panel_ = nullptr;
    ViewerAreaWidget* viewerArea_ = nullptr;
    QWidget* dialogParent_ = nullptr;
    SpectrumCurveDialog* dialog_ = nullptr;
    QByteArray latestSliceData_;
    int latestSliceWidth_ = 0;
    int latestSliceHeight_ = 0;
    quint64 latestSliceFrameId_ = 0;
    quint64 lastCurveFrameId_ = 0;
    bool active_ = false;
    int currentChannel_ = 0;
};
