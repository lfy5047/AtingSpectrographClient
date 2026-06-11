#pragma once

#include <QIcon>
#include <QImage>
#include <QSet>
#include <QWidget>
#include <QVector>

#include "Client/recording/RemoteFileDownloader.h"
#include "ImageFrameUtils.h"
#include "nlohmann/json.hpp"

class DeviceClient;
class QCheckBox;
class QComboBox;
class QDialog;
class QGroupBox;
class QKeyEvent;
class QLabel;
class QTableWidgetItem;
class QPushButton;
class QVariant;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTimer;
class QThread;
class TifRenderWorker;

class RecordPlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecordPlaybackPanel(DeviceClient* device, QWidget* parent = nullptr);
    ~RecordPlaybackPanel() override;

    void cancelRemoteWork();
    void stopPlayback();

signals:
    void playbackImageReady(QImage image, QString info, ChannelImageStats stats);
    void requestSwitchToPlaybackView();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void refreshRetention();
    void applyRetention();
    void queryRecords();
    void downloadSelected();
    void onDownloadProgress(quint64 received, quint64 total, const QString& currentFile);
    void onDownloadFinished(const QStringList& recordIds);
    void onDownloadFailed(RemoteDownloadErrorReason reason, const QString& error);
    void previousFrame();
    void nextFrame();
    void startPlayback();
    void pausePlayback();
    void onPlaybackTick();
    void onSliderReleased();
    void onRenderSettingsChanged();
    void updateRenderVisibility();
    void updateRetentionTotalLabel();
    void onFrameSpinChanged(int value);
    void exportCurrentFrame();
    void refreshCacheInfo();
    void clearNonPlaybackCache();
    void clearAllCache();
    void selectAllRecords();
    void invertRecordSelection();
    void clearRecordSelection();
    void scheduleTifRerender();
    void cancelTifRender();
    void clearSkippedBadFrames();
    void togglePlaybackSequenceVisible(bool checked);
    void removeSelectedPlaybackRecords();
    void clearPlaybackSequence();
    void appendSelectedToPlaybackSequence();
    void showRecordDetails(QTableWidgetItem* item);

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

    enum class CacheState {
        Cached,
        CachedNoManifest,
        Missing,
        Incomplete,
        PossiblyStale,
    };

    enum class SeekTrigger {
        Playback,
        Manual,
        RenderSettings,
    };

    enum class BadFramePolicy {
        Pause = 0,
        Skip = 1,
    };

    struct CacheSummary {
        int recordCount = 0;
        int fileCount = 0;
        quint64 totalBytes = 0;
    };

    struct BandSelector {
        QSlider* slider = nullptr;
        QSpinBox* spin = nullptr;
        int maxPageCount = 0;
    };

    void setupUi();
    void setStatus(const QString& text, bool isError = false);
    void updateUiEnabled();
    void updateDownloadButtonText();
    void showRecordsDialog();
    void setRecordSelectionLocked(bool locked);
    void cancelDownloadOnly();
    void clearRecords();
    void addRecordRow(const RecordItem& item);
    QVector<RecordItem> selectedRecords() const;
    bool selectedRecordsType(const QVector<RecordItem>& items, QString* type, QString* err) const;
    bool parseRecordList(const nlohmann::json& data, QVector<RecordItem>* out, QString* err, int* skipped = nullptr) const;
    bool parseRecordId(const QString& id, quint64* out, QString* err) const;
    QString cacheRoot() const;
    QString recordCacheDir(const QString& type, const QString& recordId) const;
    bool isRecordCached(const RecordItem& item, QString* err) const;
    CacheState cacheState(const RecordItem& item) const;
    QString cacheStateText(CacheState state) const;
    QIcon cacheStateIcon(CacheState state) const;
    QString manifestPath(const RecordItem& item) const;
    bool readCacheManifest(const RecordItem& item, nlohmann::json* manifest) const;
    bool writeCacheManifest(const RecordItem& item, QString* err) const;
    bool manifestFilesMatch(const RecordItem& item, const nlohmann::json& manifest) const;
    void updateRecordCacheStatuses();
    QString formatRecordTimestamp(quint64 timestampNs) const;
    quint64 retentionSecondsFromInputs() const;
    void setRetentionInputsFromSeconds(quint64 seconds);
    void setPlaybackSequenceCleared();
    QString currentExportFileName() const;
    ChannelImageStats imageStatsFromDisplayImage(const QImage& image) const;
    CacheSummary summarizeCache(const QSet<QString>& keepKeys = QSet<QString>(), bool onlyRemovable = false) const;
    bool removeCacheRecords(const QSet<QString>& keepKeys, bool removeAll, CacheSummary* removed, QString* err);
    bool prepareForCacheMutation(QString* err);
    QSet<QString> playbackCacheKeys() const;
    void updatePlaybackSequenceTable();
    QVector<RecordItem> selectedPlaybackRecords() const;
    QVector<RecordItem> currentPlaybackRecordItems() const;
    void setPlaybackSequence(const QVector<RecordItem>& items, bool append);
    bool buildPlaybackSequence(const QVector<RecordItem>& items, QString* err);
    bool loadRawEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const;
    bool loadTifEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const;
    void rebuildFrameRefs();
    void showFrame(quint64 globalIndex);
    void seekToFrame(quint64 globalIndex, SeekTrigger trigger = SeekTrigger::Manual, bool resumeIfWasPlaying = false);
    void submitTifRenderRequest(quint64 globalIndex, SeekTrigger trigger, bool resumeWhenReady);
    bool currentTifRenderArgs(int pageCount, int* mode, int* a, int* b, int* c, int* d, int* e, int* f, QString* err) const;
    void onTifRenderReady(quint64 requestId, quint64 globalIndex, QImage image, QString info, ChannelImageStats stats);
    void onTifRenderFailed(quint64 requestId, quint64 globalIndex, QString error);
    void onTifRenderCanceled(quint64 requestId);
    void onTifRenderProgress(quint64 requestId, QString text);
    void onTifRenderTimeout();
    void handleFrameError(quint64 globalIndex, const FrameRef& ref, const PlaybackEntry& entry, const QString& err);
    void finishDisplayedFrame(quint64 globalIndex, const QImage& image, const QString& info, const ChannelImageStats& stats);
    void resetSkippedBadFrames();
    void updateSkippedBadFramesStatus();
    bool cancelTifRenderAndWait(int timeoutMs, QString* err);
    void updateTifBandRanges(const PlaybackEntry& entry);
    void syncBandSelector(BandSelector* selector, int pageCount);
    int bandValue0(const BandSelector& selector) const;
    int rangeEndValueExclusive(const BandSelector& selector, const QCheckBox* toLast, int pageCount) const;
    bool renderRawFrame(const PlaybackEntry& entry, quint64 frameIndex, QImage* image, QString* info, ChannelImageStats* stats, QString* err) const;
    bool renderTifFrame(const PlaybackEntry& entry, QImage* image, QString* info, ChannelImageStats* stats, QString* err) const;
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
    void loadSettings();
    void saveSettings() const;
    void setComboByData(QComboBox* combo, const QVariant& data, int fallbackIndex);

    DeviceClient* device_ = nullptr;
    RemoteFileDownloader* downloader_ = nullptr;
    QThread* downloaderThread_ = nullptr;
    bool downloadBusy_ = false;
    bool queryBusy_ = false;
    bool recordSelectionLocked_ = false;
    bool loadingSettings_ = false;
    QString host_;
    bool connected_ = false;

    QSpinBox* retentionDaysSpin_ = nullptr;
    QSpinBox* retentionHoursSpin_ = nullptr;
    QSpinBox* retentionMinutesSpin_ = nullptr;
    QSpinBox* retentionSecondsSpin_ = nullptr;
    QPushButton* retentionRefreshBtn_ = nullptr;
    QPushButton* retentionApplyBtn_ = nullptr;
    QLabel* retentionTotalLbl_ = nullptr;
    QLabel* retentionInfoLbl_ = nullptr;
    quint64 currentRetentionSeconds_ = 0;
    bool hasCurrentRetention_ = false;

    QComboBox* typeCombo_ = nullptr;
    QComboBox* queryModeCombo_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QSpinBox* secondsSpin_ = nullptr;
    QPushButton* queryBtn_ = nullptr;
    QPushButton* openRecordsDialogBtn_ = nullptr;
    QDialog* recordsDialog_ = nullptr;
    QPushButton* selectAllBtn_ = nullptr;
    QPushButton* invertSelectionBtn_ = nullptr;
    QPushButton* clearSelectionBtn_ = nullptr;
    QTableWidget* recordsTable_ = nullptr;

    QPushButton* downloadBtn_ = nullptr;
    QLabel* downloadInfoLbl_ = nullptr;
    QPushButton* appendDownloadBtn_ = nullptr;

    QGroupBox* renderGroup_ = nullptr;
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
    QSlider* singleBandSlider_ = nullptr;
    QSlider* rangeBeginSlider_ = nullptr;
    QSlider* rangeEndSlider_ = nullptr;
    QSlider* rBandSlider_ = nullptr;
    QSlider* gBandSlider_ = nullptr;
    QSlider* bBandSlider_ = nullptr;
    QSlider* rBeginSlider_ = nullptr;
    QSlider* rEndSlider_ = nullptr;
    QSlider* gBeginSlider_ = nullptr;
    QSlider* gEndSlider_ = nullptr;
    QSlider* bBeginSlider_ = nullptr;
    QSlider* bEndSlider_ = nullptr;
    QCheckBox* rangeToLastChk_ = nullptr;
    QCheckBox* rToLastChk_ = nullptr;
    QCheckBox* gToLastChk_ = nullptr;
    QCheckBox* bToLastChk_ = nullptr;
    QLabel* tifRenderHintLbl_ = nullptr;
    QPushButton* cancelTifRenderBtn_ = nullptr;

    QPushButton* prevBtn_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QPushButton* pauseBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QSlider* progressSlider_ = nullptr;
    QSpinBox* intervalSpin_ = nullptr;
    QLabel* fpsLbl_ = nullptr;
    QSpinBox* frameSpin_ = nullptr;
    QPushButton* exportFrameBtn_ = nullptr;
    QCheckBox* loopChk_ = nullptr;
    QComboBox* badFramePolicyCombo_ = nullptr;
    QPushButton* clearSkippedBadFramesBtn_ = nullptr;
    QLabel* playbackInfoLbl_ = nullptr;
    QGroupBox* playbackSequenceGroup_ = nullptr;
    QTableWidget* playbackSequenceTable_ = nullptr;
    QPushButton* playbackSeqRemoveBtn_ = nullptr;
    QPushButton* playbackSeqClearBtn_ = nullptr;
    QLabel* cacheInfoLbl_ = nullptr;
    QPushButton* cacheRefreshBtn_ = nullptr;
    QPushButton* cacheOpenBtn_ = nullptr;
    QPushButton* cacheClearUnusedBtn_ = nullptr;
    QPushButton* cacheClearAllBtn_ = nullptr;
    QLabel* statusLbl_ = nullptr;
    QTimer* playbackTimer_ = nullptr;
    QTimer* tifRenderDebounceTimer_ = nullptr;
    QTimer* tifRenderTimeoutTimer_ = nullptr;
    TifRenderWorker* tifRenderWorker_ = nullptr;
    QThread* tifRenderThread_ = nullptr;

    QVector<RecordItem> records_;
    QVector<RecordItem> downloadedSelection_;
    QVector<RecordItem> playbackRecordItems_;
    QVector<RecordItem> pendingDownloadSelection_;
    QString pendingDownloadType_;
    QVector<PlaybackEntry> playbackEntries_;
    QVector<FrameRef> frameRefs_;
    quint64 currentFrame_ = 0;
    bool sliderDragging_ = false;
    bool progressWasPlayingBeforeDrag_ = false;
    bool playbackRequested_ = false;
    bool appendAfterDownload_ = false;
    bool pendingTifBandSettingsValid_ = false;
    int automaticDownloadRetryCount_ = 0;
    bool retryAvailable_ = false;
    QVector<int> pendingTifBandSettings_;
    QImage currentPlaybackImage_;
    ChannelImageStats currentPlaybackStats_;
    QString currentPlaybackInfo_;
    QString lastExportDir_;
    quint64 tifRenderRequestId_ = 0;
    quint64 activeTifRenderRequestId_ = 0;
    quint64 activeTifRenderFrame_ = 0;
    bool tifRenderBusy_ = false;
    bool tifRenderResumeWhenReady_ = false;
    SeekTrigger activeTifRenderTrigger_ = SeekTrigger::Manual;
    int skippedBadFrames_ = 0;
};
