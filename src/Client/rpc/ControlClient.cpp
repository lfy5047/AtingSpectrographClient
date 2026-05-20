#include "ControlClient.h"
#include <cstring>
#include <QHostAddress>
#include <QNetworkProxy>

using namespace cli::proto;

ControlClient::ControlClient(QObject* parent)
    : QObject(parent)
{
    sock_ = new QTcpSocket(this);
    // 直连设备；避免继承系统代理（如 HTTP 代理）导致 “proxy type is invalid for this operation”
    sock_->setProxy(QNetworkProxy::NoProxy);
    connect(sock_, &QTcpSocket::connected,    this, &ControlClient::onConnected);
    connect(sock_, &QTcpSocket::disconnected, this, &ControlClient::onDisconnected);
    connect(sock_, &QTcpSocket::readyRead,    this, &ControlClient::onReadyRead);
    connect(sock_, &QAbstractSocket::errorOccurred,
            this, &ControlClient::onError);

    reconnTimer_ = new QTimer(this);
    reconnTimer_->setSingleShot(true);
    connect(reconnTimer_, &QTimer::timeout, this, &ControlClient::tryReconnect);
}

ControlClient::~ControlClient()
{
    wantConnect_ = false;
    reconnTimer_->stop();
    failAllPending("destroyed");
    sock_->abort();
}

void ControlClient::connectTo(const QString& host, quint16 port)
{
    host_ = host;
    port_ = port;
    wantConnect_ = true;
    reconnDelay_ = 1000;
    reconnTimer_->stop();

    sock_->abort();
    rx_.clear();
    sock_->connectToHost(host, port);
    emit rawLog(QString("[TCP] connecting to %1:%2").arg(host).arg(port));
}

void ControlClient::disconnectFromHost()
{
    wantConnect_ = false;
    reconnTimer_->stop();
    failAllPending("user disconnect");
    sock_->abort();
}

QString ControlClient::peerAddress() const
{
    return sock_->peerAddress().toString();
}

void ControlClient::onConnected()
{
    connected_ = true;
    reconnDelay_ = 1000;
    QString ip = sock_->peerAddress().toString();
    emit rawLog(QString("[TCP] connected to %1").arg(ip));
    emit connectionChanged(true, ip);
}

void ControlClient::onDisconnected()
{
    bool was = connected_;
    connected_ = false;
    failAllPending("disconnected");
    rx_.clear();

    if (was) {
        emit rawLog("[TCP] disconnected");
        emit connectionChanged(false, QString());
    }

    if (wantConnect_) {
        reconnTimer_->start(reconnDelay_);
        reconnDelay_ = qMin(reconnDelay_ * 2, 5000);
        emit rawLog(QString("[TCP] reconnect in %1ms").arg(reconnDelay_));
    }
}

void ControlClient::onError(QAbstractSocket::SocketError)
{
    emit rawLog(QString("[TCP] error: %1").arg(sock_->errorString()));
}

void ControlClient::tryReconnect()
{
    if (!wantConnect_) return;
    sock_->abort();
    rx_.clear();
    sock_->connectToHost(host_, port_);
    emit rawLog(QString("[TCP] reconnecting to %1:%2").arg(host_).arg(port_));
}

void ControlClient::onReadyRead()
{
    rx_.append(sock_->readAll());
    tryConsume();
}

void ControlClient::tryConsume()
{
    const int hdrSize = static_cast<int>(sizeof(CtrlHeader));
    while (rx_.size() >= hdrSize) {
        CtrlHeader hdr;
        std::memcpy(&hdr, rx_.constData(), sizeof(hdr));

        if (hdr.magic != kCtrlMagic || hdr.version != kProtoVersion) {
            emit rawLog("[TCP] bad header, dropping connection");
            sock_->abort();
            return;
        }
        if (hdr.payload_len > kMaxCtrlPayload) {
            emit rawLog("[TCP] payload too large, dropping");
            sock_->abort();
            return;
        }

        int total = hdrSize + static_cast<int>(hdr.payload_len);
        if (rx_.size() < total) return;

        nlohmann::json body;
        if (hdr.payload_len > 0) {
            try {
                body = nlohmann::json::parse(
                    rx_.constData() + hdrSize,
                    rx_.constData() + total);
            } catch (...) {
                emit rawLog("[TCP] bad JSON payload");
                rx_.remove(0, total);
                continue;
            }
        }
        rx_.remove(0, total);

        MsgType type = static_cast<MsgType>(hdr.type);

        if (type == MsgType::Response || type == MsgType::Error) {
            auto it = pending_.find(hdr.seq);
            if (it != pending_.end()) {
                Pending p = it.value();
                pending_.erase(it);
                if (p.timer) { p.timer->stop(); p.timer->deleteLater(); }

                RpcResult r;
                r.ok   = body.value("ok", false);
                r.data = body.value("data", nlohmann::json());
                r.code = body.value("code", 0);
                r.msg  = QString::fromStdString(body.value("msg", std::string()));

                emit rawLog(QString("[TCP] resp seq=%1 ok=%2").arg(hdr.seq).arg(r.ok));
                if (p.cb) p.cb(r);
            }
        } else if (type == MsgType::Event) {
            QString evt = QString::fromStdString(body.value("evt", std::string()));
            nlohmann::json data = body.value("data", nlohmann::json());
            emit rawLog(QString("[TCP] event: %1").arg(evt));
            emit eventReceived(evt, data);
        }
    }
}

void ControlClient::request(const QString& cmd,
                             const nlohmann::json& params,
                             ResponseHandler cb,
                             int timeout_ms)
{
    if (!connected_) {
        if (cb) cb(RpcResult::disconnected());
        return;
    }

    uint32_t seq = nextSeq_++;
    nlohmann::json payload = {{"cmd", cmd.toStdString()}, {"params", params}};

    Pending p;
    p.cb = cb;
    p.timer = new QTimer(this);
    p.timer->setSingleShot(true);
    connect(p.timer, &QTimer::timeout, this, [this, seq]() {
        auto it = pending_.find(seq);
        if (it != pending_.end()) {
            Pending pp = it.value();
            pending_.erase(it);
            if (pp.timer) pp.timer->deleteLater();
            emit rawLog(QString("[TCP] timeout seq=%1").arg(seq));
            if (pp.cb) pp.cb(RpcResult::timeout());
        }
    });
    pending_.insert(seq, p);
    p.timer->start(timeout_ms);

    if (!sendFrame(MsgType::Request, seq, payload)) {
        auto it = pending_.find(seq);
        if (it != pending_.end()) {
            Pending pp = it.value();
            pending_.erase(it);
            if (pp.timer) {
                pp.timer->stop();
                pp.timer->deleteLater();
            }
            if (pp.cb) pp.cb(RpcResult::error(-253, "send failed"));
        }
        return;
    }

    emit rawLog(QString("[TCP] req seq=%1 cmd=%2").arg(seq).arg(cmd));
}

bool ControlClient::sendFrame(MsgType type, uint32_t seq, const nlohmann::json& payload)
{
    std::string body = payload.dump();
    QByteArray buf(static_cast<int>(sizeof(CtrlHeader) + body.size()), '\0');

    CtrlHeader hdr;
    hdr.magic       = kCtrlMagic;
    hdr.version     = kProtoVersion;
    hdr.type        = static_cast<uint16_t>(type);
    hdr.seq         = seq;
    hdr.payload_len = static_cast<uint32_t>(body.size());
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());

    return sock_->write(buf) >= 0;
}

void ControlClient::failAllPending(const QString& reason)
{
    auto copy = pending_;
    pending_.clear();
    for (auto it = copy.begin(); it != copy.end(); ++it) {
        if (it->timer) { it->timer->stop(); it->timer->deleteLater(); }
        if (it->cb) it->cb(RpcResult::error(-255, reason));
    }
}
