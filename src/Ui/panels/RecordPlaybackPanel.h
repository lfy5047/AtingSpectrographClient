#pragma once

#include <QImage>
#include <QWidget>
#include <QVector>

#include "Client/recording/RemoteFileDownloader.h"
#include "nlohmann/json.hpp"

class DeviceClient;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTimer;
class QThread;

class RecordPlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecordPlaybackPanel(DeviceClient* device, QWidget* parent = nullptr);
    ~RecordPlaybackPanel() override;

    void cancelRemoteWork();
    void stopPlayback();

signals:
    void playbackImageReady(QImage image, QString info);
    void requestSwitchToPlaybackView();

private slots:
    void refreshRetention();
    void applyRetention();
    void queryRecords();
    void downloadSelected();
    void onDownloadProgress(quint64 received, quint64 total, const QString& currentFile);
    void onDownloadFinished(const QStringList& recordIds);
    void onDownloadFailed(const QString& error);
    void previousFrame();
    void nextFrame();
    void startPlayback();
    void pausePlayback();
    void onPlaybackTick();
    void onSliderReleased();
    void onRenderSettingsChanged();
    void updateRenderVisibility();

private:
    struct RecordFile {
        QString name;
        quint64 sizeBytes = 0;
    };

    struct RecordItem {
        QString recordId;
        QString type;
        quint64 recordIdValue = 0;
        quint64 timestampNs = 0;
        QVector<RecordFile> files;
    };

    struct PlaybackEntry {
        QString type;
        QString recordId;
        quint64 recordIdValue = 0;
        QString rawPath;
        QString jsonPath;
        QString tifPath;
        int width = 0;
        int height = 0;
        quint64 frameCount = 0;
        quint64 bytesPerFrame = 0;
        QVector<quint8> frameTypes;
        int pageCount = 0;
        int tifWidth = 0;
        int tifHeight = 0;
    };

    struct FrameRef {
        int entryIndex = -1;
        quint64 frameIndex = 0;
    };

    enum class TifRenderMode {
        SingleBand = 0,
        RangeAverage = 1,
        RgbSingleBand = 2,
        RgbRangeAverage = 3,
    };

    void setupUi();
    void setStatus(const QString& text, bool isError = false);
    void updateUiEnabled();
    void clearRecords();
    void addRecordRow(const RecordItem& item);
    QVector<RecordItem> selectedRecords() const;
    bool parseRecordList(const nlohmann::json& data, QVector<RecordItem>* out, QString* err) const;
    bool parseRecordId(const QString& id, quint64* out, QString* err) const;
    QString cacheRoot() const;
    QString recordCacheDir(const QString& type, const QString& recordId) const;
    bool isRecordCached(const RecordItem& item, QString* err) const;
    bool buildPlaybackSequence(const QVector<RecordItem>& items, QString* err);
    bool loadRawEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const;
    bool loadTifEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const;
    void rebuildFrameRefs();
    void showFrame(quint64 globalIndex);
    bool renderRawFrame(const PlaybackEntry& entry, quint64 frameIndex, QImage* image, QString* info, QString* err) const;
    bool renderTifFrame(const PlaybackEntry& entry, QImage* image, QString* info, QString* err) const;
    bool validateTif(const QString& path, PlaybackEntry* entry, QString* err) const;
    bool renderTifGray(const QString& path, int pageCount, int begin, int end,
                       QImage* image, QString* err) const;
    bool renderTifRgb(const QString& path, int pageCount,
                      int rBegin, int rEnd, int gBegin, int gEnd, int bBegin, int bEnd,
                      QImage* image, QString* err) const;
    bool normalizeRange(int pageCount, int begin, int end, int* outBegin, int* outEnd, QString* err) const;
    bool readTifPageU16(void* tif, int pageIndex, int width, int height, QVector<quint16>* out, QString* err) const;
    QString filesText(const RecordItem& item) const;
    QString formatBytes(quint64 bytes) const;
    QString frameTypeText(quint8 frameType) const;

    DeviceClient* device_ = nullptr;
    RemoteFileDownloader* downloader_ = nullptr;
    QThread* downloaderThread_ = nullptr;
    bool downloadBusy_ = false;
    QString host_;
    bool connected_ = false;

    QSpinBox* retentionSpin_ = nullptr;
    QPushButton* retentionRefreshBtn_ = nullptr;
    QPushButton* retentionApplyBtn_ = nullptr;
    QLabel* retentionInfoLbl_ = nullptr;

    QComboBox* typeCombo_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QSpinBox* secondsSpin_ = nullptr;
    QPushButton* queryBtn_ = nullptr;
    QTableWidget* recordsTable_ = nullptr;

    QPushButton* downloadBtn_ = nullptr;
    QLabel* downloadInfoLbl_ = nullptr;

    QComboBox* renderModeCombo_ = nullptr;
    QSpinBox* singleBandSpin_ = nullptr;
    QSpinBox* rangeBeginSpin_ = nullptr;
    QSpinBox* rangeEndSpin_ = nullptr;
    QSpinBox* rBandSpin_ = nullptr;
    QSpinBox* gBandSpin_ = nullptr;
    QSpinBox* bBandSpin_ = nullptr;
    QSpinBox* rBeginSpin_ = nullptr;
    QSpinBox* rEndSpin_ = nullptr;
    QSpinBox* gBeginSpin_ = nullptr;
    QSpinBox* gEndSpin_ = nullptr;
    QSpinBox* bBeginSpin_ = nullptr;
    QSpinBox* bEndSpin_ = nullptr;

    QPushButton* prevBtn_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QPushButton* pauseBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QSlider* progressSlider_ = nullptr;
    QSpinBox* intervalSpin_ = nullptr;
    QCheckBox* loopChk_ = nullptr;
    QLabel* playbackInfoLbl_ = nullptr;
    QLabel* statusLbl_ = nullptr;
    QTimer* playbackTimer_ = nullptr;

    QVector<RecordItem> records_;
    QVector<RecordItem> downloadedSelection_;
    QVector<PlaybackEntry> playbackEntries_;
    QVector<FrameRef> frameRefs_;
    quint64 currentFrame_ = 0;
    bool sliderDragging_ = false;
};
