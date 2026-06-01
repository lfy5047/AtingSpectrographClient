#pragma once

#include "ImageFrameUtils.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

struct tiff;
typedef struct tiff TIFF;

struct TifRenderRequest {
    quint64 requestId = 0;
    quint64 globalIndex = 0;
    QString recordId;
    QString path;
    int pageCount = 0;
    int width = 0;
    int height = 0;
    int mode = 0;
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int e = 0;
    int f = 0;
};
Q_DECLARE_METATYPE(TifRenderRequest)

class TifRenderWorker : public QObject {
    Q_OBJECT

public:
    explicit TifRenderWorker(QObject* parent = nullptr);

    void requestCancel(quint64 newestRequestId);

public slots:
    void render(TifRenderRequest request);

signals:
    void ready(quint64 requestId, quint64 globalIndex, QImage image, QString info, ChannelImageStats stats);
    void failed(quint64 requestId, quint64 globalIndex, QString error);
    void canceled(quint64 requestId);
    void progress(quint64 requestId, QString text);

private:
    bool shouldCancel(const TifRenderRequest& request) const;
    bool normalizeRange(int pageCount, int begin, int end, int* outBegin, int* outEnd, QString* err) const;
    bool readPage(TIFF* tif, const TifRenderRequest& request, int pageIndex, QVector<quint16>* out, QString* err) const;
    bool renderGrayRange(TIFF* tif, const TifRenderRequest& request, int begin, int end, QImage* image, QString* err);
    bool renderRgb(TIFF* tif, const TifRenderRequest& request,
                   int rb, int re, int gb, int ge, int bb, int be,
                   QImage* image, QString* err);
    bool renderRequest(const TifRenderRequest& request, QImage* image, QString* info, QString* err);
    static ChannelImageStats imageStatsFromImage(const QImage& image);

    std::atomic<quint64> newestRequestId_{0};
    std::atomic_bool cancelRequested_{false};
};
