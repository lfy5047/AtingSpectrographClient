#include "RecordPlaybackPanel.h"

#include "DeviceClient.h"
#include "ImageFrameUtils.h"
#include "Protocol.h"
#include "TifRenderWorker.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
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
#include <functional>
#include <limits>

#include "tiffio.h"

#include <QAbstractSpinBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QClipboard>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QShortcut>
#include <QSettings>
#include <QStyle>
#include <QTableWidgetItem>
#include <QUrl>

namespace {
const char* kSettingsPrefix = "recordPlayback/";

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
    if (j[key].is_string()) {
        bool ok = false;
        const quint64 v = QString::fromStdString(j[key].get<std::string>()).toULongLong(&ok);
        return ok ? v : def;
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

QIcon coloredDotIcon(const QColor& color)
{
    QPixmap pix(14, 14);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color.darker(125), 1));
    p.setBrush(color);
    p.drawEllipse(QRectF(2.0, 2.0, 10.0, 10.0));
    return QIcon(pix);
}

QString cacheKey(const QString& type, const QString& recordId)
{
    return type + "/" + recordId;
}
}

class SeekSlider : public QSlider {
public:
    using QSlider::QSlider;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (orientation() == Qt::Horizontal && event->button() == Qt::LeftButton && maximum() > minimum()) {
            const int span = qMax(1, width());
            const double ratio = qBound(0.0, static_cast<double>(event->pos().x()) / span, 1.0);
            setValue(minimum() + static_cast<int>((maximum() - minimum()) * ratio + 0.5));
            emit sliderReleased();
            event->accept();
            return;
        }
        QSlider::mousePressEvent(event);
    }
};

RecordPlaybackPanel::RecordPlaybackPanel(DeviceClient* device, QWidget* parent)
    : QWidget(parent)
    , device_(device)
{
    setupUi();
    qRegisterMetaType<QVector<RemoteFetchFile>>("QVector<RemoteFetchFile>");
    qRegisterMetaType<ChannelImageStats>("ChannelImageStats");
    qRegisterMetaType<RemoteDownloadErrorReason>("RemoteDownloadErrorReason");
    qRegisterMetaType<TifRenderRequest>("TifRenderRequest");

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
        retryAvailable_ = false;
        setRecordSelectionLocked(false);
        updateRecordCacheStatuses();
        refreshCacheInfo();
        setStatus(QString::fromUtf8("下载已取消"), false);
        updateUiEnabled();
    });

    tifRenderThread_ = new QThread(this);
    tifRenderWorker_ = new TifRenderWorker();
    tifRenderWorker_->moveToThread(tifRenderThread_);
    connect(tifRenderThread_, &QThread::finished, tifRenderWorker_, &QObject::deleteLater);
    connect(tifRenderWorker_, &TifRenderWorker::ready,
            this, &RecordPlaybackPanel::onTifRenderReady);
    connect(tifRenderWorker_, &TifRenderWorker::failed,
            this, &RecordPlaybackPanel::onTifRenderFailed);
    connect(tifRenderWorker_, &TifRenderWorker::canceled,
            this, &RecordPlaybackPanel::onTifRenderCanceled);
    connect(tifRenderWorker_, &TifRenderWorker::progress,
            this, &RecordPlaybackPanel::onTifRenderProgress);
    tifRenderThread_->start();

    if (device_) {
        connect(device_, &DeviceClient::connectionChanged, this,
                [this](bool connected, const QString& ip) {
            connected_ = connected;
            host_ = connected ? ip : QString();
            if (!connected) {
                cancelDownloadOnly();
                queryBusy_ = false;
                clearRecords();
                setStatus(QString::fromUtf8("连接已断开，当前本地回放仍可使用"), false);
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
    if (tifRenderThread_) {
        if (tifRenderWorker_) {
            tifRenderWorker_->requestCancel(tifRenderRequestId_ + 1);
        }
        tifRenderThread_->quit();
        tifRenderThread_->wait(2000);
        tifRenderThread_ = nullptr;
        tifRenderWorker_ = nullptr;
    }
}

void RecordPlaybackPanel::setupUi()
{
    setFocusPolicy(Qt::StrongFocus);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* retentionGroup = new QGroupBox(QString::fromUtf8("保留时间"), this);
    auto* retentionForm = new QFormLayout(retentionGroup);
    auto* retentionRow = new QHBoxLayout();
    retentionDaysSpin_ = new QSpinBox(this);
    retentionDaysSpin_->setRange(0, 30);
    retentionDaysSpin_->setSuffix(QString::fromUtf8(" 天"));
    retentionHoursSpin_ = new QSpinBox(this);
    retentionHoursSpin_->setRange(0, 23);
    retentionHoursSpin_->setSuffix(QString::fromUtf8(" 时"));
    retentionMinutesSpin_ = new QSpinBox(this);
    retentionMinutesSpin_->setRange(0, 59);
    retentionMinutesSpin_->setSuffix(QString::fromUtf8(" 分"));
    retentionSecondsSpin_ = new QSpinBox(this);
    retentionSecondsSpin_->setRange(0, 59);
    retentionSecondsSpin_->setSuffix(QString::fromUtf8(" 秒"));
    retentionRefreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    retentionApplyBtn_ = new QPushButton(QString::fromUtf8("设置"), this);
    retentionRow->addWidget(retentionDaysSpin_);
    retentionRow->addWidget(retentionHoursSpin_);
    retentionRow->addWidget(retentionMinutesSpin_);
    retentionRow->addWidget(retentionSecondsSpin_);
    retentionForm->addRow(QString::fromUtf8("服务端保留"), retentionRow);
    auto* retentionButtonRow = new QHBoxLayout();
    retentionButtonRow->addWidget(retentionRefreshBtn_);
    retentionButtonRow->addWidget(retentionApplyBtn_);
    retentionButtonRow->addStretch(1);
    retentionForm->addRow(QString::fromUtf8("操作"), retentionButtonRow);
    retentionTotalLbl_ = new QLabel(QString::fromUtf8("总计 0 秒"), this);
    retentionForm->addRow(QString::fromUtf8("换算"), retentionTotalLbl_);
    retentionInfoLbl_ = new QLabel("-", this);
    retentionInfoLbl_->setWordWrap(true);
    retentionForm->addRow(QString::fromUtf8("估算"), retentionInfoLbl_);
    root->addWidget(retentionGroup);

    auto* queryGroup = new QGroupBox(QString::fromUtf8("远程记录"), this);
    auto* queryLayout = new QVBoxLayout(queryGroup);

    openRecordsDialogBtn_ = new QPushButton(QString::fromUtf8("打开远程记录"), this);
    queryLayout->addWidget(openRecordsDialogBtn_);
    root->addWidget(queryGroup);

    recordsDialog_ = new QDialog(this);
    recordsDialog_->setObjectName("recordsDialog");
    recordsDialog_->setWindowTitle(QString::fromUtf8("远程数据记录"));
    recordsDialog_->resize(1100, 620);
    auto* recordsDialogLayout = new QVBoxLayout(recordsDialog_);

    typeCombo_ = new QComboBox(recordsDialog_);
    typeCombo_->addItem("raw", "raw");
    typeCombo_->addItem("tif", "tif");
    typeCombo_->setFixedWidth(130);

    queryModeCombo_ = new QComboBox(recordsDialog_);
    queryModeCombo_->addItem(QString::fromUtf8("最近数量"), "count");
    queryModeCombo_->addItem(QString::fromUtf8("最近秒数"), "seconds");
    queryModeCombo_->setFixedWidth(130);

    countSpin_ = new QSpinBox(recordsDialog_);
    countSpin_->setRange(1, 10000);
    countSpin_->setValue(10);
    countSpin_->setFixedWidth(110);

    secondsSpin_ = new QSpinBox(recordsDialog_);
    secondsSpin_->setRange(1, 24 * 3600 * 30);
    secondsSpin_->setValue(60);
    secondsSpin_->setSuffix(" s");
    secondsSpin_->setFixedWidth(130);

    queryBtn_ = new QPushButton(QString::fromUtf8("查询"), recordsDialog_);

    auto* queryControlsRow = new QHBoxLayout();
    queryControlsRow->addWidget(new QLabel(QString::fromUtf8("类型"), recordsDialog_));
    queryControlsRow->addWidget(typeCombo_);
    queryControlsRow->addSpacing(12);
    queryControlsRow->addWidget(new QLabel(QString::fromUtf8("查询模式"), recordsDialog_));
    queryControlsRow->addWidget(queryModeCombo_);
    queryControlsRow->addSpacing(12);
    queryControlsRow->addWidget(new QLabel(QString::fromUtf8("数量"), recordsDialog_));
    queryControlsRow->addWidget(countSpin_);
    queryControlsRow->addSpacing(12);
    queryControlsRow->addWidget(new QLabel(QString::fromUtf8("秒数"), recordsDialog_));
    queryControlsRow->addWidget(secondsSpin_);
    queryControlsRow->addSpacing(12);
    queryControlsRow->addWidget(queryBtn_);
    queryControlsRow->addStretch(1);
    recordsDialogLayout->addLayout(queryControlsRow);

    auto* selectionRow = new QHBoxLayout();
    selectAllBtn_ = new QPushButton(QString::fromUtf8("全选"), recordsDialog_);
    invertSelectionBtn_ = new QPushButton(QString::fromUtf8("反选"), recordsDialog_);
    clearSelectionBtn_ = new QPushButton(QString::fromUtf8("清除选择"), recordsDialog_);
    selectionRow->addWidget(selectAllBtn_);
    selectionRow->addWidget(invertSelectionBtn_);
    selectionRow->addWidget(clearSelectionBtn_);
    selectionRow->addStretch(1);
    recordsDialogLayout->addLayout(selectionRow);

    recordsTable_ = new QTableWidget(recordsDialog_);
    recordsTable_->setColumnCount(6);
    recordsTable_->setHorizontalHeaderLabels(QStringList()
        << "record_id" << "type" << "timestamp" << QString::fromUtf8("缓存状态")
        << QString::fromUtf8("文件") << QString::fromUtf8("大小"));
    recordsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recordsTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    recordsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recordsTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    recordsTable_->verticalHeader()->setVisible(false);
    recordsTable_->setAlternatingRowColors(true);
    recordsTable_->setShowGrid(false);
    recordsTable_->setMinimumSize(960, 420);
    recordsTable_->setStyleSheet(
        "QTableWidget::item:selected { background-color: #2F6FED; color: white; }"
        "QTableWidget::item:selected:!active { background-color: #2F6FED; color: white; }");
    recordsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    recordsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    recordsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    recordsDialogLayout->addWidget(recordsTable_);

    auto* downloadRow = new QHBoxLayout();
    downloadBtn_ = new QPushButton(QString::fromUtf8("下载/加入播放"), recordsDialog_);
    downloadBtn_->setProperty("primary", true);
    appendDownloadBtn_ = new QPushButton(QString::fromUtf8("下载并追加"), recordsDialog_);
    downloadInfoLbl_ = new QLabel("-", recordsDialog_);
    downloadInfoLbl_->setWordWrap(true);
    downloadRow->addWidget(downloadBtn_);
    downloadRow->addWidget(appendDownloadBtn_);
    downloadRow->addWidget(downloadInfoLbl_, 1);
    recordsDialogLayout->addLayout(downloadRow);

    renderGroup_ = new QGroupBox(QString::fromUtf8("tif 渲染参数"), this);
    auto* renderForm = new QFormLayout(renderGroup_);
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
    singleBandSlider_ = new QSlider(Qt::Horizontal, this);
    rangeBeginSlider_ = new QSlider(Qt::Horizontal, this);
    rangeEndSlider_ = new QSlider(Qt::Horizontal, this);
    rBandSlider_ = new QSlider(Qt::Horizontal, this);
    gBandSlider_ = new QSlider(Qt::Horizontal, this);
    bBandSlider_ = new QSlider(Qt::Horizontal, this);
    rBeginSlider_ = new QSlider(Qt::Horizontal, this);
    rEndSlider_ = new QSlider(Qt::Horizontal, this);
    gBeginSlider_ = new QSlider(Qt::Horizontal, this);
    gEndSlider_ = new QSlider(Qt::Horizontal, this);
    bBeginSlider_ = new QSlider(Qt::Horizontal, this);
    bEndSlider_ = new QSlider(Qt::Horizontal, this);
    rangeToLastChk_ = new QCheckBox(QString::fromUtf8("到最后一页"), this);
    rToLastChk_ = new QCheckBox(QString::fromUtf8("R 到最后一页"), this);
    gToLastChk_ = new QCheckBox(QString::fromUtf8("G 到最后一页"), this);
    bToLastChk_ = new QCheckBox(QString::fromUtf8("B 到最后一页"), this);
    tifRenderHintLbl_ = new QLabel(QString::fromUtf8("加载 TIF 后可设置波段"), this);
    cancelTifRenderBtn_ = new QPushButton(QString::fromUtf8("取消 TIF 渲染"), this);
    const QList<QSpinBox*> bandSpins = {
        singleBandSpin_, rangeBeginSpin_, rangeEndSpin_, rBandSpin_, gBandSpin_, bBandSpin_,
        rBeginSpin_, rEndSpin_, gBeginSpin_, gEndSpin_, bBeginSpin_, bEndSpin_
    };
    for (QSpinBox* s : bandSpins) {
        s->setRange(1, 1);
    }
    const QList<QSlider*> bandSliders = {
        singleBandSlider_, rangeBeginSlider_, rangeEndSlider_, rBandSlider_, gBandSlider_, bBandSlider_,
        rBeginSlider_, rEndSlider_, gBeginSlider_, gEndSlider_, bBeginSlider_, bEndSlider_
    };
    for (QSlider* s : bandSliders) {
        s->setRange(1, 1);
        s->setTracking(false);
    }
    auto addBandRow = [renderForm, this](const QString& label, const char* objectName, QSlider* slider, QSpinBox* spin) {
        auto* rowWidget = new QWidget(this);
        rowWidget->setObjectName(QString::fromLatin1(objectName));
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(slider, 1);
        row->addWidget(spin);
        renderForm->addRow(label, rowWidget);
    };
    renderForm->addRow(QString::fromUtf8("状态"), tifRenderHintLbl_);
    addBandRow(QString::fromUtf8("单波段"), "singleBandRow", singleBandSlider_, singleBandSpin_);
    addBandRow(QString::fromUtf8("起始波段"), "rangeBeginRow", rangeBeginSlider_, rangeBeginSpin_);
    addBandRow(QString::fromUtf8("结束波段"), "rangeEndRow", rangeEndSlider_, rangeEndSpin_);
    renderForm->addRow("", rangeToLastChk_);
    addBandRow("R Band", "rBandRow", rBandSlider_, rBandSpin_);
    addBandRow("G Band", "gBandRow", gBandSlider_, gBandSpin_);
    addBandRow("B Band", "bBandRow", bBandSlider_, bBandSpin_);
    addBandRow("R begin", "rBeginRow", rBeginSlider_, rBeginSpin_);
    addBandRow("R end", "rEndRow", rEndSlider_, rEndSpin_);
    renderForm->addRow("", rToLastChk_);
    addBandRow("G begin", "gBeginRow", gBeginSlider_, gBeginSpin_);
    addBandRow("G end", "gEndRow", gEndSlider_, gEndSpin_);
    renderForm->addRow("", gToLastChk_);
    addBandRow("B begin", "bBeginRow", bBeginSlider_, bBeginSpin_);
    addBandRow("B end", "bEndRow", bEndSlider_, bEndSpin_);
    renderForm->addRow("", bToLastChk_);
    renderForm->addRow("", cancelTifRenderBtn_);
    root->addWidget(renderGroup_);

    updateRenderVisibility();

    auto* playbackGroup = new QGroupBox(QString::fromUtf8("播放"), this);
    auto* playbackForm = new QFormLayout(playbackGroup);
    auto* btnRow = new QHBoxLayout();
    prevBtn_ = new QPushButton(QString::fromUtf8("上一帧"), this);
    playBtn_ = new QPushButton(QString::fromUtf8("播放"), this);
    playBtn_->setProperty("primary", true);
    pauseBtn_ = new QPushButton(QString::fromUtf8("暂停"), this);
    stopBtn_ = new QPushButton(QString::fromUtf8("回到开头"), this);
    stopBtn_->setProperty("danger", true);
    nextBtn_ = new QPushButton(QString::fromUtf8("下一帧"), this);
    btnRow->addWidget(prevBtn_);
    btnRow->addWidget(playBtn_);
    btnRow->addWidget(pauseBtn_);
    btnRow->addWidget(stopBtn_);
    btnRow->addWidget(nextBtn_);
    playbackForm->addRow(btnRow);
    progressSlider_ = new SeekSlider(Qt::Horizontal, this);
    progressSlider_->setRange(0, 0);
    playbackForm->addRow(QString::fromUtf8("进度"), progressSlider_);

    frameSpin_ = new QSpinBox(this);
    frameSpin_->setRange(0, 0);
    exportFrameBtn_ = new QPushButton(QString::fromUtf8("导出当前帧"), this);
    auto* frameRow = new QHBoxLayout();
    frameRow->addWidget(frameSpin_);
    frameRow->addWidget(exportFrameBtn_);
    playbackForm->addRow(QString::fromUtf8("当前帧"), frameRow);

    intervalSpin_ = new QSpinBox(this);
    intervalSpin_->setRange(10, 600000);
    intervalSpin_->setValue(200);
    intervalSpin_->setSuffix(" ms");
    fpsLbl_ = new QLabel("-", this);
    loopChk_ = new QCheckBox(QString::fromUtf8("循环播放"), this);
    badFramePolicyCombo_ = new QComboBox(this);
    badFramePolicyCombo_->addItem(QString::fromUtf8("坏帧暂停并提示"), static_cast<int>(BadFramePolicy::Pause));
    badFramePolicyCombo_->addItem(QString::fromUtf8("坏帧跳过继续"), static_cast<int>(BadFramePolicy::Skip));
    clearSkippedBadFramesBtn_ = new QPushButton(QString::fromUtf8("清除跳过计数"), this);
    auto* intervalRow = new QHBoxLayout();
    intervalRow->addWidget(intervalSpin_);
    intervalRow->addWidget(fpsLbl_);
    intervalRow->addWidget(loopChk_);
    playbackForm->addRow(QString::fromUtf8("间隔"), intervalRow);
    auto* badFrameRow = new QHBoxLayout();
    badFrameRow->addWidget(badFramePolicyCombo_);
    badFrameRow->addWidget(clearSkippedBadFramesBtn_);
    playbackForm->addRow(QString::fromUtf8("坏帧策略"), badFrameRow);
    playbackInfoLbl_ = new QLabel("0/0", this);
    playbackInfoLbl_->setWordWrap(true);
    playbackForm->addRow(QString::fromUtf8("当前位置"), playbackInfoLbl_);
    root->addWidget(playbackGroup);

    playbackSequenceGroup_ = new QGroupBox(QString::fromUtf8("播放序列"), this);
    playbackSequenceGroup_->setCheckable(true);
    playbackSequenceGroup_->setChecked(false);
    auto* seqLayout = new QVBoxLayout(playbackSequenceGroup_);
    playbackSequenceTable_ = new QTableWidget(this);
    playbackSequenceTable_->setColumnCount(5);
    playbackSequenceTable_->setHorizontalHeaderLabels(QStringList()
        << "record_id" << "type" << QString::fromUtf8("帧/page") << QString::fromUtf8("缓存路径") << QString::fromUtf8("大小"));
    playbackSequenceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    playbackSequenceTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    playbackSequenceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playbackSequenceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    playbackSequenceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    playbackSequenceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    playbackSequenceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    playbackSequenceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    seqLayout->addWidget(playbackSequenceTable_);
    auto* seqBtnRow = new QHBoxLayout();
    playbackSeqRemoveBtn_ = new QPushButton(QString::fromUtf8("移除选中"), this);
    playbackSeqClearBtn_ = new QPushButton(QString::fromUtf8("清空序列"), this);
    seqBtnRow->addWidget(playbackSeqRemoveBtn_);
    seqBtnRow->addWidget(playbackSeqClearBtn_);
    seqBtnRow->addStretch(1);
    seqLayout->addLayout(seqBtnRow);
    playbackSequenceTable_->setVisible(false);
    playbackSeqRemoveBtn_->setVisible(false);
    playbackSeqClearBtn_->setVisible(false);
    root->addWidget(playbackSequenceGroup_);

    auto* cacheGroup = new QGroupBox(QString::fromUtf8("本地缓存"), this);
    auto* cacheLayout = new QVBoxLayout(cacheGroup);
    cacheInfoLbl_ = new QLabel("-", this);
    cacheInfoLbl_->setWordWrap(true);
    cacheLayout->addWidget(cacheInfoLbl_);
    auto* cacheBtnRow = new QHBoxLayout();
    cacheRefreshBtn_ = new QPushButton(QString::fromUtf8("刷新缓存大小"), this);
    cacheOpenBtn_ = new QPushButton(QString::fromUtf8("打开缓存目录"), this);
    cacheClearUnusedBtn_ = new QPushButton(QString::fromUtf8("清理非当前播放缓存"), this);
    cacheClearAllBtn_ = new QPushButton(QString::fromUtf8("清理全部缓存"), this);
    cacheBtnRow->addWidget(cacheRefreshBtn_);
    cacheBtnRow->addWidget(cacheOpenBtn_);
    cacheBtnRow->addStretch(1);
    cacheLayout->addLayout(cacheBtnRow);
    auto* cacheClearRow = new QHBoxLayout();
    cacheClearRow->addWidget(cacheClearUnusedBtn_);
    cacheClearRow->addWidget(cacheClearAllBtn_);
    cacheClearRow->addStretch(1);
    cacheLayout->addLayout(cacheClearRow);
    root->addWidget(cacheGroup);

    auto* statusGroup = new QGroupBox(QString::fromUtf8("状态"), this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusLbl_ = new QLabel("-", this);
    statusLbl_->setWordWrap(true);
    statusLayout->addWidget(statusLbl_);
    root->addWidget(statusGroup);

    playbackTimer_ = new QTimer(this);
    tifRenderDebounceTimer_ = new QTimer(this);
    tifRenderDebounceTimer_->setSingleShot(true);
    tifRenderDebounceTimer_->setInterval(200);
    tifRenderTimeoutTimer_ = new QTimer(this);
    tifRenderTimeoutTimer_->setSingleShot(true);

    connect(retentionRefreshBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::refreshRetention);
    connect(retentionApplyBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::applyRetention);
    const QList<QSpinBox*> retentionSpins = {
        retentionDaysSpin_, retentionHoursSpin_, retentionMinutesSpin_, retentionSecondsSpin_
    };
    for (QSpinBox* s : retentionSpins) {
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RecordPlaybackPanel::updateRetentionTotalLabel);
    }
    connect(queryBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::queryRecords);
    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
    });
    connect(queryModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecordPlaybackPanel::updateUiEnabled);
    connect(queryModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
    });
    connect(countSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        saveSettings();
    });
    connect(secondsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        saveSettings();
    });
    connect(openRecordsDialogBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::showRecordsDialog);
    connect(downloadBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::downloadSelected);
    connect(appendDownloadBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::appendSelectedToPlaybackSequence);
    connect(selectAllBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::selectAllRecords);
    connect(invertSelectionBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::invertRecordSelection);
    connect(clearSelectionBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::clearRecordSelection);
    connect(recordsTable_, &QTableWidget::itemSelectionChanged,
            this, &RecordPlaybackPanel::updateUiEnabled);
    connect(recordsTable_, &QTableWidget::itemDoubleClicked,
            this, &RecordPlaybackPanel::showRecordDetails);
    connect(recordsTable_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTableWidgetItem* item = recordsTable_->itemAt(pos);
        if (!item) return;
        QMenu menu(recordsTable_);
        QAction* details = menu.addAction(QString::fromUtf8("查看详情"));
        QAction* chosen = menu.exec(recordsTable_->viewport()->mapToGlobal(pos));
        if (chosen == details) showRecordDetails(item);
    });
    connect(prevBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::previousFrame);
    connect(nextBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::nextFrame);
    connect(playBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::startPlayback);
    connect(pauseBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::pausePlayback);
    connect(stopBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::stopPlayback);
    connect(progressSlider_, &QSlider::sliderPressed, this, [this]() {
        sliderDragging_ = true;
        progressWasPlayingBeforeDrag_ = playbackTimer_ && playbackTimer_->isActive();
        if (progressWasPlayingBeforeDrag_) playbackTimer_->stop();
    });
    connect(progressSlider_, &QSlider::sliderReleased, this, &RecordPlaybackPanel::onSliderReleased);
    connect(frameSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecordPlaybackPanel::onFrameSpinChanged);
    connect(exportFrameBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::exportCurrentFrame);
    connect(intervalSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (playbackTimer_ && playbackTimer_->isActive()) playbackTimer_->setInterval(value);
        saveSettings();
        updateUiEnabled();
    });
    connect(loopChk_, &QCheckBox::toggled, this, [this](bool) {
        saveSettings();
    });
    connect(badFramePolicyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
    });
    connect(playbackTimer_, &QTimer::timeout, this, &RecordPlaybackPanel::onPlaybackTick);
    connect(tifRenderDebounceTimer_, &QTimer::timeout, this, [this]() {
        if (!frameRefs_.isEmpty()) seekToFrame(currentFrame_, SeekTrigger::RenderSettings, false);
    });
    connect(tifRenderTimeoutTimer_, &QTimer::timeout, this, &RecordPlaybackPanel::onTifRenderTimeout);

    connect(cacheRefreshBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::refreshCacheInfo);
    connect(cacheOpenBtn_, &QPushButton::clicked, this, [this]() {
        QDir().mkpath(cacheRoot());
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(cacheRoot()).absolutePath()));
    });
    connect(cacheClearUnusedBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::clearNonPlaybackCache);
    connect(cacheClearAllBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::clearAllCache);
    connect(clearSkippedBadFramesBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::clearSkippedBadFrames);
    connect(cancelTifRenderBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::cancelTifRender);
    connect(playbackSequenceGroup_, &QGroupBox::toggled, this, &RecordPlaybackPanel::togglePlaybackSequenceVisible);
    connect(playbackSeqRemoveBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::removeSelectedPlaybackRecords);
    connect(playbackSeqClearBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::clearPlaybackSequence);
    connect(playbackSequenceTable_, &QTableWidget::itemSelectionChanged,
            this, &RecordPlaybackPanel::updateUiEnabled);

    connect(renderModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecordPlaybackPanel::onRenderSettingsChanged);
    connect(renderModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecordPlaybackPanel::updateRenderVisibility);
    connect(renderModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
    });
    for (QSpinBox* s : bandSpins) {
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RecordPlaybackPanel::scheduleTifRerender);
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            saveSettings();
        });
    }
    const QList<QSlider*> bandSlidersForConnect = {
        singleBandSlider_, rangeBeginSlider_, rangeEndSlider_, rBandSlider_, gBandSlider_, bBandSlider_,
        rBeginSlider_, rEndSlider_, gBeginSlider_, gEndSlider_, bBeginSlider_, bEndSlider_
    };
    const QList<QSpinBox*> matchingSpins = {
        singleBandSpin_, rangeBeginSpin_, rangeEndSpin_, rBandSpin_, gBandSpin_, bBandSpin_,
        rBeginSpin_, rEndSpin_, gBeginSpin_, gEndSpin_, bBeginSpin_, bEndSpin_
    };
    for (int i = 0; i < bandSlidersForConnect.size(); ++i) {
        QSlider* slider = bandSlidersForConnect[i];
        QSpinBox* spin = matchingSpins[i];
        connect(slider, &QSlider::valueChanged, this, [spin](int value) {
            QSignalBlocker blocker(spin);
            spin->setValue(value);
        });
        connect(slider, &QSlider::sliderReleased, this, [this]() {
            saveSettings();
            scheduleTifRerender();
        });
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [slider, this](int value) {
            QSignalBlocker blocker(slider);
            slider->setValue(value);
            scheduleTifRerender();
        });
    }
    const QList<QCheckBox*> toLastChecks = { rangeToLastChk_, rToLastChk_, gToLastChk_, bToLastChk_ };
    for (QCheckBox* c : toLastChecks) {
        connect(c, &QCheckBox::toggled, this, &RecordPlaybackPanel::scheduleTifRerender);
        connect(c, &QCheckBox::toggled, this, &RecordPlaybackPanel::updateRenderVisibility);
        connect(c, &QCheckBox::toggled, this, [this](bool) {
            saveSettings();
        });
    }

    auto shortcutAllowed = [this]() {
        QWidget* fw = focusWidget();
        return !qobject_cast<QAbstractSpinBox*>(fw) && !qobject_cast<QLineEdit*>(fw);
    };
    auto addShortcut = [this, shortcutAllowed](const QKeySequence& key, const std::function<void()>& fn) {
        auto* shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [shortcutAllowed, fn]() {
            if (shortcutAllowed()) fn();
        });
    };
    addShortcut(QKeySequence(Qt::Key_Space), [this]() {
        if (playbackRequested_ || (playbackTimer_ && playbackTimer_->isActive())) pausePlayback();
        else startPlayback();
    });
    addShortcut(QKeySequence(Qt::Key_Left), [this]() { previousFrame(); });
    addShortcut(QKeySequence(Qt::Key_Right), [this]() { nextFrame(); });
    addShortcut(QKeySequence(Qt::Key_Home), [this]() {
        if (!frameRefs_.isEmpty()) seekToFrame(0, SeekTrigger::Manual, false);
    });
    addShortcut(QKeySequence(Qt::Key_End), [this]() {
        if (!frameRefs_.isEmpty()) seekToFrame(static_cast<quint64>(frameRefs_.size() - 1), SeekTrigger::Manual, false);
    });

    loadSettings();
    updateRetentionTotalLabel();
    refreshCacheInfo();
}

void RecordPlaybackPanel::setStatus(const QString& text, bool isError)
{
    statusLbl_->setText(text.isEmpty() ? "-" : text);
    statusLbl_->setStyleSheet(isError ? "color:#E5484D;" : "");
}

void RecordPlaybackPanel::updateUiEnabled()
{
    const bool busy = downloadBusy_ || queryBusy_;
    const bool hasConnection = connected_ && device_ && device_->isConnected();
    const bool hasFrames = !frameRefs_.isEmpty();
    const bool playing = playbackRequested_ || (playbackTimer_ && playbackTimer_->isActive());
    const bool hasSelection = !selectedRecords().isEmpty();

    retentionRefreshBtn_->setEnabled(hasConnection && !busy);
    retentionApplyBtn_->setEnabled(hasConnection && !busy);
    retentionDaysSpin_->setEnabled(!busy);
    const bool retentionSubFieldsEnabled = !busy && retentionDaysSpin_->value() < retentionDaysSpin_->maximum();
    retentionHoursSpin_->setEnabled(retentionSubFieldsEnabled);
    retentionMinutesSpin_->setEnabled(retentionSubFieldsEnabled);
    retentionSecondsSpin_->setEnabled(retentionSubFieldsEnabled);
    typeCombo_->setEnabled(!busy);
    if (queryModeCombo_) queryModeCombo_->setEnabled(!busy);
    const bool queryBySeconds = queryModeCombo_ && queryModeCombo_->currentData().toString() == "seconds";
    countSpin_->setEnabled(!busy && !queryBySeconds);
    secondsSpin_->setEnabled(!busy && queryBySeconds);
    queryBtn_->setEnabled(hasConnection && !busy);
    openRecordsDialogBtn_->setEnabled(true);
    downloadBtn_->setEnabled(downloadBusy_ || (hasConnection && !queryBusy_ && hasSelection));
    if (appendDownloadBtn_) appendDownloadBtn_->setEnabled(hasConnection && !busy && hasSelection);
    selectAllBtn_->setEnabled(!busy && recordsTable_->rowCount() > 0);
    invertSelectionBtn_->setEnabled(!busy && recordsTable_->rowCount() > 0);
    clearSelectionBtn_->setEnabled(!busy && hasSelection);

    prevBtn_->setEnabled(hasFrames && !tifRenderBusy_);
    nextBtn_->setEnabled(hasFrames && !tifRenderBusy_);
    playBtn_->setEnabled(hasFrames && !playing && !tifRenderBusy_);
    pauseBtn_->setEnabled(playing);
    stopBtn_->setEnabled(hasFrames);
    progressSlider_->setEnabled(hasFrames && !tifRenderBusy_);
    frameSpin_->setEnabled(hasFrames && !tifRenderBusy_);
    exportFrameBtn_->setEnabled(!currentPlaybackImage_.isNull());
    cancelTifRenderBtn_->setEnabled(tifRenderBusy_);
    const bool hasSequenceSelection = playbackSequenceTable_
        && playbackSequenceTable_->selectionModel()
        && !playbackSequenceTable_->selectionModel()->selectedRows().isEmpty();
    if (playbackSeqRemoveBtn_) playbackSeqRemoveBtn_->setEnabled(hasSequenceSelection);
    if (playbackSeqClearBtn_) playbackSeqClearBtn_->setEnabled(!playbackRecordItems_.isEmpty());
    cacheRefreshBtn_->setEnabled(true);
    cacheOpenBtn_->setEnabled(true);
    cacheClearUnusedBtn_->setEnabled(!downloadBusy_);
    cacheClearAllBtn_->setEnabled(!downloadBusy_);
    if (fpsLbl_) {
        const double fps = intervalSpin_->value() > 0 ? 1000.0 / intervalSpin_->value() : 0.0;
        fpsLbl_->setText(QString("%1 FPS").arg(QString::number(fps, 'f', 2)));
    }
    updateDownloadButtonText();
}

void RecordPlaybackPanel::setComboByData(QComboBox* combo, const QVariant& data, int fallbackIndex)
{
    if (!combo) return;
    int index = combo->findData(data);
    if (index < 0) index = qBound(0, fallbackIndex, combo->count() - 1);
    if (index >= 0) combo->setCurrentIndex(index);
}

void RecordPlaybackPanel::loadSettings()
{
    loadingSettings_ = true;
    QSettings s;
    const QString p = QString::fromLatin1(kSettingsPrefix);

    setComboByData(typeCombo_, s.value(p + "query/type", "raw"), 0);
    setComboByData(queryModeCombo_, s.value(p + "query/mode", "count"), 0);
    countSpin_->setValue(s.value(p + "query/count", 10).toInt());
    secondsSpin_->setValue(s.value(p + "query/seconds", 60).toInt());

    setComboByData(renderModeCombo_, s.value(p + "tif/mode", static_cast<int>(TifRenderMode::SingleBand)), 0);
    pendingTifBandSettings_ = {
        s.value(p + "tif/singleBand", 1).toInt(),
        s.value(p + "tif/rangeBegin", 1).toInt(),
        s.value(p + "tif/rangeEnd", 1).toInt(),
        s.value(p + "tif/rBand", 1).toInt(),
        s.value(p + "tif/gBand", 1).toInt(),
        s.value(p + "tif/bBand", 1).toInt(),
        s.value(p + "tif/rBegin", 1).toInt(),
        s.value(p + "tif/rEnd", 1).toInt(),
        s.value(p + "tif/gBegin", 1).toInt(),
        s.value(p + "tif/gEnd", 1).toInt(),
        s.value(p + "tif/bBegin", 1).toInt(),
        s.value(p + "tif/bEnd", 1).toInt(),
    };
    pendingTifBandSettingsValid_ = true;
    rangeToLastChk_->setChecked(s.value(p + "tif/rangeToLast", false).toBool());
    rToLastChk_->setChecked(s.value(p + "tif/rToLast", false).toBool());
    gToLastChk_->setChecked(s.value(p + "tif/gToLast", false).toBool());
    bToLastChk_->setChecked(s.value(p + "tif/bToLast", false).toBool());

    intervalSpin_->setValue(s.value(p + "playback/intervalMs", 200).toInt());
    loopChk_->setChecked(s.value(p + "playback/loop", false).toBool());
    setComboByData(badFramePolicyCombo_,
                   s.value(p + "playback/badFramePolicy", static_cast<int>(BadFramePolicy::Pause)), 0);
    playbackSequenceGroup_->setChecked(s.value(p + "ui/playbackSequenceExpanded", false).toBool());

    loadingSettings_ = false;
    updateRenderVisibility();
    updateUiEnabled();
}

void RecordPlaybackPanel::saveSettings() const
{
    if (loadingSettings_) return;
    QSettings s;
    const QString p = QString::fromLatin1(kSettingsPrefix);

    s.setValue(p + "query/type", typeCombo_->currentData());
    s.setValue(p + "query/mode", queryModeCombo_->currentData());
    s.setValue(p + "query/count", countSpin_->value());
    s.setValue(p + "query/seconds", secondsSpin_->value());

    s.setValue(p + "tif/mode", renderModeCombo_->currentData());
    const QVector<int> tifBandValues = pendingTifBandSettingsValid_ && pendingTifBandSettings_.size() == 12
        ? pendingTifBandSettings_
        : QVector<int>{
            singleBandSpin_->value(),
            rangeBeginSpin_->value(),
            rangeEndSpin_->value(),
            rBandSpin_->value(),
            gBandSpin_->value(),
            bBandSpin_->value(),
            rBeginSpin_->value(),
            rEndSpin_->value(),
            gBeginSpin_->value(),
            gEndSpin_->value(),
            bBeginSpin_->value(),
            bEndSpin_->value(),
        };
    s.setValue(p + "tif/singleBand", tifBandValues[0]);
    s.setValue(p + "tif/rangeBegin", tifBandValues[1]);
    s.setValue(p + "tif/rangeEnd", tifBandValues[2]);
    s.setValue(p + "tif/rBand", tifBandValues[3]);
    s.setValue(p + "tif/gBand", tifBandValues[4]);
    s.setValue(p + "tif/bBand", tifBandValues[5]);
    s.setValue(p + "tif/rBegin", tifBandValues[6]);
    s.setValue(p + "tif/rEnd", tifBandValues[7]);
    s.setValue(p + "tif/gBegin", tifBandValues[8]);
    s.setValue(p + "tif/gEnd", tifBandValues[9]);
    s.setValue(p + "tif/bBegin", tifBandValues[10]);
    s.setValue(p + "tif/bEnd", tifBandValues[11]);
    s.setValue(p + "tif/rangeToLast", rangeToLastChk_->isChecked());
    s.setValue(p + "tif/rToLast", rToLastChk_->isChecked());
    s.setValue(p + "tif/gToLast", gToLastChk_->isChecked());
    s.setValue(p + "tif/bToLast", bToLastChk_->isChecked());

    s.setValue(p + "playback/intervalMs", intervalSpin_->value());
    s.setValue(p + "playback/loop", loopChk_->isChecked());
    s.setValue(p + "playback/badFramePolicy", badFramePolicyCombo_->currentData());
    s.setValue(p + "ui/playbackSequenceExpanded", playbackSequenceGroup_->isChecked());
}

void RecordPlaybackPanel::cancelRemoteWork()
{
    cancelDownloadOnly();
    queryBusy_ = false;
    if (playbackTimer_) playbackTimer_->stop();
    setPlaybackSequenceCleared();
    updateUiEnabled();
}

void RecordPlaybackPanel::updateDownloadButtonText()
{
    if (!downloadBtn_) return;
    if (downloadBusy_) {
        downloadBtn_->setText(QString::fromUtf8("取消下载"));
        return;
    }
    if (retryAvailable_) {
        downloadBtn_->setText(QString::fromUtf8("重试下载"));
        return;
    }

    const QVector<RecordItem> selected = retryAvailable_ && !pendingDownloadSelection_.isEmpty()
        ? pendingDownloadSelection_
        : selectedRecords();
    if (selected.isEmpty()) {
        downloadBtn_->setText(QString::fromUtf8("下载/加入播放"));
        return;
    }

    bool allCached = true;
    for (const RecordItem& item : selected) {
        const CacheState state = cacheState(item);
        if (state != CacheState::Cached && state != CacheState::CachedNoManifest) {
            allCached = false;
            break;
        }
    }
    downloadBtn_->setText(allCached
        ? QString::fromUtf8("加入播放")
        : QString::fromUtf8("下载并播放"));
}

void RecordPlaybackPanel::showRecordsDialog()
{
    if (!recordsDialog_) return;
    recordsDialog_->show();
    recordsDialog_->raise();
    recordsDialog_->activateWindow();
}

void RecordPlaybackPanel::setRecordSelectionLocked(bool locked)
{
    if (!recordsTable_ || recordSelectionLocked_ == locked) return;
    recordSelectionLocked_ = locked;
    recordsTable_->setSelectionMode(locked
        ? QAbstractItemView::NoSelection
        : QAbstractItemView::ExtendedSelection);
}

void RecordPlaybackPanel::cancelDownloadOnly()
{
    if (downloader_ && downloadBusy_) {
        QMetaObject::invokeMethod(downloader_, "cancel", Qt::QueuedConnection);
        downloadBusy_ = false;
    }
    setRecordSelectionLocked(false);
}

bool RecordPlaybackPanel::selectedRecordsType(const QVector<RecordItem>& items, QString* type, QString* err) const
{
    if (items.isEmpty()) return false;
    const QString firstType = items.first().type;
    for (const RecordItem& item : items) {
        if (item.type != firstType) {
            if (err) {
                *err = QString::fromUtf8("选中了 raw 和 tif 两种类型记录，请只选择同一类型后再下载");
            }
            return false;
        }
    }
    if (type) *type = firstType;
    return true;
}

RecordPlaybackPanel::CacheState RecordPlaybackPanel::cacheState(const RecordItem& item) const
{
    const QString dir = recordCacheDir(item.type, item.recordId);
    bool anyExists = false;
    bool allValid = true;
    for (const RecordFile& f : item.files) {
        const QFileInfo info(QDir(dir).filePath(f.name));
        if (info.exists()) anyExists = true;
        if (!info.exists() || !info.isFile() || static_cast<quint64>(info.size()) != f.sizeBytes) {
            allValid = false;
        }
    }
    if (allValid) {
        nlohmann::json manifest;
        if (!readCacheManifest(item, &manifest)) return CacheState::CachedNoManifest;
        return manifestFilesMatch(item, manifest) ? CacheState::Cached : CacheState::PossiblyStale;
    }
    return anyExists ? CacheState::Incomplete : CacheState::Missing;
}

QString RecordPlaybackPanel::cacheStateText(CacheState state) const
{
    if (state == CacheState::Cached) return QString::fromUtf8("已缓存");
    if (state == CacheState::CachedNoManifest) return QString::fromUtf8("已缓存(无清单)");
    if (state == CacheState::Incomplete) return QString::fromUtf8("不完整");
    if (state == CacheState::PossiblyStale) return QString::fromUtf8("可能过期");
    return QString::fromUtf8("未下载");
}

QIcon RecordPlaybackPanel::cacheStateIcon(CacheState state) const
{
    if (state == CacheState::Cached) {
        return style()->standardIcon(QStyle::SP_DialogApplyButton);
    }
    if (state == CacheState::CachedNoManifest) {
        return coloredDotIcon(QColor(60, 150, 215));
    }
    if (state == CacheState::Incomplete || state == CacheState::PossiblyStale) {
        return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    return coloredDotIcon(QColor(145, 145, 145));
}

QString RecordPlaybackPanel::manifestPath(const RecordItem& item) const
{
    return QDir(recordCacheDir(item.type, item.recordId)).filePath("manifest.json");
}

bool RecordPlaybackPanel::readCacheManifest(const RecordItem& item, nlohmann::json* manifest) const
{
    QFile f(manifestPath(item));
    if (!f.open(QIODevice::ReadOnly)) return false;
    try {
        if (manifest) *manifest = nlohmann::json::parse(f.readAll().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool RecordPlaybackPanel::manifestFilesMatch(const RecordItem& item, const nlohmann::json& manifest) const
{
    if (!manifest.contains("files") || !manifest["files"].is_array()) return false;
    QHash<QString, quint64> expected;
    for (const RecordFile& f : item.files) expected.insert(f.name, f.sizeBytes);
    if (manifest["files"].size() != static_cast<std::size_t>(expected.size())) return false;
    for (const auto& jf : manifest["files"]) {
        const QString name = jsonString(jf, "name");
        const quint64 size = jsonU64(jf, "size_bytes", 0);
        if (!expected.contains(name) || expected.value(name) != size) return false;
    }
    return true;
}

bool RecordPlaybackPanel::writeCacheManifest(const RecordItem& item, QString* err) const
{
    QDir dir(recordCacheDir(item.type, item.recordId));
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        if (err) *err = QString::fromUtf8("无法创建缓存目录: %1").arg(dir.absolutePath());
        return false;
    }
    nlohmann::json manifest;
    manifest["manifest_version"] = 1;
    manifest["type"] = item.type.toStdString();
    manifest["record_id"] = item.recordId.toStdString();
    manifest["timestamp_ns"] = item.timestampNs;
    manifest["downloaded_at_ms"] = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
    manifest["files"] = nlohmann::json::array();
    for (const RecordFile& f : item.files) {
        manifest["files"].push_back({
            {"name", f.name.toStdString()},
            {"size_bytes", f.sizeBytes},
        });
    }

    QFile out(manifestPath(item));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString::fromUtf8("无法写入缓存清单: %1").arg(out.fileName());
        return false;
    }
    out.write(QByteArray::fromStdString(manifest.dump(2)));
    return true;
}

void RecordPlaybackPanel::updateRecordCacheStatuses()
{
    if (!recordsTable_) return;
    for (int row = 0; row < records_.size() && row < recordsTable_->rowCount(); ++row) {
        const RecordItem& item = records_[row];
        QTableWidgetItem* tableItem = recordsTable_->item(row, 3);
        if (!tableItem) {
            tableItem = new QTableWidgetItem();
            recordsTable_->setItem(row, 3, tableItem);
        }
        const CacheState state = cacheState(item);
        tableItem->setText(cacheStateText(state));
        tableItem->setIcon(cacheStateIcon(state));
        tableItem->setToolTip(QDir(recordCacheDir(item.type, item.recordId)).absolutePath());
    }
    updateDownloadButtonText();
}

QString RecordPlaybackPanel::formatRecordTimestamp(quint64 timestampNs) const
{
    if (timestampNs == 0) return "-";
    const qint64 ms = static_cast<qint64>(timestampNs / 1000000ULL);
    return QDateTime::fromMSecsSinceEpoch(ms).toLocalTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

quint64 RecordPlaybackPanel::retentionSecondsFromInputs() const
{
    quint64 seconds = static_cast<quint64>(retentionDaysSpin_->value()) * 24ULL * 3600ULL;
    seconds += static_cast<quint64>(retentionHoursSpin_->value()) * 3600ULL;
    seconds += static_cast<quint64>(retentionMinutesSpin_->value()) * 60ULL;
    seconds += static_cast<quint64>(retentionSecondsSpin_->value());
    return seconds;
}

void RecordPlaybackPanel::setRetentionInputsFromSeconds(quint64 seconds)
{
    QSignalBlocker b1(retentionDaysSpin_);
    QSignalBlocker b2(retentionHoursSpin_);
    QSignalBlocker b3(retentionMinutesSpin_);
    QSignalBlocker b4(retentionSecondsSpin_);
    retentionDaysSpin_->setValue(static_cast<int>(qMin<quint64>(seconds / 86400ULL, retentionDaysSpin_->maximum())));
    seconds %= 86400ULL;
    retentionHoursSpin_->setValue(static_cast<int>(seconds / 3600ULL));
    seconds %= 3600ULL;
    retentionMinutesSpin_->setValue(static_cast<int>(seconds / 60ULL));
    retentionSecondsSpin_->setValue(static_cast<int>(seconds % 60ULL));
    updateRetentionTotalLabel();
}

void RecordPlaybackPanel::updateRetentionTotalLabel()
{
    if (!retentionTotalLbl_) return;
    if (retentionDaysSpin_->value() >= retentionDaysSpin_->maximum()) {
        QSignalBlocker b1(retentionHoursSpin_);
        QSignalBlocker b2(retentionMinutesSpin_);
        QSignalBlocker b3(retentionSecondsSpin_);
        retentionHoursSpin_->setValue(0);
        retentionMinutesSpin_->setValue(0);
        retentionSecondsSpin_->setValue(0);
        retentionHoursSpin_->setEnabled(false);
        retentionMinutesSpin_->setEnabled(false);
        retentionSecondsSpin_->setEnabled(false);
    } else {
        const bool busy = downloadBusy_ || queryBusy_;
        retentionHoursSpin_->setEnabled(!busy);
        retentionMinutesSpin_->setEnabled(!busy);
        retentionSecondsSpin_->setEnabled(!busy);
    }
    retentionTotalLbl_->setText(QString::fromUtf8("总计 %1 秒").arg(retentionSecondsFromInputs()));
}

void RecordPlaybackPanel::setPlaybackSequenceCleared()
{
    cancelTifRender();
    playbackEntries_.clear();
    frameRefs_.clear();
    playbackRecordItems_.clear();
    currentFrame_ = 0;
    currentPlaybackImage_ = QImage();
    currentPlaybackStats_ = ChannelImageStats();
    currentPlaybackInfo_.clear();
    progressSlider_->setRange(0, 0);
    frameSpin_->setRange(0, 0);
    frameSpin_->setValue(0);
    playbackInfoLbl_->setText("0/0");
    updatePlaybackSequenceTable();
}

QString RecordPlaybackPanel::currentExportFileName() const
{
    if (frameRefs_.isEmpty() || currentFrame_ >= static_cast<quint64>(frameRefs_.size())) {
        return "record_frame.png";
    }
    const FrameRef ref = frameRefs_[static_cast<int>(currentFrame_)];
    const PlaybackEntry& entry = playbackEntries_[ref.entryIndex];
    QString name = QString("record_%1_frame_%2").arg(entry.recordId).arg(ref.frameIndex + 1);
    if (entry.type == "tif") {
        name += QString("_mode_%1").arg(renderModeCombo_->currentText().replace(' ', '_'));
    }
    return name + ".png";
}

ChannelImageStats RecordPlaybackPanel::imageStatsFromDisplayImage(const QImage& image) const
{
    ChannelImageStats stats;
    if (image.isNull()) return stats;
    quint64 sum = 0;
    quint16 minV = std::numeric_limits<quint16>::max();
    quint16 maxV = 0;
    const QImage img = image.convertToFormat(QImage::Format_ARGB32);
    const int pixels = img.width() * img.height();
    if (pixels <= 0) return stats;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = row[x];
            const quint16 v = static_cast<quint16>((qRed(px) + qGreen(px) + qBlue(px)) / 3);
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            sum += v;
        }
    }
    stats.valid = true;
    stats.min = minV;
    stats.max = maxV;
    stats.avg = static_cast<double>(sum) / pixels;
    return stats;
}

QSet<QString> RecordPlaybackPanel::playbackCacheKeys() const
{
    QSet<QString> keys;
    for (const PlaybackEntry& entry : playbackEntries_) {
        keys.insert(cacheKey(entry.type, entry.recordId));
    }
    return keys;
}

RecordPlaybackPanel::CacheSummary RecordPlaybackPanel::summarizeCache(const QSet<QString>& keepKeys, bool onlyRemovable) const
{
    CacheSummary summary;
    const QDir root(cacheRoot());
    if (!root.exists()) return summary;
    const QFileInfoList typeDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& typeInfo : typeDirs) {
        const QDir typeDir(typeInfo.absoluteFilePath());
        const QFileInfoList recordDirs = typeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& recordInfo : recordDirs) {
            const QString key = cacheKey(typeInfo.fileName(), recordInfo.fileName());
            if (onlyRemovable && keepKeys.contains(key)) continue;
            ++summary.recordCount;
            const QDir recordDir(recordInfo.absoluteFilePath());
            const QFileInfoList files = recordDir.entryInfoList(QDir::Files);
            summary.fileCount += files.size();
            for (const QFileInfo& file : files) {
                summary.totalBytes += static_cast<quint64>(qMax<qint64>(0, file.size()));
            }
        }
    }
    return summary;
}

bool RecordPlaybackPanel::removeCacheRecords(const QSet<QString>& keepKeys, bool removeAll,
                                             CacheSummary* removed, QString* err)
{
    if (removed) *removed = CacheSummary();
    QDir root(cacheRoot());
    if (!root.exists()) return true;
    const QString rootPath = QFileInfo(root.absolutePath()).absoluteFilePath();
    const QFileInfoList typeDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& typeInfo : typeDirs) {
        QDir typeDir(typeInfo.absoluteFilePath());
        const QFileInfoList recordDirs = typeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& recordInfo : recordDirs) {
            const QString key = cacheKey(typeInfo.fileName(), recordInfo.fileName());
            if (!removeAll && keepKeys.contains(key)) continue;
            const QString recordPath = recordInfo.absoluteFilePath();
            if (!QFileInfo(recordPath).absoluteFilePath().startsWith(rootPath)) {
                if (err) *err = QString::fromUtf8("缓存路径异常: %1").arg(recordPath);
                return false;
            }
            QDir recordDir(recordPath);
            CacheSummary local;
            local.recordCount = 1;
            const QFileInfoList files = recordDir.entryInfoList(QDir::Files);
            local.fileCount = files.size();
            for (const QFileInfo& file : files) {
                local.totalBytes += static_cast<quint64>(qMax<qint64>(0, file.size()));
            }
            if (!recordDir.removeRecursively()) {
                if (err) *err = QString::fromUtf8("无法删除缓存目录: %1").arg(recordPath);
                return false;
            }
            if (removed) {
                removed->recordCount += local.recordCount;
                removed->fileCount += local.fileCount;
                removed->totalBytes += local.totalBytes;
            }
        }
        root.rmdir(typeInfo.fileName());
    }
    return true;
}

bool RecordPlaybackPanel::prepareForCacheMutation(QString* err)
{
    if (!cancelTifRenderAndWait(2000, err)) return false;
    return true;
}

void RecordPlaybackPanel::refreshRetention()
{
    if (!device_ || !device_->record()) {
        setStatus(QString::fromUtf8("保留时间刷新失败：未连接服务端"), true);
        if (retentionInfoLbl_) retentionInfoLbl_->setText(QString::fromUtf8("刷新失败：未连接服务端"));
        return;
    }
    device_->record()->getRetention(this, [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            const QString message = err.trimmed().isEmpty()
                ? QString::fromUtf8("保留时间刷新失败：服务端未返回错误信息")
                : QString::fromUtf8("保留时间刷新失败：%1").arg(err);
            setStatus(message, true);
            if (retentionInfoLbl_) retentionInfoLbl_->setText(message);
            return;
        }
        const quint64 seconds = jsonU64(data, "retention_seconds", 0);
        currentRetentionSeconds_ = seconds;
        hasCurrentRetention_ = true;
        setRetentionInputsFromSeconds(seconds);
        retentionInfoLbl_->setText(QString::fromUtf8("当前=%1 秒 raw估算=%2 tif估算=%3")
            .arg(seconds)
            .arg(formatBytes(jsonU64(data, "raw_estimated_bytes", 0)))
            .arg(formatBytes(jsonU64(data, "tif_estimated_bytes", 0))));
        setStatus(QString::fromUtf8("保留时间已刷新"), false);
    });
}

void RecordPlaybackPanel::applyRetention()
{
    if (!device_ || !device_->record()) {
        setStatus(QString::fromUtf8("保留时间设置失败：未连接服务端"), true);
        if (retentionInfoLbl_) retentionInfoLbl_->setText(QString::fromUtf8("设置失败：未连接服务端"));
        return;
    }
    const quint64 seconds = retentionSecondsFromInputs();
    if (seconds == 0 || (hasCurrentRetention_ && seconds < currentRetentionSeconds_)) {
        const QString current = hasCurrentRetention_
            ? QString::number(currentRetentionSeconds_)
            : QString::fromUtf8("未知");
        const QString msg = QString::fromUtf8(
            "当前服务端保留时间为 %1 秒，设为 %2 秒后服务端可能按新策略清理数据，是否继续？")
            .arg(current)
            .arg(seconds);
        if (QMessageBox::question(this, QString::fromUtf8("确认设置保留时间"), msg)
            != QMessageBox::Yes) {
            return;
        }
    }
    const QString applyingText = QString::fromUtf8("正在设置保留时间为 %1 秒...").arg(seconds);
    setStatus(applyingText, false);
    if (retentionInfoLbl_) retentionInfoLbl_->setText(applyingText);
    device_->record()->setRetention(this, seconds, [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            const QString message = err.trimmed().isEmpty()
                ? QString::fromUtf8("保留时间设置失败：服务端未返回错误信息")
                : QString::fromUtf8("保留时间设置失败：%1").arg(err);
            setStatus(message, true);
            if (retentionInfoLbl_) retentionInfoLbl_->setText(message);
            return;
        }
        const quint64 s = jsonU64(data, "retention_seconds", 0);
        currentRetentionSeconds_ = s;
        hasCurrentRetention_ = true;
        setRetentionInputsFromSeconds(s);
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
    if (queryBusy_) return;
    const QString type = typeCombo_->currentData().toString();
    const bool queryBySeconds = queryModeCombo_ && queryModeCombo_->currentData().toString() == "seconds";
    const quint64 count = queryBySeconds ? 0 : static_cast<quint64>(countSpin_->value());
    const quint64 seconds = queryBySeconds ? static_cast<quint64>(secondsSpin_->value()) : 0;
    queryBusy_ = true;
    setStatus(queryBySeconds
        ? QString::fromUtf8("正在按最近 %1 秒查询远程记录...").arg(seconds)
        : QString::fromUtf8("正在按最近 %1 条查询远程记录...").arg(count), false);
    updateUiEnabled();
    device_->record()->listRecent(this, type, count, seconds,
        [this](bool ok, const nlohmann::json& data, const QString& err) {
        queryBusy_ = false;
        if (!connected_) {
            updateUiEnabled();
            return;
        }
        if (!ok) {
            setStatus(err, true);
            updateUiEnabled();
            return;
        }
        QVector<RecordItem> items;
        QString parseErr;
        if (!parseRecordList(data, &items, &parseErr)) {
            setStatus(parseErr, true);
            updateUiEnabled();
            return;
        }
        clearRecords();
        records_ = items;
        for (const RecordItem& item : records_) {
            addRecordRow(item);
        }
        updateRecordCacheStatuses();
        setStatus(records_.isEmpty()
            ? QString::fromUtf8("未查询到记录")
            : QString::fromUtf8("查询到 %1 条记录").arg(records_.size()), false);
        updateUiEnabled();
    });
}

void RecordPlaybackPanel::clearRecords()
{
    records_.clear();
    recordsTable_->setRowCount(0);
    updateUiEnabled();
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
    auto* tsItem = new QTableWidgetItem(formatRecordTimestamp(item.timestampNs));
    tsItem->setToolTip(QString("timestamp_ns=%1").arg(item.timestampNs));
    recordsTable_->setItem(row, 2, tsItem);
    recordsTable_->setItem(row, 3, new QTableWidgetItem());
    recordsTable_->setItem(row, 4, new QTableWidgetItem(filesText(item)));
    quint64 size = 0;
    for (const RecordFile& f : item.files) size += f.sizeBytes;
    recordsTable_->setItem(row, 5, new QTableWidgetItem(formatBytes(size)));
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
    if (downloadBusy_) {
        cancelDownloadOnly();
        setStatus(QString::fromUtf8("正在取消下载..."), false);
        updateUiEnabled();
        return;
    }
    const QVector<RecordItem> selected = selectedRecords();
    if (selected.isEmpty()) return;
    if (host_.isEmpty()) {
        setStatus(QString::fromUtf8("未连接服务端"), true);
        return;
    }
    QString selectedType;
    QString typeErr;
    if (!selectedRecordsType(selected, &selectedType, &typeErr)) {
        setStatus(typeErr, true);
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
    pendingDownloadSelection_ = selected;
    pendingDownloadType_ = selectedType;

    if (missing.isEmpty()) {
        setPlaybackSequence(downloadedSelection_, appendAfterDownload_);
        appendAfterDownload_ = false;
        automaticDownloadRetryCount_ = 0;
        retryAvailable_ = false;
        setStatus(QString::fromUtf8("已复用本地缓存，播放序列已更新"), false);
        return;
    }

    QStringList ids;
    for (const RecordItem& item : missing) ids << item.recordId;
    downloadBusy_ = true;
    retryAvailable_ = false;
    setRecordSelectionLocked(true);
    setStatus(QString::fromUtf8("正在准备下载远程记录..."), false);
    updateUiEnabled();
    device_->record()->fetch(this, selectedType, ids,
        [this, selectedType](bool ok, const nlohmann::json& data, const QString& rpcErr) {
        if (!downloadBusy_) return;
        if (!ok) {
            downloadBusy_ = false;
            setRecordSelectionLocked(false);
            onDownloadFailed(RemoteDownloadErrorReason::Protocol, rpcErr);
            updateUiEnabled();
            return;
        }
        const QString transferId = jsonString(data, "transfer_id");
        const quint16 filePort = static_cast<quint16>(jsonU64(data, "file_port", 0));
        if (transferId.isEmpty() || filePort == 0 || !data.contains("files") || !data["files"].is_array()) {
            downloadBusy_ = false;
            setRecordSelectionLocked(false);
            onDownloadFailed(RemoteDownloadErrorReason::Protocol, QString::fromUtf8("record.fetch 响应无效"));
            updateUiEnabled();
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
        const QString root = cacheRoot();
        const bool invoked = QMetaObject::invokeMethod(
            downloader_, "start", Qt::QueuedConnection,
            Q_ARG(QString, host_),
            Q_ARG(quint16, filePort),
            Q_ARG(QString, transferId),
            Q_ARG(QString, selectedType),
            Q_ARG(QVector<RemoteFetchFile>, fetchFiles),
            Q_ARG(QString, root));
        if (!invoked) {
            downloadBusy_ = false;
            setRecordSelectionLocked(false);
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
    const QString pct = total > 0
        ? QString("  %1%").arg(QString::number(received * 100.0 / total, 'f', 1))
        : QString();
    downloadInfoLbl_->setText(QString("%1 / %2%3  %4")
        .arg(formatBytes(received), formatBytes(total), pct, currentFile));
}

void RecordPlaybackPanel::onDownloadFinished(const QStringList&)
{
    downloadBusy_ = false;
    setRecordSelectionLocked(false);
    QString err;
    for (const RecordItem& item : downloadedSelection_) {
        writeCacheManifest(item, nullptr);
    }
    setPlaybackSequence(downloadedSelection_, appendAfterDownload_);
    appendAfterDownload_ = false;
    automaticDownloadRetryCount_ = 0;
    retryAvailable_ = false;
    if (frameRefs_.isEmpty() && !downloadedSelection_.isEmpty()) {
        setStatus(err, true);
        updateUiEnabled();
        return;
    }
    updateRecordCacheStatuses();
    refreshCacheInfo();
    setStatus(QString::fromUtf8("下载完成，播放序列已更新"), false);
    updateUiEnabled();
}

void RecordPlaybackPanel::onDownloadFailed(RemoteDownloadErrorReason reason, const QString& error)
{
    downloadBusy_ = false;
    setRecordSelectionLocked(false);
    if (reason != RemoteDownloadErrorReason::UserCanceled && automaticDownloadRetryCount_ < 1 && !pendingDownloadSelection_.isEmpty()) {
        ++automaticDownloadRetryCount_;
        setStatus(QString::fromUtf8("下载失败，正在自动重试 1 次: %1").arg(error), true);
        downloadBusy_ = false;
        QTimer::singleShot(0, this, [this]() {
            downloadedSelection_ = pendingDownloadSelection_;
            downloadSelected();
        });
        return;
    }
    retryAvailable_ = reason != RemoteDownloadErrorReason::UserCanceled && !pendingDownloadSelection_.isEmpty();
    updateRecordCacheStatuses();
    refreshCacheInfo();
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
    const CacheState state = cacheState(item);
    return state == CacheState::Cached || state == CacheState::CachedNoManifest;
}

QVector<RecordPlaybackPanel::RecordItem> RecordPlaybackPanel::currentPlaybackRecordItems() const
{
    return playbackRecordItems_;
}

void RecordPlaybackPanel::setPlaybackSequence(const QVector<RecordItem>& items, bool append)
{
    QVector<RecordItem> next = append ? playbackRecordItems_ : QVector<RecordItem>();
    int skipped = 0;
    QSet<QString> seen;
    for (const RecordItem& item : next) seen.insert(cacheKey(item.type, item.recordId));
    for (const RecordItem& item : items) {
        const QString key = cacheKey(item.type, item.recordId);
        if (seen.contains(key)) {
            ++skipped;
            continue;
        }
        seen.insert(key);
        next.append(item);
    }
    QString err;
    if (!buildPlaybackSequence(next, &err)) {
        setStatus(err, true);
        return;
    }
    playbackRecordItems_ = next;
    updatePlaybackSequenceTable();
    resetSkippedBadFrames();
    if (skipped > 0) {
        setStatus(QString::fromUtf8("已跳过 %1 条已在播放序列中的记录").arg(skipped), false);
    }
}

QVector<RecordPlaybackPanel::RecordItem> RecordPlaybackPanel::selectedPlaybackRecords() const
{
    QVector<RecordItem> result;
    if (!playbackSequenceTable_) return result;
    QSet<int> rows;
    if (playbackSequenceTable_->selectionModel()) {
        for (const QModelIndex& index : playbackSequenceTable_->selectionModel()->selectedRows()) {
            rows.insert(index.row());
        }
    }
    if (rows.isEmpty()) {
        for (QTableWidgetItem* item : playbackSequenceTable_->selectedItems()) rows.insert(item->row());
    }
    const QList<int> rowList = rows.values();
    for (int row : rowList) {
        if (row >= 0 && row < playbackRecordItems_.size()) result.append(playbackRecordItems_[row]);
    }
    return result;
}

void RecordPlaybackPanel::updatePlaybackSequenceTable()
{
    if (!playbackSequenceTable_) return;
    playbackSequenceTable_->setRowCount(0);
    for (const RecordItem& item : playbackRecordItems_) {
        const int row = playbackSequenceTable_->rowCount();
        playbackSequenceTable_->insertRow(row);
        playbackSequenceTable_->setItem(row, 0, new QTableWidgetItem(item.recordId));
        playbackSequenceTable_->setItem(row, 1, new QTableWidgetItem(item.type));
        QString countText = "-";
        for (const PlaybackEntry& entry : playbackEntries_) {
            if (entry.type == item.type && entry.recordId == item.recordId) {
                countText = entry.type == "tif"
                    ? QString::fromUtf8("%1 page").arg(entry.pageCount)
                    : QString::fromUtf8("%1 帧").arg(entry.frameCount);
                break;
            }
        }
        playbackSequenceTable_->setItem(row, 2, new QTableWidgetItem(countText));
        playbackSequenceTable_->setItem(row, 3, new QTableWidgetItem(QDir(recordCacheDir(item.type, item.recordId)).absolutePath()));
        quint64 size = 0;
        for (const RecordFile& f : item.files) size += f.sizeBytes;
        playbackSequenceTable_->setItem(row, 4, new QTableWidgetItem(formatBytes(size)));
    }
    updateUiEnabled();
}

bool RecordPlaybackPanel::buildPlaybackSequence(const QVector<RecordItem>& items, QString* err)
{
    pausePlayback();
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
    frameSpin_->setRange(frameRefs_.isEmpty() ? 0 : 1, frameRefs_.isEmpty() ? 0 : frameRefs_.size());
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
    seekToFrame(globalIndex, SeekTrigger::Manual, false);
}

void RecordPlaybackPanel::seekToFrame(quint64 globalIndex, SeekTrigger trigger, bool resumeIfWasPlaying)
{
    if (frameRefs_.isEmpty()) return;
    if (globalIndex >= static_cast<quint64>(frameRefs_.size())) globalIndex = frameRefs_.size() - 1;

    const FrameRef ref = frameRefs_[static_cast<int>(globalIndex)];
    const PlaybackEntry& entry = playbackEntries_[ref.entryIndex];
    if (entry.type == "tif") {
        updateTifBandRanges(entry);
        updateRenderVisibility();
        submitTifRenderRequest(globalIndex, trigger, resumeIfWasPlaying);
        return;
    }
    cancelTifRender();
    updateRenderVisibility();
    QImage image;
    QString info;
    QString err;
    ChannelImageStats stats;
    const bool ok = renderRawFrame(entry, ref.frameIndex, &image, &info, &stats, &err);
    if (!ok) {
        handleFrameError(globalIndex, ref, entry, err);
        return;
    }
    finishDisplayedFrame(globalIndex, image, info, stats);
    if (resumeIfWasPlaying && playbackRequested_ && playbackTimer_) {
        playbackTimer_->start(intervalSpin_->value());
        updateUiEnabled();
    }
}

void RecordPlaybackPanel::finishDisplayedFrame(quint64 globalIndex, const QImage& image,
                                               const QString& info, const ChannelImageStats& stats)
{
    currentFrame_ = globalIndex;
    if (!sliderDragging_) progressSlider_->setValue(static_cast<int>(globalIndex));
    {
        QSignalBlocker blocker(frameSpin_);
        frameSpin_->setValue(static_cast<int>(globalIndex + 1));
    }
    currentPlaybackImage_ = image;
    currentPlaybackStats_ = stats;
    currentPlaybackInfo_ = info;
    playbackInfoLbl_->setText(QString("%1/%2  %3")
        .arg(globalIndex + 1)
        .arg(frameRefs_.size())
        .arg(info));
    emit requestSwitchToPlaybackView();
    emit playbackImageReady(image, info, stats);
    updateUiEnabled();
}

int RecordPlaybackPanel::bandValue0(const BandSelector& selector) const
{
    return qMax(0, selector.spin ? selector.spin->value() - 1 : 0);
}

int RecordPlaybackPanel::rangeEndValueExclusive(const BandSelector& selector, const QCheckBox* toLast, int pageCount) const
{
    if (toLast && toLast->isChecked()) return pageCount;
    return qBound(1, selector.spin ? selector.spin->value() : 1, pageCount);
}

bool RecordPlaybackPanel::currentTifRenderArgs(int pageCount, int* mode, int* a, int* b, int* c, int* d, int* e, int* f, QString* err) const
{
    const int m = renderModeCombo_->currentData().toInt();
    if (mode) *mode = m;
    auto requireRange = [pageCount, err](int begin, int end) {
        if (begin < 0 || begin >= pageCount || end <= begin || end > pageCount) {
            if (err) *err = QString::fromUtf8("波段范围非法");
            return false;
        }
        return true;
    };

    if (m == static_cast<int>(TifRenderMode::SingleBand)) {
        const int begin = bandValue0({singleBandSlider_, singleBandSpin_, pageCount});
        if (!requireRange(begin, begin + 1)) return false;
        if (a) *a = begin;
        if (b) *b = begin + 1;
    } else if (m == static_cast<int>(TifRenderMode::RangeAverage)) {
        const int begin = bandValue0({rangeBeginSlider_, rangeBeginSpin_, pageCount});
        const int end = rangeEndValueExclusive({rangeEndSlider_, rangeEndSpin_, pageCount}, rangeToLastChk_, pageCount);
        if (!requireRange(begin, end)) return false;
        if (a) *a = begin;
        if (b) *b = end;
    } else if (m == static_cast<int>(TifRenderMode::RgbSingleBand)) {
        const int rb = bandValue0({rBandSlider_, rBandSpin_, pageCount});
        const int gb = bandValue0({gBandSlider_, gBandSpin_, pageCount});
        const int bb = bandValue0({bBandSlider_, bBandSpin_, pageCount});
        if (!requireRange(rb, rb + 1) || !requireRange(gb, gb + 1) || !requireRange(bb, bb + 1)) return false;
        if (a) *a = rb;
        if (b) *b = rb + 1;
        if (c) *c = gb;
        if (d) *d = gb + 1;
        if (e) *e = bb;
        if (f) *f = bb + 1;
    } else {
        const int rb = bandValue0({rBeginSlider_, rBeginSpin_, pageCount});
        const int re = rangeEndValueExclusive({rEndSlider_, rEndSpin_, pageCount}, rToLastChk_, pageCount);
        const int gb = bandValue0({gBeginSlider_, gBeginSpin_, pageCount});
        const int ge = rangeEndValueExclusive({gEndSlider_, gEndSpin_, pageCount}, gToLastChk_, pageCount);
        const int bb = bandValue0({bBeginSlider_, bBeginSpin_, pageCount});
        const int be = rangeEndValueExclusive({bEndSlider_, bEndSpin_, pageCount}, bToLastChk_, pageCount);
        if (!requireRange(rb, re) || !requireRange(gb, ge) || !requireRange(bb, be)) return false;
        if (a) *a = rb;
        if (b) *b = re;
        if (c) *c = gb;
        if (d) *d = ge;
        if (e) *e = bb;
        if (f) *f = be;
    }
    return true;
}

void RecordPlaybackPanel::submitTifRenderRequest(quint64 globalIndex, SeekTrigger trigger, bool resumeWhenReady)
{
    if (!tifRenderWorker_ || frameRefs_.isEmpty()) return;
    const FrameRef ref = frameRefs_[static_cast<int>(globalIndex)];
    const PlaybackEntry& entry = playbackEntries_[ref.entryIndex];
    int mode = 0, a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    QString err;
    if (!currentTifRenderArgs(entry.pageCount, &mode, &a, &b, &c, &d, &e, &f, &err)) {
        handleFrameError(globalIndex, ref, entry, err);
        return;
    }

    const quint64 requestId = ++tifRenderRequestId_;
    activeTifRenderRequestId_ = requestId;
    activeTifRenderFrame_ = globalIndex;
    activeTifRenderTrigger_ = trigger;
    tifRenderResumeWhenReady_ = resumeWhenReady;
    tifRenderBusy_ = true;
    if (playbackTimer_) playbackTimer_->stop();
    if (tifRenderWorker_) tifRenderWorker_->requestCancel(requestId);

    TifRenderRequest request;
    request.requestId = requestId;
    request.globalIndex = globalIndex;
    request.recordId = entry.recordId;
    request.path = entry.tifPath;
    request.pageCount = entry.pageCount;
    request.width = entry.tifWidth;
    request.height = entry.tifHeight;
    request.mode = mode;
    request.a = a;
    request.b = b;
    request.c = c;
    request.d = d;
    request.e = e;
    request.f = f;

    if (tifRenderHintLbl_) tifRenderHintLbl_->setText(QString::fromUtf8("正在渲染 TIF..."));
    setStatus(QString::fromUtf8("正在渲染 TIF..."), false);
    tifRenderTimeoutTimer_->start(qMax(2000, intervalSpin_->value() * 2));
    QMetaObject::invokeMethod(tifRenderWorker_, "render", Qt::QueuedConnection,
                              Q_ARG(TifRenderRequest, request));
    updateUiEnabled();
}

void RecordPlaybackPanel::onTifRenderReady(quint64 requestId, quint64 globalIndex, QImage image, QString info, ChannelImageStats stats)
{
    if (requestId != activeTifRenderRequestId_ || globalIndex != activeTifRenderFrame_) return;
    tifRenderBusy_ = false;
    tifRenderTimeoutTimer_->stop();
    finishDisplayedFrame(globalIndex, image, info, stats);
    if (tifRenderHintLbl_) tifRenderHintLbl_->setText(QString::fromUtf8("TIF 渲染完成"));
    if (tifRenderResumeWhenReady_ && playbackRequested_ && playbackTimer_) {
        playbackTimer_->start(intervalSpin_->value());
    }
    tifRenderResumeWhenReady_ = false;
    updateUiEnabled();
}

void RecordPlaybackPanel::onTifRenderFailed(quint64 requestId, quint64 globalIndex, QString error)
{
    if (requestId != activeTifRenderRequestId_) return;
    tifRenderBusy_ = false;
    tifRenderTimeoutTimer_->stop();
    const FrameRef ref = frameRefs_.isEmpty() ? FrameRef() : frameRefs_[static_cast<int>(qMin<quint64>(globalIndex, frameRefs_.size() - 1))];
    const PlaybackEntry entry = (ref.entryIndex >= 0 && ref.entryIndex < playbackEntries_.size()) ? playbackEntries_[ref.entryIndex] : PlaybackEntry();
    handleFrameError(globalIndex, ref, entry, error);
}

void RecordPlaybackPanel::onTifRenderCanceled(quint64 requestId)
{
    if (requestId != activeTifRenderRequestId_) return;
    tifRenderBusy_ = false;
    tifRenderTimeoutTimer_->stop();
    tifRenderResumeWhenReady_ = false;
    if (tifRenderHintLbl_) tifRenderHintLbl_->setText(QString::fromUtf8("TIF 渲染已取消"));
    updateUiEnabled();
}

void RecordPlaybackPanel::onTifRenderProgress(quint64 requestId, QString text)
{
    if (requestId != activeTifRenderRequestId_) return;
    if (tifRenderHintLbl_) tifRenderHintLbl_->setText(text);
}

void RecordPlaybackPanel::onTifRenderTimeout()
{
    if (!tifRenderBusy_) return;
    const quint64 timedOutFrame = activeTifRenderFrame_;
    cancelTifRender();
    if (badFramePolicyCombo_->currentData().toInt() == static_cast<int>(BadFramePolicy::Skip)) {
        ++skippedBadFrames_;
        updateSkippedBadFramesStatus();
        if (timedOutFrame + 1 < static_cast<quint64>(frameRefs_.size())) {
            seekToFrame(timedOutFrame + 1, SeekTrigger::Playback, true);
        }
    } else {
        setStatus(QString::fromUtf8("TIF 渲染超时，已暂停"), true);
    }
}

void RecordPlaybackPanel::cancelTifRender()
{
    if (!tifRenderBusy_) return;
    const quint64 cancelMarker = ++tifRenderRequestId_;
    if (tifRenderWorker_) tifRenderWorker_->requestCancel(cancelMarker);
    tifRenderTimeoutTimer_->stop();
    tifRenderResumeWhenReady_ = false;
    if (tifRenderHintLbl_) tifRenderHintLbl_->setText(QString::fromUtf8("正在取消 TIF 渲染..."));
    updateUiEnabled();
}

bool RecordPlaybackPanel::cancelTifRenderAndWait(int timeoutMs, QString* err)
{
    if (!tifRenderBusy_) return true;
    cancelTifRender();
    QElapsedTimer timer;
    timer.start();
    while (tifRenderBusy_ && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (tifRenderBusy_) {
        if (err) *err = QString::fromUtf8("渲染任务未结束，稍后重试");
        return false;
    }
    return true;
}

void RecordPlaybackPanel::handleFrameError(quint64 globalIndex, const FrameRef& ref, const PlaybackEntry& entry, const QString& err)
{
    const QString detail = QString::fromUtf8("%1 record_id=%2 global_frame=%3 entry_frame=%4 path=%5")
        .arg(err)
        .arg(entry.recordId)
        .arg(globalIndex + 1)
        .arg(ref.frameIndex + 1)
        .arg(entry.type == "tif" ? entry.tifPath : entry.rawPath);
    if (badFramePolicyCombo_ && badFramePolicyCombo_->currentData().toInt() == static_cast<int>(BadFramePolicy::Skip)) {
        ++skippedBadFrames_;
        updateSkippedBadFramesStatus();
        if (globalIndex + 1 < static_cast<quint64>(frameRefs_.size())) {
            seekToFrame(globalIndex + 1, SeekTrigger::Playback, playbackRequested_ || (playbackTimer_ && playbackTimer_->isActive()));
            return;
        }
    }
    pausePlayback();
    setStatus(detail, true);
}

void RecordPlaybackPanel::resetSkippedBadFrames()
{
    skippedBadFrames_ = 0;
    updateSkippedBadFramesStatus();
}

void RecordPlaybackPanel::updateSkippedBadFramesStatus()
{
    if (skippedBadFrames_ > 0) {
        setStatus(QString::fromUtf8("已跳过 %1 帧").arg(skippedBadFrames_), false);
    }
}

void RecordPlaybackPanel::clearSkippedBadFrames()
{
    resetSkippedBadFrames();
    setStatus(QString::fromUtf8("已清除坏帧跳过计数"), false);
}

void RecordPlaybackPanel::syncBandSelector(BandSelector* selector, int pageCount)
{
    if (!selector || !selector->slider || !selector->spin) return;
    const int maxValue = qMax(1, pageCount);
    const int value = qBound(1, selector->spin->value(), maxValue);
    {
        QSignalBlocker b1(selector->slider);
        QSignalBlocker b2(selector->spin);
        selector->slider->setRange(1, maxValue);
        selector->spin->setRange(1, maxValue);
        selector->slider->setValue(value);
        selector->spin->setValue(value);
    }
    selector->maxPageCount = pageCount;
}

void RecordPlaybackPanel::updateTifBandRanges(const PlaybackEntry& entry)
{
    const int pageCount = qMax(1, entry.pageCount);
    QVector<BandSelector> selectors = {
        {singleBandSlider_, singleBandSpin_, 0},
        {rangeBeginSlider_, rangeBeginSpin_, 0},
        {rangeEndSlider_, rangeEndSpin_, 0},
        {rBandSlider_, rBandSpin_, 0},
        {gBandSlider_, gBandSpin_, 0},
        {bBandSlider_, bBandSpin_, 0},
        {rBeginSlider_, rBeginSpin_, 0},
        {rEndSlider_, rEndSpin_, 0},
        {gBeginSlider_, gBeginSpin_, 0},
        {gEndSlider_, gEndSpin_, 0},
        {bBeginSlider_, bBeginSpin_, 0},
        {bEndSlider_, bEndSpin_, 0},
    };
    if (pendingTifBandSettingsValid_ && pendingTifBandSettings_.size() == selectors.size()) {
        for (int i = 0; i < selectors.size(); ++i) {
            BandSelector& selector = selectors[i];
            if (!selector.slider || !selector.spin) continue;
            const int value = qBound(1, pendingTifBandSettings_[i], pageCount);
            QSignalBlocker b1(selector.slider);
            QSignalBlocker b2(selector.spin);
            selector.slider->setRange(1, pageCount);
            selector.spin->setRange(1, pageCount);
            selector.slider->setValue(value);
            selector.spin->setValue(value);
            selector.maxPageCount = pageCount;
        }
        pendingTifBandSettingsValid_ = false;
        pendingTifBandSettings_.clear();
    } else {
        for (BandSelector& selector : selectors) syncBandSelector(&selector, pageCount);
    }
    if (tifRenderHintLbl_) {
        tifRenderHintLbl_->setText(QString::fromUtf8("TIF pages=%1 size=%2x%3")
            .arg(entry.pageCount).arg(entry.tifWidth).arg(entry.tifHeight));
    }
}

bool RecordPlaybackPanel::renderRawFrame(const PlaybackEntry& entry, quint64 frameIndex,
                                         QImage* image, QString* info, ChannelImageStats* stats, QString* err) const
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
    if (stats) *stats = makeChannelImageStats(entry.width, entry.height, cli::proto::Mono16, data);
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

bool RecordPlaybackPanel::renderTifFrame(const PlaybackEntry& entry, QImage* image, QString* info, ChannelImageStats* stats, QString* err) const
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
    if (stats && image) *stats = imageStatsFromDisplayImage(*image);
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

void RecordPlaybackPanel::keyPressEvent(QKeyEvent* event)
{
    QWidget* fw = focusWidget();
    if (qobject_cast<QAbstractSpinBox*>(fw) || qobject_cast<QLineEdit*>(fw)) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Space:
        if (playbackRequested_ || (playbackTimer_ && playbackTimer_->isActive())) pausePlayback();
        else startPlayback();
        event->accept();
        return;
    case Qt::Key_Left:
        previousFrame();
        event->accept();
        return;
    case Qt::Key_Right:
        nextFrame();
        event->accept();
        return;
    case Qt::Key_Home:
        if (!frameRefs_.isEmpty()) seekToFrame(0, SeekTrigger::Manual, false);
        event->accept();
        return;
    case Qt::Key_End:
        if (!frameRefs_.isEmpty()) seekToFrame(static_cast<quint64>(frameRefs_.size() - 1), SeekTrigger::Manual, false);
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
}

void RecordPlaybackPanel::onFrameSpinChanged(int value)
{
    if (frameRefs_.isEmpty() || value <= 0) return;
    seekToFrame(static_cast<quint64>(value - 1), SeekTrigger::Manual, false);
}

void RecordPlaybackPanel::exportCurrentFrame()
{
    if (currentPlaybackImage_.isNull()) return;
    QDir().mkpath(QDir("recordings").filePath("exports"));
    const QString initialDir = lastExportDir_.isEmpty()
        ? QDir("recordings").filePath("exports")
        : lastExportDir_;
    const QString initialPath = QDir(initialDir).filePath(currentExportFileName());
    const QString path = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("导出当前帧"),
        initialPath,
        QString::fromUtf8("PNG 图片 (*.png)"));
    if (path.isEmpty()) return;
    lastExportDir_ = QFileInfo(path).absolutePath();
    if (!currentPlaybackImage_.save(path, "PNG")) {
        setStatus(QString::fromUtf8("导出当前帧失败: %1").arg(path), true);
        return;
    }
    setStatus(QString::fromUtf8("当前帧已导出: %1").arg(path), false);
}

void RecordPlaybackPanel::refreshCacheInfo()
{
    const CacheSummary summary = summarizeCache();
    if (cacheInfoLbl_) {
        cacheInfoLbl_->setText(QString::fromUtf8("目录: %1\n记录: %2  文件: %3  大小: %4")
            .arg(QDir(cacheRoot()).absolutePath())
            .arg(summary.recordCount)
            .arg(summary.fileCount)
            .arg(formatBytes(summary.totalBytes)));
    }
}

void RecordPlaybackPanel::clearNonPlaybackCache()
{
    const QSet<QString> keep = playbackCacheKeys();
    const CacheSummary summary = summarizeCache(keep, true);
    if (summary.recordCount == 0) {
        setStatus(QString::fromUtf8("没有可清理的非当前播放缓存"), false);
        return;
    }
    const QString msg = QString::fromUtf8("将删除 %1 条记录、%2 个文件（共 %3）。是否继续？")
        .arg(summary.recordCount)
        .arg(summary.fileCount)
        .arg(formatBytes(summary.totalBytes));
    if (QMessageBox::question(this, QString::fromUtf8("清理非当前播放缓存"), msg) != QMessageBox::Yes) {
        return;
    }
    CacheSummary removed;
    QString err;
    if (!prepareForCacheMutation(&err)) {
        setStatus(err, true);
        return;
    }
    if (!removeCacheRecords(keep, false, &removed, &err)) {
        setStatus(err, true);
        return;
    }
    updateRecordCacheStatuses();
    refreshCacheInfo();
    setStatus(QString::fromUtf8("已清理 %1 条缓存记录").arg(removed.recordCount), false);
}

void RecordPlaybackPanel::clearAllCache()
{
    const CacheSummary summary = summarizeCache();
    if (summary.recordCount == 0) {
        setStatus(QString::fromUtf8("缓存为空"), false);
        return;
    }
    const QString msg = QString::fromUtf8("将删除 %1 条记录、%2 个文件（共 %3），当前播放序列将一并清空。是否继续？")
        .arg(summary.recordCount)
        .arg(summary.fileCount)
        .arg(formatBytes(summary.totalBytes));
    if (QMessageBox::question(this, QString::fromUtf8("清理全部缓存"), msg) != QMessageBox::Yes) {
        return;
    }
    CacheSummary removed;
    QString err;
    if (!prepareForCacheMutation(&err)) {
        setStatus(err, true);
        return;
    }
    pausePlayback();
    if (!removeCacheRecords(QSet<QString>(), true, &removed, &err)) {
        setStatus(err, true);
        return;
    }
    setPlaybackSequenceCleared();
    updateRecordCacheStatuses();
    refreshCacheInfo();
    setStatus(QString::fromUtf8("已清理全部缓存"), false);
    updateUiEnabled();
}

void RecordPlaybackPanel::selectAllRecords()
{
    if (recordSelectionLocked_) return;
    recordsTable_->selectAll();
}

void RecordPlaybackPanel::invertRecordSelection()
{
    if (recordSelectionLocked_) return;
    QSignalBlocker blocker(recordsTable_);
    for (int row = 0; row < recordsTable_->rowCount(); ++row) {
        const bool selected = recordsTable_->item(row, 0) && recordsTable_->item(row, 0)->isSelected();
        for (int col = 0; col < recordsTable_->columnCount(); ++col) {
            if (QTableWidgetItem* item = recordsTable_->item(row, col)) item->setSelected(!selected);
        }
    }
    updateUiEnabled();
}

void RecordPlaybackPanel::clearRecordSelection()
{
    if (recordSelectionLocked_) return;
    recordsTable_->clearSelection();
}

void RecordPlaybackPanel::appendSelectedToPlaybackSequence()
{
    appendAfterDownload_ = true;
    downloadSelected();
}

void RecordPlaybackPanel::togglePlaybackSequenceVisible(bool checked)
{
    if (playbackSequenceTable_) playbackSequenceTable_->setVisible(checked);
    if (playbackSeqRemoveBtn_) playbackSeqRemoveBtn_->setVisible(checked);
    if (playbackSeqClearBtn_) playbackSeqClearBtn_->setVisible(checked);
    saveSettings();
    updateUiEnabled();
}

void RecordPlaybackPanel::removeSelectedPlaybackRecords()
{
    const QVector<RecordItem> selected = selectedPlaybackRecords();
    if (selected.isEmpty()) return;
    QSet<QString> removeKeys;
    for (const RecordItem& item : selected) removeKeys.insert(cacheKey(item.type, item.recordId));
    if (tifRenderBusy_ && activeTifRenderFrame_ < static_cast<quint64>(frameRefs_.size())) {
        const FrameRef ref = frameRefs_[static_cast<int>(activeTifRenderFrame_)];
        const PlaybackEntry& entry = playbackEntries_[ref.entryIndex];
        if (removeKeys.contains(cacheKey(entry.type, entry.recordId))) {
            QString err;
            if (!cancelTifRenderAndWait(2000, &err)) {
                setStatus(err, true);
                return;
            }
        }
    }
    QVector<RecordItem> next;
    for (const RecordItem& item : playbackRecordItems_) {
        if (!removeKeys.contains(cacheKey(item.type, item.recordId))) next.append(item);
    }
    setPlaybackSequence(next, false);
}

void RecordPlaybackPanel::clearPlaybackSequence()
{
    QString err;
    if (!cancelTifRenderAndWait(2000, &err)) {
        setStatus(err, true);
        return;
    }
    pausePlayback();
    setPlaybackSequenceCleared();
    resetSkippedBadFrames();
    updateUiEnabled();
}

void RecordPlaybackPanel::showRecordDetails(QTableWidgetItem* item)
{
    if (!item) return;
    const int row = item->row();
    if (row < 0 || row >= records_.size()) return;
    const RecordItem& record = records_[row];
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("记录详情"));
    auto* form = new QFormLayout(&dlg);
    auto addText = [form, &dlg](const QString& label, const QString& value) {
        auto* lbl = new QLabel(value, &dlg);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setWordWrap(true);
        form->addRow(label, lbl);
    };
    addText("record_id", record.recordId);
    addText("type", record.type);
    addText("timestamp", formatRecordTimestamp(record.timestampNs));
    addText("timestamp_ns", QString::number(record.timestampNs));
    addText(QString::fromUtf8("文件"), filesText(record));
    quint64 size = 0;
    for (const RecordFile& f : record.files) size += f.sizeBytes;
    addText(QString::fromUtf8("总大小"), formatBytes(size));
    addText(QString::fromUtf8("缓存状态"), cacheStateText(cacheState(record)));
    addText(QString::fromUtf8("缓存目录"), QDir(recordCacheDir(record.type, record.recordId)).absolutePath());
    PlaybackEntry entry;
    QString err;
    if (isRecordCached(record, nullptr)) {
        if (record.type == "raw" && loadRawEntry(record, &entry, &err)) {
            addText(QString::fromUtf8("raw 尺寸/帧数"), QString("%1x%2 / %3").arg(entry.width).arg(entry.height).arg(entry.frameCount));
        } else if (record.type == "tif" && loadTifEntry(record, &entry, &err)) {
            addText(QString::fromUtf8("tif 尺寸/page"), QString("%1x%2 / %3").arg(entry.tifWidth).arg(entry.tifHeight).arg(entry.pageCount));
        }
    }
    auto* buttons = new QHBoxLayout();
    auto* openBtn = new QPushButton(QString::fromUtf8("打开缓存目录"), &dlg);
    auto* copyBtn = new QPushButton(QString::fromUtf8("复制缓存路径"), &dlg);
    buttons->addWidget(openBtn);
    buttons->addWidget(copyBtn);
    buttons->addStretch(1);
    form->addRow(buttons);
    const QString cachePath = QDir(recordCacheDir(record.type, record.recordId)).absolutePath();
    connect(openBtn, &QPushButton::clicked, &dlg, [cachePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(cachePath));
    });
    connect(copyBtn, &QPushButton::clicked, &dlg, [cachePath]() {
        QGuiApplication::clipboard()->setText(cachePath);
    });
    dlg.exec();
}

void RecordPlaybackPanel::previousFrame()
{
    if (currentFrame_ == 0) return;
    const bool wasPlaying = playbackTimer_ && playbackTimer_->isActive();
    if (wasPlaying) playbackTimer_->stop();
    seekToFrame(currentFrame_ - 1, SeekTrigger::Manual, wasPlaying);
}

void RecordPlaybackPanel::nextFrame()
{
    if (frameRefs_.isEmpty()) return;
    if (currentFrame_ + 1 >= static_cast<quint64>(frameRefs_.size())) {
        if (loopChk_->isChecked()) seekToFrame(0, SeekTrigger::Playback, playbackTimer_ && playbackTimer_->isActive());
        else pausePlayback();
        return;
    }
    const bool wasPlaying = playbackTimer_ && playbackTimer_->isActive();
    if (wasPlaying) playbackTimer_->stop();
    seekToFrame(currentFrame_ + 1, wasPlaying ? SeekTrigger::Playback : SeekTrigger::Manual, wasPlaying);
}

void RecordPlaybackPanel::startPlayback()
{
    if (frameRefs_.isEmpty()) return;
    if (!loopChk_->isChecked() && currentFrame_ + 1 >= static_cast<quint64>(frameRefs_.size())) {
        seekToFrame(0, SeekTrigger::Manual, false);
    }
    playbackRequested_ = true;
    playbackTimer_->start(intervalSpin_->value());
    emit requestSwitchToPlaybackView();
    updateUiEnabled();
}

void RecordPlaybackPanel::pausePlayback()
{
    playbackRequested_ = false;
    tifRenderResumeWhenReady_ = false;
    if (playbackTimer_) playbackTimer_->stop();
    updateUiEnabled();
}

void RecordPlaybackPanel::stopPlayback()
{
    pausePlayback();
    if (!frameRefs_.isEmpty()) {
        resetSkippedBadFrames();
        seekToFrame(0, SeekTrigger::Manual, false);
    }
}

void RecordPlaybackPanel::onPlaybackTick()
{
    nextFrame();
}

void RecordPlaybackPanel::onSliderReleased()
{
    const bool resume = progressWasPlayingBeforeDrag_;
    sliderDragging_ = false;
    seekToFrame(static_cast<quint64>(progressSlider_->value()), SeekTrigger::Manual, resume);
    progressWasPlayingBeforeDrag_ = false;
}

void RecordPlaybackPanel::onRenderSettingsChanged()
{
    if (loadingSettings_) return;
    scheduleTifRerender();
}

void RecordPlaybackPanel::scheduleTifRerender()
{
    if (loadingSettings_) return;
    if (frameRefs_.isEmpty()) return;
    const FrameRef ref = frameRefs_[static_cast<int>(currentFrame_)];
    if (ref.entryIndex >= 0 && playbackEntries_[ref.entryIndex].type == "tif") {
        if (tifRenderDebounceTimer_) tifRenderDebounceTimer_->start(200);
    }
}

void RecordPlaybackPanel::updateRenderVisibility()
{
    bool currentIsTif = false;
    if (!frameRefs_.isEmpty() && currentFrame_ < static_cast<quint64>(frameRefs_.size())) {
        const FrameRef ref = frameRefs_[static_cast<int>(currentFrame_)];
        currentIsTif = ref.entryIndex >= 0
            && ref.entryIndex < playbackEntries_.size()
            && playbackEntries_[ref.entryIndex].type == "tif";
    }
    if (renderGroup_) {
        renderGroup_->setVisible(currentIsTif);
    }
    if (!currentIsTif) return;

    const auto mode = static_cast<TifRenderMode>(renderModeCombo_->currentData().toInt());
    const bool single = mode == TifRenderMode::SingleBand;
    const bool range = mode == TifRenderMode::RangeAverage;
    const bool rgbSingle = mode == TifRenderMode::RgbSingleBand;
    const bool rgbRange = mode == TifRenderMode::RgbRangeAverage;

    auto* renderForm = renderGroup_ ? qobject_cast<QFormLayout*>(renderGroup_->layout()) : nullptr;
    if (!renderForm) return;

    auto setFieldVisible = [renderForm](QWidget* field, bool visible) {
        if (!field) return;
        field->setVisible(visible);
        if (QLabel* label = qobject_cast<QLabel*>(renderForm->labelForField(field))) {
            label->setVisible(visible);
        }
    };
    auto setBandRowVisible = [this, setFieldVisible](const char* objectName, bool visible) {
        QWidget* row = renderGroup_ ? renderGroup_->findChild<QWidget*>(QString::fromLatin1(objectName)) : nullptr;
        setFieldVisible(row, visible);
    };

    setBandRowVisible("singleBandRow", single);
    setBandRowVisible("rangeBeginRow", range);
    setBandRowVisible("rangeEndRow", range && !(rangeToLastChk_ && rangeToLastChk_->isChecked()));
    if (rangeToLastChk_) rangeToLastChk_->setVisible(range);
    setBandRowVisible("rBandRow", rgbSingle);
    setBandRowVisible("gBandRow", rgbSingle);
    setBandRowVisible("bBandRow", rgbSingle);
    setBandRowVisible("rBeginRow", rgbRange);
    setBandRowVisible("rEndRow", rgbRange && !(rToLastChk_ && rToLastChk_->isChecked()));
    if (rToLastChk_) rToLastChk_->setVisible(rgbRange);
    setBandRowVisible("gBeginRow", rgbRange);
    setBandRowVisible("gEndRow", rgbRange && !(gToLastChk_ && gToLastChk_->isChecked()));
    if (gToLastChk_) gToLastChk_->setVisible(rgbRange);
    setBandRowVisible("bBeginRow", rgbRange);
    setBandRowVisible("bEndRow", rgbRange && !(bToLastChk_ && bToLastChk_->isChecked()));
    if (bToLastChk_) bToLastChk_->setVisible(rgbRange);
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
