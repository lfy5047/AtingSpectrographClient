#include "RecordPlaybackPanel.h"

#include "DeviceClient.h"
#include "ImageFrameUtils.h"
#include "Protocol.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QMetaType>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <limits>

#include "tiffio.h"

namespace {
QString jsonString(const nlohmann::json& j, const char* key)
{
    return j.contains(key) && j[key].is_string()
        ? QString::fromStdString(j[key].get<std::string>())
        : QString();
}

quint64 jsonU64(const nlohmann::json& j, const char* key, quint64 def = 0)
{
    if (!j.contains(key)) return def;
    if (j[key].is_number_unsigned()) return j[key].get<quint64>();
    if (j[key].is_number_integer()) {
        const qint64 v = j[key].get<qint64>();
        return v >= 0 ? static_cast<quint64>(v) : def;
    }
    return def;
}

int jsonInt(const nlohmann::json& j, const char* key, int def = 0)
{
    if (!j.contains(key) || !j[key].is_number_integer()) return def;
    return j[key].get<int>();
}

bool checkedMul(quint64 a, quint64 b, quint64* out)
{
    if (!out) return false;
    if (a != 0 && b > std::numeric_limits<quint64>::max() / a) return false;
    *out = a * b;
    return true;
}

quint8 frameTypeFromJson(const nlohmann::json& j)
{
    using namespace cli::proto;
    nlohmann::json v;
    if (j.contains("FrameType")) v = j["FrameType"];
    else if (j.contains("frame_type")) v = j["frame_type"];
    else if (j.contains("type")) v = j["type"];

    if (v.is_number_integer()) {
        const int n = v.get<int>();
        if (n >= UnknownFrame && n <= TailFrame) return static_cast<quint8>(n);
        return UnknownFrame;
    }
    if (!v.is_string()) return UnknownFrame;

    const QString s = QString::fromStdString(v.get<std::string>()).toLower();
    if (s == "headerframe" || s == "header") return HeaderFrame;
    if (s == "dataframe" || s == "data") return DataFrame;
    if (s == "tailframe" || s == "tail") return TailFrame;
    return UnknownFrame;
}
}

RecordPlaybackPanel::RecordPlaybackPanel(DeviceClient* device, QWidget* parent)
    : QWidget(parent)
    , device_(device)
{
    setupUi();
    qRegisterMetaType<QVector<RemoteFetchFile>>("QVector<RemoteFetchFile>");

    downloaderThread_ = new QThread(this);
    downloader_ = new RemoteFileDownloader();
    downloader_->moveToThread(downloaderThread_);
    connect(downloaderThread_, &QThread::finished, downloader_, &QObject::deleteLater);
    downloaderThread_->start();

    connect(downloader_, &RemoteFileDownloader::progress,
            this, &RecordPlaybackPanel::onDownloadProgress);
    connect(downloader_, &RemoteFileDownloader::finished,
            this, &RecordPlaybackPanel::onDownloadFinished);
    connect(downloader_, &RemoteFileDownloader::failed,
            this, &RecordPlaybackPanel::onDownloadFailed);
    connect(downloader_, &RemoteFileDownloader::canceled, this, [this]() {
        downloadBusy_ = false;
        setStatus(QString::fromUtf8("下载已取消"), false);
        updateUiEnabled();
    });

    if (device_) {
        connect(device_, &DeviceClient::connectionChanged, this,
                [this](bool connected, const QString& ip) {
            connected_ = connected;
            host_ = connected ? ip : QString();
            if (!connected) {
                cancelRemoteWork();
                clearRecords();
            } else {
                refreshRetention();
                queryRecords();
            }
            updateUiEnabled();
        });
        connected_ = device_->isConnected();
        host_ = device_->control() ? device_->control()->peerAddress() : QString();
    }

    updateUiEnabled();
}

RecordPlaybackPanel::~RecordPlaybackPanel()
{
    cancelRemoteWork();
    if (downloaderThread_) {
        if (downloaderThread_->isRunning()) {
            QMetaObject::invokeMethod(downloader_, "cancel", Qt::BlockingQueuedConnection);
            downloaderThread_->quit();
            downloaderThread_->wait();
        }
        downloaderThread_ = nullptr;
        downloader_ = nullptr;
    }
}

void RecordPlaybackPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* retentionGroup = new QGroupBox(QString::fromUtf8("保留时间"), this);
    auto* retentionForm = new QFormLayout(retentionGroup);
    auto* retentionRow = new QHBoxLayout();
    retentionSpin_ = new QSpinBox(this);
    retentionSpin_->setRange(0, 24 * 3600 * 30);
    retentionSpin_->setSuffix(QString::fromUtf8(" 秒"));
    retentionRefreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    retentionApplyBtn_ = new QPushButton(QString::fromUtf8("设置"), this);
    retentionRow->addWidget(retentionSpin_);
    retentionRow->addWidget(retentionRefreshBtn_);
    retentionRow->addWidget(retentionApplyBtn_);
    retentionForm->addRow(QString::fromUtf8("服务端保留"), retentionRow);
    retentionInfoLbl_ = new QLabel("-", this);
    retentionInfoLbl_->setWordWrap(true);
    retentionForm->addRow(QString::fromUtf8("估算"), retentionInfoLbl_);
    root->addWidget(retentionGroup);

    auto* queryGroup = new QGroupBox(QString::fromUtf8("远程记录"), this);
    auto* queryLayout = new QVBoxLayout(queryGroup);

    auto* queryForm = new QFormLayout();
    queryForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem("raw", "raw");
    typeCombo_->addItem("tif", "tif");
    typeCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    countSpin_ = new QSpinBox(this);
    countSpin_->setRange(0, 10000);
    countSpin_->setValue(10);
    countSpin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    secondsSpin_ = new QSpinBox(this);
    secondsSpin_->setRange(0, 24 * 3600 * 30);
    secondsSpin_->setValue(0);
    secondsSpin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    queryBtn_ = new QPushButton(QString::fromUtf8("查询"), this);

    auto* typeCountRow = new QHBoxLayout();
    typeCountRow->addWidget(typeCombo_);
    typeCountRow->addSpacing(8);
    typeCountRow->addWidget(countSpin_);
    queryForm->addRow(QString::fromUtf8("类型 / 最近数量"), typeCountRow);

    auto* secondsRow = new QHBoxLayout();
    secondsRow->addWidget(secondsSpin_);
    secondsRow->addSpacing(8);
    secondsRow->addWidget(queryBtn_);
    queryForm->addRow(QString::fromUtf8("最近秒数 (0=不限)"), secondsRow);

    queryLayout->addLayout(queryForm);

    recordsTable_ = new QTableWidget(this);
    recordsTable_->setColumnCount(5);
    recordsTable_->setHorizontalHeaderLabels(QStringList()
        << "record_id" << "type" << "timestamp" << QString::fromUtf8("文件") << QString::fromUtf8("大小"));
    recordsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recordsTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    recordsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recordsTable_->verticalHeader()->setVisible(false);
    recordsTable_->setAlternatingRowColors(true);
    recordsTable_->setShowGrid(false);
    recordsTable_->setMinimumHeight(150);
    recordsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    recordsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    queryLayout->addWidget(recordsTable_);

    auto* downloadRow = new QHBoxLayout();
    downloadBtn_ = new QPushButton(QString::fromUtf8("下载/加入播放"), this);
    downloadBtn_->setProperty("primary", true);
    downloadInfoLbl_ = new QLabel("-", this);
    downloadInfoLbl_->setWordWrap(true);
    downloadRow->addWidget(downloadBtn_);
    downloadRow->addWidget(downloadInfoLbl_, 1);
    queryLayout->addLayout(downloadRow);
    root->addWidget(queryGroup, 1);

    auto* renderGroup = new QGroupBox(QString::fromUtf8("tif 渲染参数"), this);
    auto* renderForm = new QFormLayout(renderGroup);
    renderModeCombo_ = new QComboBox(this);
    renderModeCombo_->addItem(QString::fromUtf8("单波段"), static_cast<int>(TifRenderMode::SingleBand));
    renderModeCombo_->addItem(QString::fromUtf8("范围平均"), static_cast<int>(TifRenderMode::RangeAverage));
    renderModeCombo_->addItem(QString::fromUtf8("单波段 RGB 合成"), static_cast<int>(TifRenderMode::RgbSingleBand));
    renderModeCombo_->addItem(QString::fromUtf8("范围平均 RGB 合成"), static_cast<int>(TifRenderMode::RgbRangeAverage));
    renderForm->addRow(QString::fromUtf8("模式"), renderModeCombo_);
    singleBandSpin_ = new QSpinBox(this);
    rangeBeginSpin_ = new QSpinBox(this);
    rangeEndSpin_ = new QSpinBox(this);
    rBandSpin_ = new QSpinBox(this);
    gBandSpin_ = new QSpinBox(this);
    bBandSpin_ = new QSpinBox(this);
    rBeginSpin_ = new QSpinBox(this);
    rEndSpin_ = new QSpinBox(this);
    gBeginSpin_ = new QSpinBox(this);
    gEndSpin_ = new QSpinBox(this);
    bBeginSpin_ = new QSpinBox(this);
    bEndSpin_ = new QSpinBox(this);
    const QList<QSpinBox*> bandSpins = {
        singleBandSpin_, rangeBeginSpin_, rangeEndSpin_, rBandSpin_, gBandSpin_, bBandSpin_,
        rBeginSpin_, rEndSpin_, gBeginSpin_, gEndSpin_, bBeginSpin_, bEndSpin_
    };
    for (QSpinBox* s : bandSpins) {
        s->setRange(0, 0);
    }
    renderForm->addRow(QString::fromUtf8("单波段"), singleBandSpin_);
    renderForm->addRow(QString::fromUtf8("范围 begin"), rangeBeginSpin_);
    renderForm->addRow(QString::fromUtf8("范围 end(0=末尾)"), rangeEndSpin_);
    renderForm->addRow("R Band", rBandSpin_);
    renderForm->addRow("G Band", gBandSpin_);
    renderForm->addRow("B Band", bBandSpin_);
    renderForm->addRow("R begin", rBeginSpin_);
    renderForm->addRow("R end(0=end)", rEndSpin_);
    renderForm->addRow("G begin", gBeginSpin_);
    renderForm->addRow("G end(0=end)", gEndSpin_);
    renderForm->addRow("B begin", bBeginSpin_);
    renderForm->addRow("B end(0=end)", bEndSpin_);
    root->addWidget(renderGroup);

    updateRenderVisibility();

    auto* playbackGroup = new QGroupBox(QString::fromUtf8("播放"), this);
    auto* playbackForm = new QFormLayout(playbackGroup);
    auto* btnRow = new QHBoxLayout();
    prevBtn_ = new QPushButton(QString::fromUtf8("上一帧"), this);
    playBtn_ = new QPushButton(QString::fromUtf8("播放"), this);
    playBtn_->setProperty("primary", true);
    pauseBtn_ = new QPushButton(QString::fromUtf8("暂停"), this);
    stopBtn_ = new QPushButton(QString::fromUtf8("停止"), this);
    stopBtn_->setProperty("danger", true);
    nextBtn_ = new QPushButton(QString::fromUtf8("下一帧"), this);
    btnRow->addWidget(prevBtn_);
    btnRow->addWidget(playBtn_);
    btnRow->addWidget(pauseBtn_);
    btnRow->addWidget(stopBtn_);
    btnRow->addWidget(nextBtn_);
    playbackForm->addRow(btnRow);
    progressSlider_ = new QSlider(Qt::Horizontal, this);
    progressSlider_->setRange(0, 0);
    playbackForm->addRow(QString::fromUtf8("进度"), progressSlider_);
    intervalSpin_ = new QSpinBox(this);
    intervalSpin_->setRange(10, 600000);
    intervalSpin_->setValue(200);
    intervalSpin_->setSuffix(" ms");
    loopChk_ = new QCheckBox(QString::fromUtf8("循环播放"), this);
    auto* intervalRow = new QHBoxLayout();
    intervalRow->addWidget(intervalSpin_);
    intervalRow->addWidget(loopChk_);
    playbackForm->addRow(QString::fromUtf8("间隔"), intervalRow);
    playbackInfoLbl_ = new QLabel("0/0", this);
    playbackInfoLbl_->setWordWrap(true);
    playbackForm->addRow(QString::fromUtf8("当前位置"), playbackInfoLbl_);
    root->addWidget(playbackGroup);

    auto* statusGroup = new QGroupBox(QString::fromUtf8("状态"), this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusLbl_ = new QLabel("-", this);
    statusLbl_->setWordWrap(true);
    statusLayout->addWidget(statusLbl_);
    root->addWidget(statusGroup);

    playbackTimer_ = new QTimer(this);

    connect(retentionRefreshBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::refreshRetention);
    connect(retentionApplyBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::applyRetention);
    connect(queryBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::queryRecords);
    connect(downloadBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::downloadSelected);
    connect(recordsTable_, &QTableWidget::itemSelectionChanged,
            this, &RecordPlaybackPanel::updateUiEnabled);
    connect(prevBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::previousFrame);
    connect(nextBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::nextFrame);
    connect(playBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::startPlayback);
    connect(pauseBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::pausePlayback);
    connect(stopBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::stopPlayback);
    connect(progressSlider_, &QSlider::sliderPressed, this, [this]() { sliderDragging_ = true; });
    connect(progressSlider_, &QSlider::sliderReleased, this, &RecordPlaybackPanel::onSliderReleased);
    connect(playbackTimer_, &QTimer::timeout, this, &RecordPlaybackPanel::onPlaybackTick);

    connect(renderModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecordPlaybackPanel::onRenderSettingsChanged);
    connect(renderModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecordPlaybackPanel::updateRenderVisibility);
    for (QSpinBox* s : bandSpins) {
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RecordPlaybackPanel::onRenderSettingsChanged);
    }
}

void RecordPlaybackPanel::setStatus(const QString& text, bool isError)
{
    statusLbl_->setText(text.isEmpty() ? "-" : text);
    statusLbl_->setStyleSheet(isError ? "color:#E5484D;" : "");
}

void RecordPlaybackPanel::updateUiEnabled()
{
    const bool busy = downloadBusy_;
    const bool hasConnection = connected_ && device_ && device_->isConnected();
    const bool hasFrames = !frameRefs_.isEmpty();
    const bool playing = playbackTimer_ && playbackTimer_->isActive();

    retentionRefreshBtn_->setEnabled(hasConnection && !busy);
    retentionApplyBtn_->setEnabled(hasConnection && !busy);
    queryBtn_->setEnabled(hasConnection && !busy);
    downloadBtn_->setEnabled(hasConnection && !busy && !recordsTable_->selectedItems().isEmpty());

    prevBtn_->setEnabled(hasFrames && !playing);
    nextBtn_->setEnabled(hasFrames && !playing);
    playBtn_->setEnabled(hasFrames && !playing);
    pauseBtn_->setEnabled(playing);
    stopBtn_->setEnabled(hasFrames);
    progressSlider_->setEnabled(hasFrames && !playing);
}

void RecordPlaybackPanel::cancelRemoteWork()
{
    if (downloader_ && downloadBusy_) {
        QMetaObject::invokeMethod(downloader_, "cancel", Qt::QueuedConnection);
        downloadBusy_ = false;
    }
    if (playbackTimer_) playbackTimer_->stop();
    playbackEntries_.clear();
    frameRefs_.clear();
    currentFrame_ = 0;
    progressSlider_->setRange(0, 0);
    playbackInfoLbl_->setText("0/0");
    updateUiEnabled();
}

void RecordPlaybackPanel::refreshRetention()
{
    if (!device_ || !device_->record()) return;
    device_->record()->getRetention(this, [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            setStatus(err, true);
            return;
        }
        const quint64 seconds = jsonU64(data, "retention_seconds", 0);
        retentionSpin_->setValue(static_cast<int>(qMin<quint64>(seconds, retentionSpin_->maximum())));
        retentionInfoLbl_->setText(QString::fromUtf8("当前保留 %1 秒").arg(seconds));
        setStatus(QString::fromUtf8("保留时间已刷新"), false);
    });
}

void RecordPlaybackPanel::applyRetention()
{
    if (!device_ || !device_->record()) return;
    const quint64 seconds = static_cast<quint64>(retentionSpin_->value());
    device_->record()->setRetention(this, seconds, [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            setStatus(err, true);
            return;
        }
        const quint64 s = jsonU64(data, "retention_seconds", 0);
        retentionInfoLbl_->setText(QString::fromUtf8("当前=%1 秒 raw估算=%2 tif估算=%3")
            .arg(s)
            .arg(formatBytes(jsonU64(data, "raw_estimated_bytes", 0)))
            .arg(formatBytes(jsonU64(data, "tif_estimated_bytes", 0))));
        setStatus(QString::fromUtf8("保留时间设置成功"), false);
    });
}

void RecordPlaybackPanel::queryRecords()
{
    if (!device_ || !device_->record()) return;
    const QString type = typeCombo_->currentData().toString();
    quint64 count = static_cast<quint64>(countSpin_->value());
    quint64 seconds = static_cast<quint64>(secondsSpin_->value());
    if (count == 0 && seconds == 0) count = 10;
    device_->record()->listRecent(this, type, count, seconds,
        [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            setStatus(err, true);
            return;
        }
        QVector<RecordItem> items;
        QString parseErr;
        if (!parseRecordList(data, &items, &parseErr)) {
            setStatus(parseErr, true);
            return;
        }
        clearRecords();
        records_ = items;
        for (const RecordItem& item : records_) {
            addRecordRow(item);
        }
        setStatus(QString::fromUtf8("查询到 %1 条记录").arg(records_.size()), false);
        updateUiEnabled();
    });
}

void RecordPlaybackPanel::clearRecords()
{
    records_.clear();
    recordsTable_->setRowCount(0);
}

bool RecordPlaybackPanel::parseRecordList(const nlohmann::json& data, QVector<RecordItem>* out, QString* err) const
{
    if (!out) return false;
    out->clear();
    if (!data.contains("items") || !data["items"].is_array()) {
        if (err) *err = QString::fromUtf8("record.list_recent 响应缺少 items");
        return false;
    }
    for (const auto& j : data["items"]) {
        RecordItem item;
        item.recordId = jsonString(j, "record_id");
        item.type = jsonString(j, "type");
        item.timestampNs = jsonU64(j, "timestamp_ns", 0);
        if (!parseRecordId(item.recordId, &item.recordIdValue, err)) return false;
        if (!j.contains("files") || !j["files"].is_array()) {
            if (err) *err = QString::fromUtf8("记录 %1 缺少 files").arg(item.recordId);
            return false;
        }
        for (const auto& f : j["files"]) {
            RecordFile rf;
            rf.name = jsonString(f, "name");
            rf.sizeBytes = jsonU64(f, "size_bytes", 0);
            if (rf.name.isEmpty() || rf.sizeBytes == 0) {
                if (err) *err = QString::fromUtf8("记录 %1 文件信息无效").arg(item.recordId);
                return false;
            }
            item.files.append(rf);
        }
        out->append(item);
    }
    return true;
}

bool RecordPlaybackPanel::parseRecordId(const QString& id, quint64* out, QString* err) const
{
    bool ok = false;
    const quint64 v = id.toULongLong(&ok, 10);
    if (!ok) {
        if (err) *err = QString::fromUtf8("record_id 不是 unsigned 64-bit: %1").arg(id);
        return false;
    }
    if (out) *out = v;
    return true;
}

void RecordPlaybackPanel::addRecordRow(const RecordItem& item)
{
    const int row = recordsTable_->rowCount();
    recordsTable_->insertRow(row);
    recordsTable_->setItem(row, 0, new QTableWidgetItem(item.recordId));
    recordsTable_->setItem(row, 1, new QTableWidgetItem(item.type));
    recordsTable_->setItem(row, 2, new QTableWidgetItem(QString::number(item.timestampNs)));
    recordsTable_->setItem(row, 3, new QTableWidgetItem(filesText(item)));
    quint64 size = 0;
    for (const RecordFile& f : item.files) size += f.sizeBytes;
    recordsTable_->setItem(row, 4, new QTableWidgetItem(formatBytes(size)));
}

QVector<RecordPlaybackPanel::RecordItem> RecordPlaybackPanel::selectedRecords() const
{
    QVector<RecordItem> result;
    QSet<int> rows;
    for (QTableWidgetItem* item : recordsTable_->selectedItems()) rows.insert(item->row());
    const QList<int> rowList = rows.values();
    for (int row : rowList) {
        if (row >= 0 && row < records_.size()) result.append(records_[row]);
    }
    std::sort(result.begin(), result.end(), [](const RecordItem& a, const RecordItem& b) {
        return a.recordIdValue < b.recordIdValue;
    });
    return result;
}

void RecordPlaybackPanel::downloadSelected()
{
    const QVector<RecordItem> selected = selectedRecords();
    if (selected.isEmpty()) return;
    if (host_.isEmpty()) {
        setStatus(QString::fromUtf8("未连接服务端"), true);
        return;
    }

    QVector<RecordItem> missing;
    QString err;
    for (const RecordItem& item : selected) {
        if (!isRecordCached(item, &err)) {
            missing.append(item);
        }
    }
    downloadedSelection_ = selected;

    if (missing.isEmpty()) {
        if (!buildPlaybackSequence(downloadedSelection_, &err)) {
            setStatus(err, true);
            return;
        }
        setStatus(QString::fromUtf8("已复用本地缓存并加入播放序列"), false);
        return;
    }

    QStringList ids;
    for (const RecordItem& item : missing) ids << item.recordId;
    device_->record()->fetch(this, typeCombo_->currentData().toString(), ids,
        [this, missing](bool ok, const nlohmann::json& data, const QString& rpcErr) {
        if (!ok) {
            setStatus(rpcErr, true);
            return;
        }
        const QString transferId = jsonString(data, "transfer_id");
        const quint16 filePort = static_cast<quint16>(jsonU64(data, "file_port", 0));
        if (transferId.isEmpty() || filePort == 0 || !data.contains("files") || !data["files"].is_array()) {
            setStatus(QString::fromUtf8("record.fetch 响应无效"), true);
            return;
        }
        QVector<RemoteFetchFile> fetchFiles;
        for (const auto& f : data["files"]) {
            RemoteFetchFile ff;
            ff.recordId = jsonString(f, "record_id");
            ff.name = jsonString(f, "name");
            ff.sizeBytes = jsonU64(f, "size_bytes", 0);
            fetchFiles.append(ff);
        }
        downloadBusy_ = true;
        const QString type = typeCombo_->currentData().toString();
        const QString root = cacheRoot();
        const bool invoked = QMetaObject::invokeMethod(
            downloader_, "start", Qt::QueuedConnection,
            Q_ARG(QString, host_),
            Q_ARG(quint16, filePort),
            Q_ARG(QString, transferId),
            Q_ARG(QString, type),
            Q_ARG(QVector<RemoteFetchFile>, fetchFiles),
            Q_ARG(QString, root));
        if (!invoked) {
            downloadBusy_ = false;
            setStatus(QString::fromUtf8("无法启动下载线程"), true);
            updateUiEnabled();
            return;
        }
        setStatus(QString::fromUtf8("正在下载远程记录..."), false);
        updateUiEnabled();
    });
}

void RecordPlaybackPanel::onDownloadProgress(quint64 received, quint64 total, const QString& currentFile)
{
    downloadInfoLbl_->setText(QString("%1 / %2  %3")
        .arg(formatBytes(received), formatBytes(total), currentFile));
}

void RecordPlaybackPanel::onDownloadFinished(const QStringList&)
{
    downloadBusy_ = false;
    QString err;
    if (!buildPlaybackSequence(downloadedSelection_, &err)) {
        setStatus(err, true);
        updateUiEnabled();
        return;
    }
    setStatus(QString::fromUtf8("下载完成，播放序列已就绪"), false);
    updateUiEnabled();
}

void RecordPlaybackPanel::onDownloadFailed(const QString& error)
{
    downloadBusy_ = false;
    setStatus(error, true);
    updateUiEnabled();
}

QString RecordPlaybackPanel::cacheRoot() const
{
    return QDir("recordings").filePath("remote_cache");
}

QString RecordPlaybackPanel::recordCacheDir(const QString& type, const QString& recordId) const
{
    return QDir(cacheRoot()).filePath(type + "/" + recordId);
}

bool RecordPlaybackPanel::isRecordCached(const RecordItem& item, QString*) const
{
    const QString dir = recordCacheDir(item.type, item.recordId);
    for (const RecordFile& f : item.files) {
        QFileInfo info(QDir(dir).filePath(f.name));
        if (!info.exists() || !info.isFile() || static_cast<quint64>(info.size()) != f.sizeBytes) {
            return false;
        }
    }
    return true;
}

bool RecordPlaybackPanel::buildPlaybackSequence(const QVector<RecordItem>& items, QString* err)
{
    stopPlayback();
    playbackEntries_.clear();
    frameRefs_.clear();

    QVector<RecordItem> sorted = items;
    std::sort(sorted.begin(), sorted.end(), [](const RecordItem& a, const RecordItem& b) {
        return a.recordIdValue < b.recordIdValue;
    });

    for (const RecordItem& item : sorted) {
        PlaybackEntry entry;
        if (item.type == "raw") {
            if (!loadRawEntry(item, &entry, err)) return false;
        } else if (item.type == "tif") {
            if (!loadTifEntry(item, &entry, err)) return false;
        } else {
            if (err) *err = QString::fromUtf8("不支持的记录类型: %1").arg(item.type);
            return false;
        }
        playbackEntries_.append(entry);
    }

    rebuildFrameRefs();
    progressSlider_->setRange(0, frameRefs_.isEmpty() ? 0 : frameRefs_.size() - 1);
    currentFrame_ = 0;
    if (!frameRefs_.isEmpty()) {
        showFrame(0);
    }
    updateUiEnabled();
    return true;
}

bool RecordPlaybackPanel::loadRawEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const
{
    QString rawName;
    QString jsonName;
    for (const RecordFile& f : item.files) {
        if (f.name.endsWith(".raw", Qt::CaseInsensitive)) rawName = f.name;
        if (f.name.endsWith(".json", Qt::CaseInsensitive)) jsonName = f.name;
    }
    if (rawName.isEmpty() || jsonName.isEmpty()) {
        if (err) *err = QString::fromUtf8("raw 记录缺少 .raw 或 .json: %1").arg(item.recordId);
        return false;
    }

    const QString dir = recordCacheDir(item.type, item.recordId);
    const QString rawPath = QDir(dir).filePath(rawName);
    const QString jsonPath = QDir(dir).filePath(jsonName);
    QFile jf(jsonPath);
    if (!jf.open(QIODevice::ReadOnly)) {
        if (err) *err = QString::fromUtf8("无法读取 raw json: %1").arg(jsonPath);
        return false;
    }

    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(jf.readAll().constData());
    } catch (const std::exception& e) {
        if (err) *err = QString::fromUtf8("raw json 解析失败: %1").arg(e.what());
        return false;
    }

    const int width = jsonInt(meta, "width");
    const int height = jsonInt(meta, "height");
    const quint64 frameCount = jsonU64(meta, "frame_count");
    if (width <= 0 || height <= 0 || frameCount == 0 || frameCount > static_cast<quint64>(std::numeric_limits<int>::max())) {
        if (err) *err = QString::fromUtf8("raw 尺寸或帧数非法: %1").arg(item.recordId);
        return false;
    }
    if (!meta.contains("frames") || !meta["frames"].is_array() ||
        meta["frames"].size() < static_cast<std::size_t>(frameCount)) {
        if (err) *err = QString::fromUtf8("raw frames 数量不足: %1").arg(item.recordId);
        return false;
    }

    quint64 pixels = 0;
    quint64 bytesPerFrame = 0;
    quint64 expectedBytes = 0;
    if (!checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels) ||
        !checkedMul(pixels, 2, &bytesPerFrame) ||
        !checkedMul(bytesPerFrame, frameCount, &expectedBytes) ||
        bytesPerFrame > static_cast<quint64>(std::numeric_limits<int>::max()) ||
        expectedBytes > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        if (err) *err = QString::fromUtf8("raw 文件大小计算溢出: %1").arg(item.recordId);
        return false;
    }

    QFileInfo rawInfo(rawPath);
    if (!rawInfo.exists() || static_cast<quint64>(rawInfo.size()) != expectedBytes) {
        if (err) *err = QString::fromUtf8("raw 文件大小不匹配: %1").arg(item.recordId);
        return false;
    }

    PlaybackEntry e;
    e.type = "raw";
    e.recordId = item.recordId;
    e.recordIdValue = item.recordIdValue;
    e.rawPath = rawPath;
    e.jsonPath = jsonPath;
    e.width = width;
    e.height = height;
    e.frameCount = frameCount;
    e.bytesPerFrame = bytesPerFrame;
    e.frameTypes.reserve(static_cast<int>(frameCount));
    for (quint64 i = 0; i < frameCount; ++i) {
        e.frameTypes.append(frameTypeFromJson(meta["frames"][static_cast<std::size_t>(i)]));
    }
    *out = e;
    return true;
}

bool RecordPlaybackPanel::loadTifEntry(const RecordItem& item, PlaybackEntry* out, QString* err) const
{
    QString tifName;
    for (const RecordFile& f : item.files) {
        if (f.name.endsWith(".tif", Qt::CaseInsensitive) || f.name.endsWith(".tiff", Qt::CaseInsensitive)) {
            tifName = f.name;
            break;
        }
    }
    if (tifName.isEmpty()) {
        if (err) *err = QString::fromUtf8("tif 记录缺少 .tif 文件: %1").arg(item.recordId);
        return false;
    }
    PlaybackEntry e;
    e.type = "tif";
    e.recordId = item.recordId;
    e.recordIdValue = item.recordIdValue;
    e.tifPath = QDir(recordCacheDir(item.type, item.recordId)).filePath(tifName);
    e.frameCount = 1;
    if (!validateTif(e.tifPath, &e, err)) return false;
    *out = e;
    return true;
}

void RecordPlaybackPanel::rebuildFrameRefs()
{
    frameRefs_.clear();
    for (int i = 0; i < playbackEntries_.size(); ++i) {
        const PlaybackEntry& entry = playbackEntries_[i];
        const quint64 count = entry.type == "tif" ? 1 : entry.frameCount;
        for (quint64 f = 0; f < count; ++f) {
            FrameRef ref;
            ref.entryIndex = i;
            ref.frameIndex = f;
            frameRefs_.append(ref);
        }
    }
}

void RecordPlaybackPanel::showFrame(quint64 globalIndex)
{
    if (frameRefs_.isEmpty()) return;
    if (globalIndex >= static_cast<quint64>(frameRefs_.size())) globalIndex = frameRefs_.size() - 1;

    const FrameRef ref = frameRefs_[static_cast<int>(globalIndex)];
    const PlaybackEntry& entry = playbackEntries_[ref.entryIndex];
    if (entry.type == "tif") {
        const int maxBand = qMax(0, entry.pageCount - 1);
        const QList<QSpinBox*> bandSpins = {singleBandSpin_, rBandSpin_, gBandSpin_, bBandSpin_};
        const QList<QSpinBox*> rangeSpins = {
            rangeBeginSpin_, rangeEndSpin_, rBeginSpin_, rEndSpin_,
            gBeginSpin_, gEndSpin_, bBeginSpin_, bEndSpin_
        };
        for (QSpinBox* s : bandSpins) {
            QSignalBlocker blocker(s);
            s->setRange(0, maxBand);
        }
        for (QSpinBox* s : rangeSpins) {
            QSignalBlocker blocker(s);
            s->setRange(0, entry.pageCount);
        }
    }
    QImage image;
    QString info;
    QString err;
    const bool ok = entry.type == "raw"
        ? renderRawFrame(entry, ref.frameIndex, &image, &info, &err)
        : renderTifFrame(entry, &image, &info, &err);
    if (!ok) {
        setStatus(err, true);
        pausePlayback();
        return;
    }

    currentFrame_ = globalIndex;
    if (!sliderDragging_) progressSlider_->setValue(static_cast<int>(globalIndex));
    playbackInfoLbl_->setText(QString("%1/%2  %3")
        .arg(globalIndex + 1)
        .arg(frameRefs_.size())
        .arg(info));
    emit requestSwitchToPlaybackView();
    emit playbackImageReady(image, info);
}

bool RecordPlaybackPanel::renderRawFrame(const PlaybackEntry& entry, quint64 frameIndex,
                                         QImage* image, QString* info, QString* err) const
{
    quint64 offset = 0;
    if (!checkedMul(entry.bytesPerFrame, frameIndex, &offset) ||
        offset > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        if (err) *err = QString::fromUtf8("raw seek 偏移溢出");
        return false;
    }
    QFile f(entry.rawPath);
    if (!f.open(QIODevice::ReadOnly) || !f.seek(static_cast<qint64>(offset))) {
        if (err) *err = QString::fromUtf8("无法读取 raw 帧");
        return false;
    }
    const QByteArray data = f.read(static_cast<qint64>(entry.bytesPerFrame));
    if (static_cast<quint64>(data.size()) != entry.bytesPerFrame) {
        if (err) *err = QString::fromUtf8("raw 帧读取不完整");
        return false;
    }
    const QImage img = makeDisplayImage(entry.width, entry.height, cli::proto::Mono16, data);
    if (img.isNull()) {
        if (err) *err = QString::fromUtf8("raw 帧渲染失败");
        return false;
    }
    if (image) *image = img;
    if (info) {
        const quint8 ft = frameIndex < static_cast<quint64>(entry.frameTypes.size())
            ? entry.frameTypes[static_cast<int>(frameIndex)]
            : cli::proto::UnknownFrame;
        *info = QString("raw %1 frame=%2/%3 %4x%5 %6")
            .arg(entry.recordId)
            .arg(frameIndex + 1)
            .arg(entry.frameCount)
            .arg(entry.width)
            .arg(entry.height)
            .arg(frameTypeText(ft));
    }
    return true;
}

bool RecordPlaybackPanel::renderTifFrame(const PlaybackEntry& entry, QImage* image, QString* info, QString* err) const
{
    const int pageCount = entry.pageCount;
    const auto mode = static_cast<TifRenderMode>(renderModeCombo_->currentData().toInt());
    bool ok = false;
    if (mode == TifRenderMode::SingleBand) {
        int begin = 0, end = 0;
        ok = normalizeRange(pageCount, singleBandSpin_->value(), singleBandSpin_->value() + 1, &begin, &end, err) &&
             renderTifGray(entry.tifPath, pageCount, begin, end, image, err);
    } else if (mode == TifRenderMode::RangeAverage) {
        int begin = 0, end = 0;
        ok = normalizeRange(pageCount, rangeBeginSpin_->value(), rangeEndSpin_->value(), &begin, &end, err) &&
             renderTifGray(entry.tifPath, pageCount, begin, end, image, err);
    } else if (mode == TifRenderMode::RgbSingleBand) {
        ok = renderTifRgb(entry.tifPath, pageCount,
                          rBandSpin_->value(), rBandSpin_->value() + 1,
                          gBandSpin_->value(), gBandSpin_->value() + 1,
                          bBandSpin_->value(), bBandSpin_->value() + 1,
                          image, err);
    } else {
        ok = renderTifRgb(entry.tifPath, pageCount,
                          rBeginSpin_->value(), rEndSpin_->value(),
                          gBeginSpin_->value(), gEndSpin_->value(),
                          bBeginSpin_->value(), bEndSpin_->value(),
                          image, err);
    }
    if (!ok) return false;
    if (info) {
        *info = QString("tif %1 pages=%2 size=%3x%4")
            .arg(entry.recordId)
            .arg(entry.pageCount)
            .arg(entry.tifWidth)
            .arg(entry.tifHeight);
    }
    return true;
}

bool RecordPlaybackPanel::validateTif(const QString& path, PlaybackEntry* entry, QString* err) const
{
    TIFF* tif = TIFFOpen(path.toLocal8Bit().constData(), "r");
    if (!tif) {
        if (err) *err = QString::fromUtf8("无法打开 tif: %1").arg(path);
        return false;
    }

    uint32 width = 0, height = 0;
    uint16 bps = 0, spp = 0, photo = PHOTOMETRIC_MINISBLACK;
    uint32 firstW = 0, firstH = 0;
    int pages = 0;
    do {
        if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
            !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height) ||
            !TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bps) ||
            !TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &spp)) {
            TIFFClose(tif);
            if (err) *err = QString::fromUtf8("tif page 缺少必要 tag");
            return false;
        }
        TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photo);
        if (bps != 16 || spp != 1 ||
            (photo != PHOTOMETRIC_MINISBLACK && photo != PHOTOMETRIC_MINISWHITE)) {
            TIFFClose(tif);
            if (err) *err = QString::fromUtf8("tif 不是 16bit 单通道灰度");
            return false;
        }
        if (pages == 0) {
            firstW = width;
            firstH = height;
        } else if (width != firstW || height != firstH) {
            TIFFClose(tif);
            if (err) *err = QString::fromUtf8("tif page 尺寸不一致");
            return false;
        }
        quint64 pixels = 0;
        if (!checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels) ||
            pixels > static_cast<quint64>(std::numeric_limits<int>::max())) {
            TIFFClose(tif);
            if (err) *err = QString::fromUtf8("tif 尺寸过大");
            return false;
        }
        ++pages;
    } while (TIFFReadDirectory(tif));
    TIFFClose(tif);

    if (pages <= 0 || firstW == 0 || firstH == 0) {
        if (err) *err = QString::fromUtf8("tif 没有可用 page");
        return false;
    }
    entry->pageCount = pages;
    entry->tifWidth = static_cast<int>(firstW);
    entry->tifHeight = static_cast<int>(firstH);
    return true;
}

bool RecordPlaybackPanel::normalizeRange(int pageCount, int begin, int end,
                                         int* outBegin, int* outEnd, QString* err) const
{
    if (end == 0) end = pageCount;
    if (begin < 0 || begin >= pageCount || end <= begin || end > pageCount) {
        if (err) *err = QString::fromUtf8("波段范围非法，要求 0 <= begin < end <= page_count，end=0 表示末尾");
        return false;
    }
    if (outBegin) *outBegin = begin;
    if (outEnd) *outEnd = end;
    return true;
}

bool RecordPlaybackPanel::readTifPageU16(void* handle, int pageIndex, int width, int height,
                                         QVector<quint16>* out, QString* err) const
{
    TIFF* tif = static_cast<TIFF*>(handle);
    quint64 pixels64 = 0;
    if (!checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels64) ||
        pixels64 > static_cast<quint64>(std::numeric_limits<int>::max())) {
        if (err) *err = QString::fromUtf8("tif page 尺寸过大");
        return false;
    }
    if (!TIFFSetDirectory(tif, static_cast<tdir_t>(pageIndex))) {
        if (err) *err = QString::fromUtf8("无法定位 tif page %1").arg(pageIndex);
        return false;
    }
    const tsize_t scanline = TIFFScanlineSize(tif);
    if (scanline < width * static_cast<tsize_t>(sizeof(quint16))) {
        if (err) *err = QString::fromUtf8("tif scanline 尺寸异常");
        return false;
    }
    out->resize(width * height);
    QByteArray row(static_cast<int>(scanline), '\0');
    for (int y = 0; y < height; ++y) {
        if (TIFFReadScanline(tif, row.data(), y, 0) < 0) {
            if (err) *err = QString::fromUtf8("读取 tif scanline 失败");
            return false;
        }
        std::memcpy(out->data() + y * width, row.constData(), width * sizeof(quint16));
    }
    return true;
}

bool RecordPlaybackPanel::renderTifGray(const QString& path, int pageCount, int begin, int end,
                                        QImage* image, QString* err) const
{
    int b = 0, e = 0;
    if (!normalizeRange(pageCount, begin, end, &b, &e, err)) return false;
    TIFF* tif = TIFFOpen(path.toLocal8Bit().constData(), "r");
    if (!tif) {
        if (err) *err = QString::fromUtf8("无法打开 tif");
        return false;
    }
    uint32 width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    quint64 pixels64 = 0;
    if (!checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels64) ||
        pixels64 > static_cast<quint64>(std::numeric_limits<int>::max())) {
        TIFFClose(tif);
        if (err) *err = QString::fromUtf8("tif 尺寸过大");
        return false;
    }
    const int pixels = static_cast<int>(pixels64);
    QVector<quint64> acc(pixels, 0);
    QVector<quint16> page;
    for (int p = b; p < e; ++p) {
        if (!readTifPageU16(tif, p, static_cast<int>(width), static_cast<int>(height), &page, err)) {
            TIFFClose(tif);
            return false;
        }
        for (int i = 0; i < pixels; ++i) acc[i] += page[i];
    }
    TIFFClose(tif);

    const int count = e - b;
    int minV = std::numeric_limits<int>::max();
    int maxV = 0;
    QVector<int> values(pixels, 0);
    for (int i = 0; i < pixels; ++i) {
        values[i] = static_cast<int>(acc[i] / count);
        minV = std::min(minV, values[i]);
        maxV = std::max(maxV, values[i]);
    }
    QByteArray gray(pixels, '\0');
    if (maxV > minV) {
        const double scale = 255.0 / (maxV - minV);
        for (int i = 0; i < pixels; ++i) {
            gray[i] = static_cast<char>(qBound(0, static_cast<int>((values[i] - minV) * scale), 255));
        }
    }
    *image = QImage(reinterpret_cast<const uchar*>(gray.constData()),
                    static_cast<int>(width), static_cast<int>(height),
                    static_cast<int>(width), QImage::Format_Grayscale8).copy();
    return true;
}

bool RecordPlaybackPanel::renderTifRgb(const QString& path, int pageCount,
                                       int rBegin, int rEnd, int gBegin, int gEnd, int bBegin, int bEnd,
                                       QImage* image, QString* err) const
{
    int rb = 0, re = 0, gb = 0, ge = 0, bb = 0, be = 0;
    if (!normalizeRange(pageCount, rBegin, rEnd, &rb, &re, err) ||
        !normalizeRange(pageCount, gBegin, gEnd, &gb, &ge, err) ||
        !normalizeRange(pageCount, bBegin, bEnd, &bb, &be, err)) {
        return false;
    }

    TIFF* tif = TIFFOpen(path.toLocal8Bit().constData(), "r");
    if (!tif) {
        if (err) *err = QString::fromUtf8("无法打开 tif");
        return false;
    }
    uint32 width = 0, height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    quint64 pixels64 = 0;
    if (!checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels64) ||
        pixels64 > static_cast<quint64>(std::numeric_limits<int>::max())) {
        TIFFClose(tif);
        if (err) *err = QString::fromUtf8("tif 尺寸过大");
        return false;
    }
    const int pixels = static_cast<int>(pixels64);

    auto readRange = [&](int begin, int end, QVector<int>* out, QString* localErr) -> bool {
        QVector<quint64> acc(pixels, 0);
        QVector<quint16> page;
        for (int p = begin; p < end; ++p) {
            if (!readTifPageU16(tif, p, static_cast<int>(width), static_cast<int>(height), &page, localErr)) {
                return false;
            }
            for (int i = 0; i < pixels; ++i) acc[i] += page[i];
        }
        out->resize(pixels);
        const int count = end - begin;
        for (int i = 0; i < pixels; ++i) (*out)[i] = static_cast<int>(acc[i] / count);
        return true;
    };

    QVector<int> rv, gv, bv;
    if (!readRange(rb, re, &rv, err) || !readRange(gb, ge, &gv, err) || !readRange(bb, be, &bv, err)) {
        TIFFClose(tif);
        return false;
    }
    TIFFClose(tif);

    auto minmax = [](const QVector<int>& v, int* mn, int* mx) {
        *mn = std::numeric_limits<int>::max();
        *mx = 0;
        for (int x : v) {
            *mn = std::min(*mn, x);
            *mx = std::max(*mx, x);
        }
    };
    int rMin, rMax, gMin, gMax, bMin, bMax;
    minmax(rv, &rMin, &rMax);
    minmax(gv, &gMin, &gMax);
    minmax(bv, &bMin, &bMax);
    QByteArray rgb(pixels * 3, '\0');
    for (int i = 0; i < pixels; ++i) {
        const int out = i * 3;
        rgb[out] = static_cast<char>(rMax > rMin ? qBound(0, static_cast<int>((rv[i] - rMin) * 255.0 / (rMax - rMin)), 255) : 0);
        rgb[out + 1] = static_cast<char>(gMax > gMin ? qBound(0, static_cast<int>((gv[i] - gMin) * 255.0 / (gMax - gMin)), 255) : 0);
        rgb[out + 2] = static_cast<char>(bMax > bMin ? qBound(0, static_cast<int>((bv[i] - bMin) * 255.0 / (bMax - bMin)), 255) : 0);
    }
    *image = QImage(reinterpret_cast<const uchar*>(rgb.constData()),
                    static_cast<int>(width), static_cast<int>(height),
                    static_cast<int>(width) * 3, QImage::Format_RGB888).copy();
    return true;
}

void RecordPlaybackPanel::previousFrame()
{
    if (currentFrame_ == 0) return;
    showFrame(currentFrame_ - 1);
}

void RecordPlaybackPanel::nextFrame()
{
    if (frameRefs_.isEmpty()) return;
    if (currentFrame_ + 1 >= static_cast<quint64>(frameRefs_.size())) {
        if (loopChk_->isChecked()) showFrame(0);
        else pausePlayback();
        return;
    }
    showFrame(currentFrame_ + 1);
}

void RecordPlaybackPanel::startPlayback()
{
    if (frameRefs_.isEmpty()) return;
    playbackTimer_->start(intervalSpin_->value());
    emit requestSwitchToPlaybackView();
    updateUiEnabled();
}

void RecordPlaybackPanel::pausePlayback()
{
    if (playbackTimer_) playbackTimer_->stop();
    updateUiEnabled();
}

void RecordPlaybackPanel::stopPlayback()
{
    pausePlayback();
    if (!frameRefs_.isEmpty()) {
        showFrame(0);
    }
}

void RecordPlaybackPanel::onPlaybackTick()
{
    nextFrame();
}

void RecordPlaybackPanel::onSliderReleased()
{
    sliderDragging_ = false;
    showFrame(static_cast<quint64>(progressSlider_->value()));
}

void RecordPlaybackPanel::onRenderSettingsChanged()
{
    if (frameRefs_.isEmpty()) return;
    const FrameRef ref = frameRefs_[static_cast<int>(currentFrame_)];
    if (ref.entryIndex >= 0 && playbackEntries_[ref.entryIndex].type == "tif") {
        showFrame(currentFrame_);
    }
}

void RecordPlaybackPanel::updateRenderVisibility()
{
    const auto mode = static_cast<TifRenderMode>(renderModeCombo_->currentData().toInt());
    const bool single = mode == TifRenderMode::SingleBand;
    const bool range = mode == TifRenderMode::RangeAverage;
    const bool rgbSingle = mode == TifRenderMode::RgbSingleBand;
    const bool rgbRange = mode == TifRenderMode::RgbRangeAverage;

    auto* renderForm = qobject_cast<QFormLayout*>(singleBandSpin_->parentWidget()->layout());
    if (!renderForm) return;

    auto setFieldVisible = [renderForm](QWidget* field, bool visible) {
        if (!field) return;
        field->setVisible(visible);
        if (QLabel* label = qobject_cast<QLabel*>(renderForm->labelForField(field))) {
            label->setVisible(visible);
        }
    };

    setFieldVisible(singleBandSpin_, single);
    setFieldVisible(rangeBeginSpin_, range);
    setFieldVisible(rangeEndSpin_, range);
    setFieldVisible(rBandSpin_, rgbSingle || rgbRange);
    setFieldVisible(gBandSpin_, rgbSingle || rgbRange);
    setFieldVisible(bBandSpin_, rgbSingle || rgbRange);
    setFieldVisible(rBeginSpin_, rgbRange);
    setFieldVisible(rEndSpin_, rgbRange);
    setFieldVisible(gBeginSpin_, rgbRange);
    setFieldVisible(gEndSpin_, rgbRange);
    setFieldVisible(bBeginSpin_, rgbRange);
    setFieldVisible(bEndSpin_, rgbRange);
}

QString RecordPlaybackPanel::filesText(const RecordItem& item) const
{
    QStringList parts;
    for (const RecordFile& f : item.files) parts << f.name;
    return parts.join(", ");
}

QString RecordPlaybackPanel::formatBytes(quint64 bytes) const
{
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    if (bytes >= static_cast<quint64>(gb)) return QString::number(bytes / gb, 'f', 2) + " GiB";
    if (bytes >= static_cast<quint64>(mb)) return QString::number(bytes / mb, 'f', 2) + " MiB";
    if (bytes >= static_cast<quint64>(kb)) return QString::number(bytes / kb, 'f', 1) + " KiB";
    return QString::number(bytes) + " B";
}

QString RecordPlaybackPanel::frameTypeText(quint8 frameType) const
{
    using namespace cli::proto;
    if (frameType == HeaderFrame) return "HeaderFrame";
    if (frameType == DataFrame) return "DataFrame";
    if (frameType == TailFrame) return "TailFrame";
    return "UnknownFrame";
}
