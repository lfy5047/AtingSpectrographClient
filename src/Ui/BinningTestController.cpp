#include "BinningTestController.h"

#include "BinningTestTypes.h"
#include "DeviceClient.h"
#include "ImageFrameUtils.h"
#include "Protocol.h"
#include "panels/BinningTestPanel.h"
#include "widgets/BinningCompareWidget.h"

#include <QTimer>

namespace {

const int kTestFactors[] = {1, 2, 4};

QString errorText(const QString& prefix, const QString& detail)
{
    return detail.isEmpty() ? prefix : QStringLiteral("%1：%2").arg(prefix, detail);
}

QString sourceName(int channel)
{
    return channel == cli::proto::SliceStitch16
        ? QStringLiteral("SliceStitch16") : QStringLiteral("Raw16");
}

} // namespace

BinningTestController::BinningTestController(DeviceClient* device,
                                             BinningTestPanel* panel,
                                             BinningCompareWidget* compare,
                                             QObject* parent)
    : QObject(parent)
    , device_(device)
    , panel_(panel)
    , compare_(compare)
{
    frameTimeout_.setSingleShot(true);
    frameTimeout_.setInterval(4000);

    connect(&frameTimeout_, &QTimer::timeout, this, [this]() {
        if (testRunning_ && awaitingFrame_) {
            failTest(QString::fromUtf8("等待当前倍率的 %1 Mono16 稳定帧超时")
                         .arg(sourceName(sourceChannel_)));
        }
    });
    connect(panel_, &BinningTestPanel::refreshRequested,
            this, &BinningTestController::refreshConfig);
    connect(panel_, &BinningTestPanel::applyFactorRequested,
            this, &BinningTestController::applyManualFactor);
    connect(panel_, &BinningTestPanel::startRequested,
            this, &BinningTestController::startTest);
    connect(panel_, &BinningTestPanel::cancelRequested,
            this, &BinningTestController::cancelTest);
    connect(panel_, &BinningTestPanel::measurementOrientationChanged,
            this, &BinningTestController::resetMeasurements);
    connect(compare_, &BinningCompareWidget::measurementChanged,
            this, &BinningTestController::handleMeasurement);
    connect(device_, &DeviceClient::frameReady,
            this, &BinningTestController::handleFrame, Qt::QueuedConnection);
    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool connected, const QString&) {
        if (connected) {
            if (restorePending_ && originalConfigValid_) {
                restoreOriginalConfig(QString::fromUtf8("重连后已恢复测试前配置"));
            } else {
                refreshConfig();
            }
            return;
        }

        if (testRunning_ || restoring_) {
            ++operation_;
            testRunning_ = false;
            restoring_ = false;
            awaitingFrame_ = false;
            frameTimeout_.stop();
            restorePending_ = originalConfigValid_;
            panel_->setBusy(false);
            panel_->setStatusText(
                restorePending_
                    ? QString::fromUtf8("设备连接中断；重新连接后将恢复测试前配置")
                    : QString::fromUtf8("设备连接中断"),
                true);
        }
    });
}

void BinningTestController::refreshConfig()
{
    if (!device_->isConnected()) {
        panel_->setStatusText(QString::fromUtf8("设备未连接"), true);
        return;
    }
    if (testRunning_ || restoring_) return;
    if (restorePending_ && originalConfigValid_) {
        restoreOriginalConfig(QString::fromUtf8("已重试并恢复测试前配置"));
        return;
    }

    const quint64 operation = ++operation_;
    device_->binning()->getConfig(this,
        [this, operation](bool ok, const BinningConfig& config, const QString& err) {
        if (operation != operation_ || testRunning_ || restoring_) return;
        if (!ok) {
            panel_->setStatusText(errorText(QString::fromUtf8("读取 Binning 配置失败"), err), true);
            return;
        }
        panel_->setCurrentConfig(config);
        panel_->setStatusText(QString::fromUtf8("Binning 配置已读取"));
    });
}

bool BinningTestController::prepareForClose()
{
    if (testRunning_) cancelTest();
    return !testRunning_ && !restoring_ && !restorePending_;
}

QString BinningTestController::closeBlockReason() const
{
    if (restorePending_) {
        return QString::fromUtf8("测试前配置尚未恢复。请保持或重新连接设备，并点击“读取”重试恢复后再关闭。");
    }
    return QString::fromUtf8("正在恢复 Binning 测试前配置，请稍后再关闭。");
}

BinningConfig BinningTestController::symmetricConfig(int factor)
{
    BinningConfig config;
    config.enabled = factor != 1;
    config.spectralFactor = factor;
    config.spatialFactor = factor;
    return config;
}

bool BinningTestController::configMatches(const BinningConfig& config, int factor)
{
    return isSupportedBinningFactor(factor)
        && config.enabled == (factor != 1)
        && config.spectralFactor == factor
        && config.spatialFactor == factor;
}

int BinningTestController::measurementIndex(int factor)
{
    if (factor == 1) return 0;
    if (factor == 2) return 1;
    if (factor == 4) return 2;
    return -1;
}

void BinningTestController::startTest()
{
    if (testRunning_ || restoring_) return;
    if (restorePending_) {
        panel_->setStatusText(
            QString::fromUtf8("测试前配置尚未恢复，请先点击“读取”重试恢复"), true);
        return;
    }
    if (!device_->isConnected()) {
        panel_->setStatusText(QString::fromUtf8("设备未连接，无法开始测试"), true);
        return;
    }

    const quint64 operation = ++operation_;
    testRunning_ = true;
    originalConfigValid_ = false;
    restorePending_ = false;
    awaitingFrame_ = false;
    stepIndex_ = 0;
    currentFactor_ = 1;
    sourceChannel_ = panel_->sourceChannel();
    baselineSize_ = QSize();
    measurements_.fill(-1);
    compare_->clearSnapshots();
    panel_->resetResults();
    panel_->setBusy(true, true);
    panel_->setProgress(0, QString::fromUtf8("读取测试前配置"));
    panel_->setStatusText(QString::fromUtf8("正在读取测试前 Binning 配置..."));

    device_->binning()->getConfig(this,
        [this, operation](bool ok, const BinningConfig& config, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok) {
            originalConfigValid_ = false;
            failTest(errorText(QString::fromUtf8("读取测试前配置失败"), err));
            return;
        }
        originalConfig_ = config;
        originalConfigValid_ = true;
        beginCurrentFactor(operation);
    });
}

void BinningTestController::cancelTest()
{
    if (!testRunning_) return;
    ++operation_;
    testRunning_ = false;
    awaitingFrame_ = false;
    frameTimeout_.stop();
    finishAndRestore(QString::fromUtf8("测试已取消，已恢复测试前配置"));
}

void BinningTestController::applyManualFactor(int factor)
{
    if (testRunning_ || restoring_ || !isSupportedBinningFactor(factor)) return;
    if (restorePending_) {
        panel_->setStatusText(
            QString::fromUtf8("测试前配置尚未恢复，请先点击“读取”重试恢复"), true);
        return;
    }
    if (!device_->isConnected()) {
        panel_->setStatusText(QString::fromUtf8("设备未连接，无法应用配置"), true);
        return;
    }

    const quint64 operation = ++operation_;
    panel_->setBusy(true, false);
    panel_->setStatusText(QString::fromUtf8("正在应用 %1x%1...").arg(factor));
    device_->binning()->setConfig(this, symmetricConfig(factor),
        [this, operation, factor](bool ok, const BinningConfig&, const QString& err) {
        if (operation != operation_) return;
        if (!ok) {
            panel_->setBusy(false);
            panel_->setStatusText(errorText(QString::fromUtf8("应用 Binning 配置失败"), err), true);
            return;
        }
        device_->binning()->getConfig(this,
            [this, operation, factor](bool readOk, const BinningConfig& config, const QString& readErr) {
            if (operation != operation_) return;
            panel_->setBusy(false);
            if (!readOk || !configMatches(config, factor)) {
                panel_->setStatusText(errorText(QString::fromUtf8("配置回读不一致"), readErr), true);
                return;
            }
            panel_->setCurrentConfig(config);
            panel_->setStatusText(QString::fromUtf8("已应用并确认 %1x%1").arg(factor));
        });
    });
}

void BinningTestController::beginCurrentFactor(quint64 operation)
{
    if (operation != operation_ || !testRunning_) return;
    if (stepIndex_ >= 3) {
        testRunning_ = false;
        finishAndRestore(QString::fromUtf8("三组图像采集完成，已恢复测试前配置"));
        return;
    }

    currentFactor_ = kTestFactors[stepIndex_];
    panel_->setProgress(stepIndex_, QString::fromUtf8("正在设置 %1x%1").arg(currentFactor_));
    panel_->setStatusText(QString::fromUtf8("正在设置并回读 %1x%1 配置...").arg(currentFactor_));

    device_->binning()->setConfig(this, symmetricConfig(currentFactor_),
        [this, operation](bool ok, const BinningConfig& config, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok || !configMatches(config, currentFactor_)) {
            failTest(errorText(QString::fromUtf8("设置 %1x%1 失败").arg(currentFactor_), err));
            return;
        }

        device_->binning()->getConfig(this,
            [this, operation](bool readOk, const BinningConfig& readConfig, const QString& readErr) {
            if (operation != operation_ || !testRunning_) return;
            if (!readOk || !configMatches(readConfig, currentFactor_)) {
                failTest(errorText(QString::fromUtf8("%1x%1 配置回读不一致").arg(currentFactor_), readErr));
                return;
            }

            panel_->setCurrentConfig(readConfig);
            panel_->setStatusText(QString::fromUtf8("等待 %1x%1 %2 稳定图像...")
                                      .arg(currentFactor_)
                                      .arg(sourceName(sourceChannel_)));
            panel_->setProgress(stepIndex_, QString::fromUtf8("等待 %1x%1 %2 图像")
                                                    .arg(currentFactor_)
                                                    .arg(sourceName(sourceChannel_)));
            awaitingFrame_ = true;
            settleFramesRemaining_ = 2;
            frameFence_ = sourceChannel_ == cli::proto::Raw16
                ? lastRawFrameId_ : lastSliceFrameId_;
            frameTimeout_.start();
        });
    });
}

void BinningTestController::handleFrame(const StreamFrame& frame)
{
    using namespace cli::proto;
    if (frame.pixfmt != Mono16) return;
    if (frame.channel == Raw16) {
        lastRawFrameId_ = frame.streamFrameId;
    } else if (frame.channel == SliceStitch16) {
        lastSliceFrameId_ = frame.streamFrameId;
    } else {
        return;
    }
    if (frame.channel != sourceChannel_) return;
    if (!testRunning_ || !awaitingFrame_) return;

    if (frame.streamFrameId != 0 && frameFence_ != 0 && frame.streamFrameId == frameFence_) return;

    if (currentFactor_ != 1 && baselineSize_.isValid()) {
        const QSize expected = expectedBinningSize(baselineSize_, currentFactor_);
        if (frame.width != expected.width() || frame.height != expected.height()) return;
    }

    if (settleFramesRemaining_ > 0) {
        --settleFramesRemaining_;
        return;
    }
    captureFrame(frame);
}

void BinningTestController::captureFrame(const StreamFrame& frame)
{
    awaitingFrame_ = false;
    frameTimeout_.stop();

    const QSize actual(frame.width, frame.height);
    if (currentFactor_ == 1) baselineSize_ = actual;
    const QSize expected = expectedBinningSize(baselineSize_, currentFactor_);
    const bool dimensionsPassed = expected.isValid() && actual == expected;

    BinningSnapshot snapshot;
    snapshot.factor = currentFactor_;
    snapshot.width = frame.width;
    snapshot.height = frame.height;
    snapshot.streamFrameId = frame.streamFrameId;
    snapshot.data = frame.data;
    snapshot.stats = makeChannelImageStats(frame.width, frame.height, frame.pixfmt, frame.data);
    if (!snapshot.stats.valid) {
        failTest(QString::fromUtf8("%1x%1 图像数据长度或格式无效").arg(currentFactor_));
        return;
    }

    compare_->setSnapshot(snapshot);
    panel_->setCaptureResult(currentFactor_, true, expected, actual);
    if (!dimensionsPassed) {
        failTest(QString::fromUtf8("%1x%1 实际尺寸 %2x%3 与理论尺寸 %4x%5 不一致")
            .arg(currentFactor_)
            .arg(actual.width()).arg(actual.height())
            .arg(expected.width()).arg(expected.height()));
        return;
    }

    ++stepIndex_;
    panel_->setProgress(stepIndex_, QString::fromUtf8("已采集 %1/3").arg(stepIndex_));
    const quint64 operation = operation_;
    QTimer::singleShot(0, this, [this, operation]() { beginCurrentFactor(operation); });
}

void BinningTestController::failTest(const QString& message)
{
    ++operation_;
    testRunning_ = false;
    awaitingFrame_ = false;
    frameTimeout_.stop();
    panel_->setStatusText(message, true);
    finishAndRestore(QString::fromUtf8("%1；已恢复测试前配置").arg(message), true);
}

void BinningTestController::finishAndRestore(const QString& completionText, bool error)
{
    if (!originalConfigValid_) {
        restoring_ = false;
        panel_->setBusy(false);
        panel_->setStatusText(completionText, error);
        return;
    }
    restoreOriginalConfig(completionText, error);
}

void BinningTestController::restoreOriginalConfig(const QString& completionText, bool error)
{
    if (!originalConfigValid_) return;
    if (!device_->isConnected()) {
        restorePending_ = true;
        restoring_ = false;
        panel_->setBusy(false);
        panel_->setStatusText(QString::fromUtf8("设备已断开；重连后将恢复测试前配置"), true);
        return;
    }

    restoring_ = true;
    restorePending_ = false;
    panel_->setBusy(true, false);
    const quint64 operation = ++operation_;
    device_->binning()->setConfig(this, originalConfig_,
        [this, operation, completionText, error](bool ok, const BinningConfig& config, const QString& err) {
        if (operation != operation_) return;
        restoring_ = false;
        panel_->setBusy(false);
        if (!ok) {
            restorePending_ = true;
            panel_->setStatusText(errorText(QString::fromUtf8("恢复测试前配置失败"), err), true);
            return;
        }
        originalConfigValid_ = false;
        restorePending_ = false;
        panel_->setCurrentConfig(config);
        panel_->setProgress(3, QString::fromUtf8("采集完成"));
        panel_->setStatusText(completionText, error);
    });
}

void BinningTestController::handleMeasurement(int factor, int pixels)
{
    const int index = measurementIndex(factor);
    if (index < 0) return;
    measurements_[static_cast<std::size_t>(index)] = pixels;
    updateMeasurementResults();
}

void BinningTestController::resetMeasurements(Qt::Orientation orientation)
{
    compare_->setMeasurementOrientation(orientation);
    measurements_.fill(-1);
    for (int factor : kTestFactors) {
        panel_->setMeasurementResult(factor, -1);
    }
}

void BinningTestController::updateMeasurementResults()
{
    for (int index = 0; index < 3; ++index) {
        const int pixels = measurements_[static_cast<std::size_t>(index)];
        panel_->setMeasurementResult(kTestFactors[index], pixels);
    }
}
