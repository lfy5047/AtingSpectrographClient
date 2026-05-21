#pragma once

#include <QObject>

#include "FramePlaybackController.h"

class FramePlaybackScanWorker : public QObject {
    Q_OBJECT
public:
    explicit FramePlaybackScanWorker(FramePlaybackController* owner, QObject* parent = nullptr);

public slots:
    void scan(const QString& path);

private:
    void postResult(const QString& path, bool ok, const QString& err,
                    const recording::FileHeader& header,
                    const QList<FramePlaybackController::IndexEntry>& idx,
                    quint64 damaged);

    FramePlaybackController* owner_ = nullptr;
};
