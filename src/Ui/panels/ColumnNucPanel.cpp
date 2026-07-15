#include "ColumnNucPanel.h"

#include "DeviceClient.h"
#include "RpcTypes.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString jsonString(const nlohmann::json& value, const char* key)
{
    return value.contains(key) && value[key].is_string()
        ? QString::fromStdString(value[key].get<std::string>())
        : QString();
}

int jsonInt(const nlohmann::json& value, const char* key, int fallback = 0)
{
    if (!value.contains(key)) return fallback;
    if (value[key].is_number_integer()) return value[key].get<int>();
    if (value[key].is_number_unsigned()) return static_cast<int>(value[key].get<unsigned int>());
    return fallback;
}

quint64 jsonU64(const nlohmann::json& value, const char* key, quint64 fallback = 0)
{
    if (!value.contains(key)) return fallback;
    if (value[key].is_number_unsigned()) return value[key].get<quint64>();
    if (value[key].is_number_integer()) {
        const qint64 number = value[key].get<qint64>();
        return number >= 0 ? static_cast<quint64>(number) : fallback;
    }
    if (value[key].is_string()) {
        bool ok = false;
        const quint64 number = QString::fromStdString(value[key].get<std::string>()).toULongLong(&ok);
        return ok ? number : fallback;
    }
    return fallback;
}

double jsonDouble(const nlohmann::json& value, const char* key, double fallback = 0.0)
{
    return value.contains(key) && value[key].is_number() ? value[key].get<double>() : fallback;
}

bool jsonBool(const nlohmann::json& value, const char* key, bool fallback = false)
{
    return value.contains(key) && value[key].is_boolean() ? value[key].get<bool>() : fallback;
}

QString formatBytes(quint64 bytes)
{
    const double kib = 1024.0;
    const double mib = kib * 1024.0;
    const double gib = mib * 1024.0;
    if (bytes >= static_cast<quint64>(gib)) return QString::number(bytes / gib, 'f', 2) + QStringLiteral(" GiB");
    if (bytes >= static_cast<quint64>(mib)) return QString::number(bytes / mib, 'f', 1) + QStringLiteral(" MiB");
    if (bytes >= static_cast<quint64>(kib)) return QString::number(bytes / kib, 'f', 1) + QStringLiteral(" KiB");
    return QString::number(bytes) + QStringLiteral(" B");
}

QString formatTimestamp(quint64 timestampNs)
{
    if (timestampNs == 0) return QStringLiteral("-");
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestampNs / 1000000ULL))
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString formatTemperature(double temperature)
{
    return QString::number(temperature, 'f', 2) + QString::fromUtf8(" ℃");
}

QString phaseText(const QString& phase)
{
    if (phase == QStringLiteral("moving_mirror")) return QString::fromUtf8("转镜前往预设 1");
    if (phase == QStringLiteral("setting_temperature")) return QString::fromUtf8("设置黑体温度");
    if (phase == QStringLiteral("waiting_temperature")) return QString::fromUtf8("等待黑体温度稳定");
    if (phase == QStringLiteral("capturing")) return QString::fromUtf8("采集未校正 Raw");
    if (phase == QStringLiteral("completed")) return QString::fromUtf8("已完成");
    if (phase == QStringLiteral("failed")) return QString::fromUtf8("失败");
    if (phase == QStringLiteral("cancelled")) return QString::fromUtf8("已取消");
    return phase;
}

QTableWidgetItem* makeItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}
} // namespace

ColumnNucPanel::ColumnNucPanel(DeviceClient* device, QWidget* parent)
    : QWidget(parent)
    , device_(device)
{
    setObjectName(QStringLiteral("columnNucPanel"));
    setupUi();

    captureTimer_ = new QTimer(this);
    captureTimer_->setSingleShot(true);
    connect(captureTimer_, &QTimer::timeout, this, &ColumnNucPanel::pollCaptureStatus);

    if (device_) {
        connected_ = device_->isConnected();
        connect(device_, &DeviceClient::connectionChanged, this,
                [this](bool connected, const QString&) {
                    connected_ = connected;
                    if (connected_) {
                        refreshRemote();
                        if (!activeTaskId_.isEmpty()) pollCaptureStatus();
                    } else {
                        captureTimer_->stop();
                        captureStatusPending_ = false;
                        if (!activeTaskId_.isEmpty()) {
                            setStatus(QString::fromUtf8("连接已断开；服务端任务可能仍在运行，重连后继续查询"), true);
                        } else {
                            setStatus(QString::fromUtf8("连接已断开"));
                        }
                    }
                    updateUiEnabled();
                });
    }

    updateSelectionState();
    updateUiEnabled();
    if (connected_) QTimer::singleShot(0, this, &ColumnNucPanel::refreshRemote);
}

void ColumnNucPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* configGroup = new QGroupBox(QString::fromUtf8("当前配置"), this);
    configGroup->setObjectName(QStringLiteral("columnNucConfigGroup"));
    auto* configLayout = new QVBoxLayout(configGroup);
    auto* configForm = new QFormLayout();
    enabledCheck_ = new QCheckBox(QString::fromUtf8("启用列向 NUC"), this);
    enabledCheck_->setObjectName(QStringLiteral("columnNucEnabledCheck"));
    matricesLabel_ = new QLabel(QStringLiteral("-"), this);
    sizeLabel_ = new QLabel(QStringLiteral("-"), this);
    epsLabel_ = new QLabel(QStringLiteral("-"), this);
    gainLabel_ = new QLabel(QStringLiteral("-"), this);
    offsetLabel_ = new QLabel(QStringLiteral("-"), this);
    gainLabel_->setWordWrap(true);
    offsetLabel_->setWordWrap(true);
    configForm->addRow(QString::fromUtf8("运行状态"), enabledCheck_);
    configForm->addRow(QString::fromUtf8("矩阵加载"), matricesLabel_);
    configForm->addRow(QString::fromUtf8("矩阵尺寸"), sizeLabel_);
    configForm->addRow(QStringLiteral("eps"), epsLabel_);
    configForm->addRow(QStringLiteral("gain"), gainLabel_);
    configForm->addRow(QStringLiteral("offset"), offsetLabel_);
    configLayout->addLayout(configForm);
    auto* configButtons = new QHBoxLayout();
    applyEnabledBtn_ = new QPushButton(QString::fromUtf8("应用启用状态"), this);
    applyEnabledBtn_->setObjectName(QStringLiteral("columnNucApplyEnabledButton"));
    reloadBtn_ = new QPushButton(QString::fromUtf8("重新加载矩阵"), this);
    reloadBtn_->setObjectName(QStringLiteral("columnNucReloadButton"));
    refreshBtn_ = new QPushButton(QString::fromUtf8("刷新配置与列表"), this);
    refreshBtn_->setObjectName(QStringLiteral("columnNucRefreshButton"));
    configButtons->addWidget(applyEnabledBtn_);
    configButtons->addWidget(reloadBtn_);
    configButtons->addStretch();
    configButtons->addWidget(refreshBtn_);
    configLayout->addLayout(configButtons);
    root->addWidget(configGroup);

    auto* captureGroup = new QGroupBox(QString::fromUtf8("黑体 Raw 采集"), this);
    captureGroup->setObjectName(QStringLiteral("columnNucCaptureGroup"));
    auto* captureLayout = new QVBoxLayout(captureGroup);
    auto* captureForm = new QFormLayout();
    frameCountSpin_ = new QSpinBox(this);
    frameCountSpin_->setObjectName(QStringLiteral("columnNucFrameCountSpin"));
    frameCountSpin_->setRange(1, 10000);
    frameCountSpin_->setValue(64);
    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setObjectName(QStringLiteral("columnNucTimeoutSpin"));
    timeoutSpin_->setRange(1, 600000);
    timeoutSpin_->setSuffix(QStringLiteral(" ms"));
    timeoutSpin_->setValue(10000);
    lowTemperatureSpin_ = new QDoubleSpinBox(this);
    lowTemperatureSpin_->setObjectName(QStringLiteral("columnNucLowTemperatureSpin"));
    lowTemperatureSpin_->setRange(-100.0, 300.0);
    lowTemperatureSpin_->setDecimals(2);
    lowTemperatureSpin_->setSuffix(QString::fromUtf8(" ℃"));
    lowTemperatureSpin_->setValue(30.0);
    highTemperatureSpin_ = new QDoubleSpinBox(this);
    highTemperatureSpin_->setObjectName(QStringLiteral("columnNucHighTemperatureSpin"));
    highTemperatureSpin_->setRange(-100.0, 300.0);
    highTemperatureSpin_->setDecimals(2);
    highTemperatureSpin_->setSuffix(QString::fromUtf8(" ℃"));
    highTemperatureSpin_->setValue(45.0);
    captureForm->addRow(QString::fromUtf8("采集帧数"), frameCountSpin_);
    captureForm->addRow(QString::fromUtf8("Raw 阶段超时"), timeoutSpin_);
    captureForm->addRow(QString::fromUtf8("Low 目标温度"), lowTemperatureSpin_);
    captureForm->addRow(QString::fromUtf8("High 目标温度"), highTemperatureSpin_);
    captureLayout->addLayout(captureForm);

    auto* captureButtons = new QHBoxLayout();
    captureLowBtn_ = new QPushButton(QString::fromUtf8("采集 Low"), this);
    captureLowBtn_->setObjectName(QStringLiteral("columnNucCaptureLowButton"));
    captureHighBtn_ = new QPushButton(QString::fromUtf8("采集 High"), this);
    captureHighBtn_->setObjectName(QStringLiteral("columnNucCaptureHighButton"));
    cancelBtn_ = new QPushButton(QString::fromUtf8("取消采集"), this);
    cancelBtn_->setObjectName(QStringLiteral("columnNucCancelCaptureButton"));
    captureLowBtn_->setProperty("primary", true);
    captureHighBtn_->setProperty("primary", true);
    cancelBtn_->setProperty("danger", true);
    captureButtons->addWidget(captureLowBtn_);
    captureButtons->addWidget(captureHighBtn_);
    captureButtons->addWidget(cancelBtn_);
    captureLayout->addLayout(captureButtons);
    captureStatusLabel_ = new QLabel(QString::fromUtf8("未启动"), this);
    captureStatusLabel_->setObjectName(QStringLiteral("columnNucCaptureStatusLabel"));
    captureStatusLabel_->setProperty("readout", true);
    captureStatusLabel_->setWordWrap(true);
    captureLayout->addWidget(captureStatusLabel_);
    root->addWidget(captureGroup);

    auto* listGroup = new QGroupBox(QString::fromUtf8("已完成黑体采集"), this);
    listGroup->setObjectName(QStringLiteral("columnNucCaptureListGroup"));
    auto* listLayout = new QVBoxLayout(listGroup);
    lowTable_ = new QTableWidget(this);
    lowTable_->setObjectName(QStringLiteral("columnNucLowCaptureTable"));
    highTable_ = new QTableWidget(this);
    highTable_->setObjectName(QStringLiteral("columnNucHighCaptureTable"));
    const QList<QTableWidget*> tables = {lowTable_, highTable_};
    for (QTableWidget* table : tables) {
        table->setColumnCount(7);
        table->setHorizontalHeaderLabels(QStringList()
            << QStringLiteral("ID") << QString::fromUtf8("时间") << QString::fromUtf8("目标温度")
            << QString::fromUtf8("实际温度") << QString::fromUtf8("帧数")
            << QString::fromUtf8("尺寸") << QString::fromUtf8("Raw 大小"));
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setMinimumHeight(120);
    }
    listLayout->addWidget(new QLabel(QStringLiteral("Low"), this));
    listLayout->addWidget(lowTable_);
    listLayout->addWidget(new QLabel(QStringLiteral("High"), this));
    listLayout->addWidget(highTable_);
    root->addWidget(listGroup);

    auto* calibrateGroup = new QGroupBox(QString::fromUtf8("生成并应用校正矩阵"), this);
    calibrateGroup->setObjectName(QStringLiteral("columnNucCalibrateGroup"));
    auto* calibrateLayout = new QVBoxLayout(calibrateGroup);
    auto* selectionForm = new QFormLayout();
    lowSelectionLabel_ = new QLabel(QStringLiteral("-"), this);
    highSelectionLabel_ = new QLabel(QStringLiteral("-"), this);
    lowSelectionLabel_->setWordWrap(true);
    highSelectionLabel_->setWordWrap(true);
    selectionForm->addRow(QStringLiteral("Low"), lowSelectionLabel_);
    selectionForm->addRow(QStringLiteral("High"), highSelectionLabel_);
    calibrateLayout->addLayout(selectionForm);
    calibrateBtn_ = new QPushButton(QString::fromUtf8("生成并应用矩阵"), this);
    calibrateBtn_->setObjectName(QStringLiteral("columnNucCalibrateButton"));
    calibrateBtn_->setProperty("primary", true);
    calibrateLayout->addWidget(calibrateBtn_);
    resultLabel_ = new QLabel(QStringLiteral("-"), this);
    resultLabel_->setObjectName(QStringLiteral("columnNucResultLabel"));
    resultLabel_->setProperty("readout", true);
    resultLabel_->setWordWrap(true);
    calibrateLayout->addWidget(resultLabel_);
    root->addWidget(calibrateGroup);

    connect(applyEnabledBtn_, &QPushButton::clicked, this, &ColumnNucPanel::applyEnabledState);
    connect(reloadBtn_, &QPushButton::clicked, this, &ColumnNucPanel::reloadMatrices);
    connect(refreshBtn_, &QPushButton::clicked, this, &ColumnNucPanel::refreshRemote);
    connect(captureLowBtn_, &QPushButton::clicked, this, &ColumnNucPanel::startLowCapture);
    connect(captureHighBtn_, &QPushButton::clicked, this, &ColumnNucPanel::startHighCapture);
    connect(cancelBtn_, &QPushButton::clicked, this, &ColumnNucPanel::cancelCapture);
    connect(calibrateBtn_, &QPushButton::clicked, this, &ColumnNucPanel::calibrateSelection);
    connect(lowTable_, &QTableWidget::itemSelectionChanged, this, &ColumnNucPanel::updateSelectionState);
    connect(highTable_, &QTableWidget::itemSelectionChanged, this, &ColumnNucPanel::updateSelectionState);
}

void ColumnNucPanel::refreshRemote()
{
    if (!device_ || !connected_) return;
    refreshConfig();
    refreshCaptures();
}

void ColumnNucPanel::refreshConfig()
{
    if (!device_ || !connected_) return;
    device_->systemApi()->columnNucGetConfig(this, [this](const RpcResult& result) {
        if (!result.ok) {
            setResult(rpcErrorText(result.code, result.msg), true);
            return;
        }
        applyConfig(result.data);
    });
}

void ColumnNucPanel::applyConfig(const nlohmann::json& data)
{
    currentEnabled_ = jsonBool(data, "enabled", false);
    currentMatricesLoaded_ = jsonBool(data, "matrices_loaded", false);
    currentGainFile_ = jsonString(data, "gain_file");
    currentOffsetFile_ = jsonString(data, "offset_file");
    currentWidth_ = jsonInt(data, "width", 0);
    currentHeight_ = jsonInt(data, "height", 0);
    currentEps_ = jsonDouble(data, "eps", 1e-6);
    if (currentEps_ <= 0.0) currentEps_ = 1e-6;

    const QSignalBlocker blocker(enabledCheck_);
    enabledCheck_->setChecked(currentEnabled_);
    matricesLabel_->setText(currentMatricesLoaded_ ? QString::fromUtf8("已加载") : QString::fromUtf8("未加载"));
    sizeLabel_->setText(currentWidth_ > 0 && currentHeight_ > 0
        ? QStringLiteral("%1 x %2").arg(currentWidth_).arg(currentHeight_)
        : QStringLiteral("-"));
    epsLabel_->setText(QString::number(currentEps_, 'g', 8));
    gainLabel_->setText(currentGainFile_.isEmpty() ? QStringLiteral("-") : currentGainFile_);
    offsetLabel_->setText(currentOffsetFile_.isEmpty() ? QStringLiteral("-") : currentOffsetFile_);

    const auto capture = data.find("capture");
    if (capture != data.end() && capture->is_object()) {
        frameCountSpin_->setValue(qBound(1, jsonInt(*capture, "frame_count", frameCountSpin_->value()), 10000));
        timeoutSpin_->setValue(qBound(1, jsonInt(*capture, "timeout_ms", timeoutSpin_->value()), 600000));
    }
    updateSelectionState();
}

void ColumnNucPanel::applyEnabledState()
{
    if (!device_ || !connected_ || configBusy_) return;
    configBusy_ = true;
    updateUiEnabled();
    device_->systemApi()->columnNucSetConfig(
        this, enabledCheck_->isChecked(), currentGainFile_, currentOffsetFile_,
        currentWidth_, currentHeight_, currentEps_, [this](const RpcResult& result) {
            configBusy_ = false;
            if (!result.ok) {
                enabledCheck_->setChecked(currentEnabled_);
                setResult(rpcErrorText(result.code, result.msg), true);
            } else {
                applyConfig(result.data);
                setResult(QString::fromUtf8("启用状态已应用"));
            }
            updateUiEnabled();
        });
}

void ColumnNucPanel::reloadMatrices()
{
    if (!device_ || !connected_ || configBusy_) return;
    configBusy_ = true;
    setResult(QString::fromUtf8("正在重新加载矩阵..."));
    updateUiEnabled();
    device_->systemApi()->columnNucReload(this, [this](const RpcResult& result) {
        if (!result.ok) {
            configBusy_ = false;
            setResult(rpcErrorText(result.code, result.msg), true);
            updateUiEnabled();
            return;
        }
        device_->systemApi()->columnNucGetConfig(this, [this](const RpcResult& configResult) {
            configBusy_ = false;
            if (!configResult.ok) {
                setResult(QString::fromUtf8("矩阵重载成功，但配置复核失败：%1")
                              .arg(rpcErrorText(configResult.code, configResult.msg)), true);
            } else {
                applyConfig(configResult.data);
                setResult(jsonBool(configResult.data, "matrices_loaded", false)
                    ? QString::fromUtf8("矩阵已重新加载")
                    : QString::fromUtf8("服务端未确认矩阵已加载"),
                    !jsonBool(configResult.data, "matrices_loaded", false));
            }
            updateUiEnabled();
        });
    });
}

void ColumnNucPanel::refreshCaptures()
{
    if (!device_ || !connected_) return;
    device_->systemApi()->columnNucListCaptures(this, 100, [this](const RpcResult& result) {
        if (!result.ok) {
            setStatus(rpcErrorText(result.code, result.msg), true);
            return;
        }
        applyCaptureList(result.data);
    });
}

void ColumnNucPanel::applyCaptureList(const nlohmann::json& data)
{
    lowItems_.clear();
    highItems_.clear();

    auto parseGroup = [](const nlohmann::json& root, const char* key, QVector<CaptureItem>* output) {
        const auto group = root.find(key);
        if (group == root.end() || !group->is_array() || !output) return;
        for (const auto& value : *group) {
            if (!value.is_object()) continue;
            CaptureItem item;
            item.captureId = jsonString(value, "capture_id");
            item.timestampNs = jsonU64(value, "timestamp_ns");
            item.level = jsonString(value, "level");
            item.rawPath = jsonString(value, "raw_path");
            item.jsonPath = jsonString(value, "json_path");
            item.frameCount = jsonInt(value, "frame_count");
            item.width = jsonInt(value, "width");
            item.height = jsonInt(value, "height");
            item.temperature = jsonDouble(value, "temperature");
            item.hasActualTemperature = value.contains("actual_temperature")
                && value["actual_temperature"].is_number();
            item.actualTemperature = jsonDouble(value, "actual_temperature");
            item.rawSizeBytes = jsonU64(value, "raw_size_bytes");
            item.jsonSizeBytes = jsonU64(value, "json_size_bytes");
            if (!item.captureId.isEmpty() && !item.rawPath.isEmpty()) output->append(item);
        }
    };

    parseGroup(data, "low", &lowItems_);
    parseGroup(data, "high", &highItems_);
    fillCaptureTable(lowTable_, lowItems_);
    fillCaptureTable(highTable_, highItems_);
    setStatus(QString::fromUtf8("列表已刷新：Low %1 条，High %2 条")
                  .arg(lowItems_.size()).arg(highItems_.size()));
    updateSelectionState();
}

void ColumnNucPanel::fillCaptureTable(QTableWidget* table, const QVector<CaptureItem>& items)
{
    if (!table) return;
    table->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const CaptureItem& item = items.at(row);
        table->setItem(row, 0, makeItem(item.captureId));
        table->setItem(row, 1, makeItem(formatTimestamp(item.timestampNs)));
        table->setItem(row, 2, makeItem(formatTemperature(item.temperature)));
        table->setItem(row, 3, makeItem(item.hasActualTemperature
            ? formatTemperature(item.actualTemperature) : QStringLiteral("-")));
        table->setItem(row, 4, makeItem(QString::number(item.frameCount)));
        table->setItem(row, 5, makeItem(QStringLiteral("%1 x %2").arg(item.width).arg(item.height)));
        table->setItem(row, 6, makeItem(formatBytes(item.rawSizeBytes)));
    }
    if (!items.isEmpty()) table->selectRow(0);
}

void ColumnNucPanel::startLowCapture()
{
    startCapture(QStringLiteral("low"), lowTemperatureSpin_->value());
}

void ColumnNucPanel::startHighCapture()
{
    startCapture(QStringLiteral("high"), highTemperatureSpin_->value());
}

void ColumnNucPanel::startCapture(const QString& level, double temperature)
{
    if (!device_ || !connected_ || captureBusy_) return;
    const QString levelName = level == QStringLiteral("low") ? QStringLiteral("Low") : QStringLiteral("High");
    const QString prompt = QString::fromUtf8(
        "服务端将把转镜移动到预设 1，并把黑体温度设置为 %1，随后采集 %2 帧未校正 Raw。\n\n是否开始 %3 采集？")
        .arg(formatTemperature(temperature))
        .arg(frameCountSpin_->value())
        .arg(levelName);
    if (QMessageBox::question(this, QString::fromUtf8("Column NUC 黑体采集"), prompt,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    captureBusy_ = true;
    activeTaskId_.clear();
    activeLevel_ = level;
    setStatus(QString::fromUtf8("正在创建 %1 采集任务...").arg(levelName));
    updateUiEnabled();

    device_->systemApi()->columnNucCapture(
        this, level, temperature, frameCountSpin_->value(), timeoutSpin_->value(),
        [this](const RpcResult& result) {
            if (!result.ok) {
                captureBusy_ = false;
                activeLevel_.clear();
                setStatus(rpcErrorText(result.code, result.msg), true);
                updateUiEnabled();
                return;
            }
            activeTaskId_ = jsonString(result.data, "task_id");
            activeLevel_ = jsonString(result.data, "level");
            if (activeTaskId_.isEmpty()) {
                captureBusy_ = false;
                activeLevel_.clear();
                setStatus(QString::fromUtf8("服务端未返回 task_id"), true);
                updateUiEnabled();
                return;
            }
            handleCaptureStatus(result.data);
        });
}

void ColumnNucPanel::pollCaptureStatus()
{
    if (!device_ || !connected_ || activeTaskId_.isEmpty() || captureStatusPending_) return;
    captureStatusPending_ = true;
    device_->systemApi()->columnNucCaptureStatus(this, activeTaskId_, [this](const RpcResult& result) {
        captureStatusPending_ = false;
        if (!result.ok) {
            setStatus(rpcErrorText(result.code, result.msg), true);
            if (result.code == -3) {
                stopCapturePolling(true);
            } else if (connected_ && !activeTaskId_.isEmpty()) {
                captureBusy_ = true;
                captureTimer_->start(1000);
            }
            updateUiEnabled();
            return;
        }
        handleCaptureStatus(result.data);
    });
}

void ColumnNucPanel::cancelCapture()
{
    if (!device_ || !connected_ || activeTaskId_.isEmpty()) return;
    captureTimer_->stop();
    setStatus(QString::fromUtf8("正在取消采集..."));
    updateUiEnabled();
    device_->systemApi()->columnNucCaptureCancel(this, activeTaskId_, [this](const RpcResult& result) {
        if (!result.ok) {
            setStatus(rpcErrorText(result.code, result.msg), true);
            if (connected_ && !activeTaskId_.isEmpty()) captureTimer_->start(1000);
            updateUiEnabled();
            return;
        }
        handleCaptureStatus(result.data);
    });
}

void ColumnNucPanel::handleCaptureStatus(const nlohmann::json& data)
{
    const QString state = jsonString(data, "state");
    const QString phase = jsonString(data, "phase");
    const QString level = jsonString(data, "level");
    const int captured = jsonInt(data, "captured_count", 0);
    const int total = jsonInt(data, "frame_count", frameCountSpin_->value());
    const double targetTemperature = jsonDouble(data, "temperature", 0.0);
    const bool hasActualTemperature = jsonBool(data, "has_actual_temperature", false);
    const double actualTemperature = jsonDouble(data, "actual_temperature", 0.0);
    const QString levelName = (level.isEmpty() ? activeLevel_ : level).toUpper();

    QStringList details;
    details << QString::fromUtf8("%1：%2").arg(levelName, phaseText(phase));
    if (targetTemperature != 0.0) details << QString::fromUtf8("目标 %1").arg(formatTemperature(targetTemperature));
    if (hasActualTemperature) details << QString::fromUtf8("实际 %1").arg(formatTemperature(actualTemperature));
    if (phase == QStringLiteral("capturing") || captured > 0) {
        details << QString::fromUtf8("进度 %1/%2").arg(captured).arg(total);
    }

    if (state == QStringLiteral("running")) {
        captureBusy_ = true;
        setStatus(details.join(QStringLiteral("；")));
        captureTimer_->start(500);
    } else if (state == QStringLiteral("completed")) {
        stopCapturePolling(true);
        setStatus(details.join(QStringLiteral("；")));
        refreshCaptures();
    } else if (state == QStringLiteral("failed")) {
        const QString error = jsonString(data, "error");
        stopCapturePolling(true);
        setStatus(error.isEmpty() ? details.join(QStringLiteral("；")) : error, true);
    } else if (state == QStringLiteral("cancelled")) {
        stopCapturePolling(true);
        setStatus(details.join(QStringLiteral("；")));
    } else {
        stopCapturePolling(true);
        setStatus(QString::fromUtf8("未知采集状态：%1").arg(state), true);
    }
    updateUiEnabled();
}

void ColumnNucPanel::stopCapturePolling(bool clearTask)
{
    captureTimer_->stop();
    captureStatusPending_ = false;
    captureBusy_ = false;
    if (clearTask) {
        activeTaskId_.clear();
        activeLevel_.clear();
    }
}

ColumnNucPanel::CaptureItem ColumnNucPanel::selectedLow() const
{
    const int row = lowTable_ ? lowTable_->currentRow() : -1;
    return row >= 0 && row < lowItems_.size() ? lowItems_.at(row) : CaptureItem();
}

ColumnNucPanel::CaptureItem ColumnNucPanel::selectedHigh() const
{
    const int row = highTable_ ? highTable_->currentRow() : -1;
    return row >= 0 && row < highItems_.size() ? highItems_.at(row) : CaptureItem();
}

bool ColumnNucPanel::selectedPairValid(QString* error) const
{
    const CaptureItem low = selectedLow();
    const CaptureItem high = selectedHigh();
    if (low.rawPath.isEmpty() || high.rawPath.isEmpty()) {
        if (error) *error = QString::fromUtf8("请分别选择 Low 和 High 采集");
        return false;
    }
    if (low.width <= 0 || low.height <= 0 || high.width <= 0 || high.height <= 0) {
        if (error) *error = QString::fromUtf8("所选采集的尺寸无效");
        return false;
    }
    if (low.width != high.width || low.height != high.height) {
        if (error) *error = QString::fromUtf8("Low/High 尺寸不一致");
        return false;
    }
    return true;
}

void ColumnNucPanel::updateSelectionState()
{
    const CaptureItem low = selectedLow();
    const CaptureItem high = selectedHigh();
    lowSelectionLabel_->setText(low.captureId.isEmpty() ? QStringLiteral("-")
        : QStringLiteral("%1 (%2 x %3, %4)")
              .arg(low.captureId).arg(low.width).arg(low.height).arg(formatTemperature(low.temperature)));
    highSelectionLabel_->setText(high.captureId.isEmpty() ? QStringLiteral("-")
        : QStringLiteral("%1 (%2 x %3, %4)")
              .arg(high.captureId).arg(high.width).arg(high.height).arg(formatTemperature(high.temperature)));
    updateUiEnabled();
}

void ColumnNucPanel::calibrateSelection()
{
    QString error;
    if (!selectedPairValid(&error)) {
        setResult(error, true);
        return;
    }

    const CaptureItem low = selectedLow();
    const CaptureItem high = selectedHigh();
    const QString prompt = QString::fromUtf8(
        "将使用以下服务端 Raw 生成矩阵并立即应用到运行态：\n\nLow：%1\nHigh：%2\n尺寸：%3 x %4\n\n是否继续？")
        .arg(low.captureId, high.captureId).arg(low.width).arg(low.height);
    if (QMessageBox::question(this, QString::fromUtf8("Column NUC 标定"), prompt,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    calibrating_ = true;
    setResult(QString::fromUtf8("正在生成并应用矩阵，请等待服务端返回..."));
    updateUiEnabled();
    device_->systemApi()->columnNucCalibrate(
        this, low.rawPath, high.rawPath, QStringLiteral("./raw_data/column_nuc_matrix"),
        low.width, low.height, currentEps_, true, [this](const RpcResult& result) {
            if (!result.ok) {
                calibrating_ = false;
                setResult(rpcErrorText(result.code, result.msg), true);
                updateUiEnabled();
                return;
            }

            const nlohmann::json calibrateData = result.data;
            device_->systemApi()->columnNucGetConfig(this, [this, calibrateData](const RpcResult& configResult) {
                calibrating_ = false;
                const bool cacheHit = jsonBool(calibrateData, "cache_hit", false);
                const bool applied = jsonBool(calibrateData, "applied", false);
                QStringList lines;
                lines << (cacheHit ? QString::fromUtf8("已复用缓存矩阵") : QString::fromUtf8("已生成新矩阵"));
                lines << QString::fromUtf8("服务端应用：%1").arg(applied ? QString::fromUtf8("是") : QString::fromUtf8("否"));
                const QString cacheDir = jsonString(calibrateData, "cache_dir");
                if (!cacheDir.isEmpty()) lines << QString::fromUtf8("缓存目录：%1").arg(cacheDir);

                bool verified = false;
                if (configResult.ok) {
                    applyConfig(configResult.data);
                    verified = jsonBool(configResult.data, "matrices_loaded", false);
                    lines << QString::fromUtf8("配置复核：%1")
                                 .arg(verified ? QString::fromUtf8("矩阵已加载")
                                               : QString::fromUtf8("矩阵未加载"));
                } else {
                    lines << QString::fromUtf8("配置复核失败：%1")
                                 .arg(rpcErrorText(configResult.code, configResult.msg));
                }
                setResult(lines.join(QStringLiteral("\n")), !(applied && verified));
                updateUiEnabled();
            });
        });
}

void ColumnNucPanel::setStatus(const QString& text, bool isError)
{
    captureStatusLabel_->setText(text);
    captureStatusLabel_->setProperty("error", isError);
    captureStatusLabel_->style()->polish(captureStatusLabel_);
}

void ColumnNucPanel::setResult(const QString& text, bool isError)
{
    resultLabel_->setText(text);
    resultLabel_->setProperty("error", isError);
    resultLabel_->style()->polish(resultLabel_);
}

void ColumnNucPanel::updateUiEnabled()
{
    const bool remoteReady = connected_ && !configBusy_ && !calibrating_;
    const bool captureReady = remoteReady && !captureBusy_;
    enabledCheck_->setEnabled(remoteReady && !captureBusy_);
    applyEnabledBtn_->setEnabled(remoteReady && !captureBusy_);
    reloadBtn_->setEnabled(remoteReady && !captureBusy_);
    refreshBtn_->setEnabled(remoteReady && !captureBusy_);
    frameCountSpin_->setEnabled(captureReady);
    timeoutSpin_->setEnabled(captureReady);
    lowTemperatureSpin_->setEnabled(captureReady);
    highTemperatureSpin_->setEnabled(captureReady);
    captureLowBtn_->setEnabled(captureReady);
    captureHighBtn_->setEnabled(captureReady);
    cancelBtn_->setEnabled(remoteReady && captureBusy_ && !activeTaskId_.isEmpty());

    QString error;
    const bool canCalibrate = remoteReady && !captureBusy_ && selectedPairValid(&error);
    calibrateBtn_->setEnabled(canCalibrate);
    calibrateBtn_->setToolTip(canCalibrate ? QString() : error);
}

QString ColumnNucPanel::rpcErrorText(int code, const QString& message) const
{
    if (code == -12) return QString::fromUtf8("任务或资源忙：已有采集或校正任务运行中");
    if (code == -11) return QString::fromUtf8("设备未就绪或正在维护");
    if (code == -10) return QString::fromUtf8("转镜、相机或温控子系统未绑定");
    if (code == -3) return QString::fromUtf8("参数错误或任务不存在：%1").arg(message);
    if (code == -22) return QString::fromUtf8("温控仪通信失败：%1").arg(message);
    if (code == -254) return QString::fromUtf8("请求超时");
    if (code == -255) return QString::fromUtf8("连接已断开");
    if (!message.isEmpty()) return message;
    return QString::fromUtf8("RPC 调用失败，错误码 %1").arg(code);
}
