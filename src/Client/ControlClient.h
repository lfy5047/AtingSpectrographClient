#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include <functional>
#include "Protocol.h"
#include "RpcTypes.h"

class ControlClient : public QObject {
    Q_OBJECT
public:
    using ResponseHandler = std::function<void(const RpcResult&)>;

    explicit ControlClient(QObject* parent = nullptr);
    ~ControlClient() override;

    void connectTo(const QString& host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const { return connected_; }
    QString peerAddress() const;

    void request(const QString& cmd,
                 const nlohmann::json& params,
                 ResponseHandler cb,
                 int timeout_ms = 3000);

signals:
    void connectionChanged(bool connected, QString peerIp);
    void eventReceived(QString evt, nlohmann::json data);
    void rawLog(QString line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);
    void tryReconnect();

private:
    struct Pending {
        ResponseHandler cb;
        QTimer* timer = nullptr;
    };

    void sendFrame(cli::proto::MsgType type, uint32_t seq, const nlohmann::json& payload);
    void tryConsume();
    void failAllPending(const QString& reason);

    QTcpSocket* sock_ = nullptr;
    QByteArray  rx_;
    bool        connected_ = false;

    uint32_t nextSeq_ = 1;
    QHash<uint32_t, Pending> pending_;

    // reconnect
    QString  host_;
    quint16  port_ = 0;
    bool     wantConnect_ = false;
    QTimer*  reconnTimer_ = nullptr;
    int      reconnDelay_ = 1000;
};
