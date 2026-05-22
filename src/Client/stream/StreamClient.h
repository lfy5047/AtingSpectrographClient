#pragma once

#include <QHash>
#include <QObject>
#include <QUdpSocket>
#include "FrameAssembler.h"
#include "StreamFrame.h"

class StreamClient : public QObject {
    Q_OBJECT
public:
    explicit StreamClient(QObject* parent = nullptr);

    bool bind(quint16 localPort);
    void close();
    bool isBound() const { return bound_; }
    quint16 localPort() const { return port_; }

    FrameAssembler* assembler() { return &assembler_; }

    quint64 framesReceived() const { return assembler_.framesReceived(); }
    quint64 framesDropped()  const { return assembler_.framesDropped(); }
    quint64 framesReceived(int channel) const { return framesByChannel_.value(channel, 0); }
    double  fps() const { return fps_; }
    double  fps(int channel) const { return fpsByChannel_.value(channel, 0.0); }

signals:
    void frameReady(StreamFrame frame);
    void statsUpdated();

private slots:
    void onReadyRead();
    void updateFps();

private:
    void logDropRateLimited(const char* reason, const cli::proto::StreamHeader* hdr, int datagramBytes);

    QUdpSocket sock_;
    FrameAssembler assembler_;
    bool bound_ = false;
    quint16 port_ = 0;

    quint64 lastFrameCount_ = 0;
    double  fps_ = 0.0;
    QHash<int, quint64> framesByChannel_;
    QHash<int, quint64> lastFrameCountByChannel_;
    QHash<int, double> fpsByChannel_;
    QTimer  fpsTimer_;
    qint64 dropLogLastMs_ = 0;
    quint64 dropLogSuppressed_ = 0;
};
