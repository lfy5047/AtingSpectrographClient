#pragma once

#include <QObject>
#include <QUdpSocket>
#include "FrameAssembler.h"

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
    double  fps() const { return fps_; }

signals:
    void frameReady(int channel, int width, int height, int pixfmt, QByteArray data);

private slots:
    void onReadyRead();
    void updateFps();

private:
    QUdpSocket sock_;
    FrameAssembler assembler_;
    bool bound_ = false;
    quint16 port_ = 0;

    quint64 lastFrameCount_ = 0;
    double  fps_ = 0.0;
    QTimer  fpsTimer_;
};
