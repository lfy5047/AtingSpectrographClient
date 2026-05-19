#pragma once

#include <QWidget>
#include <QLabel>

class DashboardPanel : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPanel(QWidget* parent = nullptr);

    void setConnectionInfo(const QString& host, quint16 tcpPort, quint16 udpPort,
                           bool connected, const QString& version = QString());
    void setStreamStats(double fpsRaw, double fpsSlice, quint64 frames, quint64 dropped);
    void setMirrorInfo(double angle, bool moving);
    void setUptime(const QString& uptime);
    void setSubscribedChannels(const QString& channels);

private:
    void setupUi();

    // Device overview
    QLabel* dashTcp_   = nullptr;
    QLabel* dashUdp_   = nullptr;
    QLabel* dashConn_  = nullptr;
    QLabel* dashVer_   = nullptr;

    // Stream stats
    QLabel* dashFpsRaw_   = nullptr;
    QLabel* dashFpsSlice_ = nullptr;
    QLabel* dashFrames_   = nullptr;
    QLabel* dashDropped_  = nullptr;

    // Mirror
    QLabel* dashAngle_  = nullptr;
    QLabel* dashMoving_ = nullptr;

    // System
    QLabel* dashVerSys_ = nullptr;
    QLabel* dashUptime_ = nullptr;
    QLabel* dashChannels_ = nullptr;
};
