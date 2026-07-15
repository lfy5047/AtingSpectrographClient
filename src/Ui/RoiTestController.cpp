#include "RoiTestController.h"

#include "DeviceClient.h"
#include "ImageFrameUtils.h"
#include "Protocol.h"
#include "RoiTestTypes.h"
#include "panels/RoiTestPanel.h"
#include "widgets/RoiCompareWidget.h"

#include <QTimer>

namespace {

RoiConfig defaultWindowConfig()
{
    RoiConfig config;
    config.sliceBegin = 240;
    config.sliceEnd = 435;
    config.sliceHBegin = 0;
    config.sliceHEnd = 512;
    return config;
}

} // namespace

RoiTestController::RoiTestController(DeviceClient* device,
                                     RoiTestPanel* panel,
                                     RoiCompareWidget* compare,
                                     QObject* parent)
    : QObject(parent)
    , device_(device)
    , panel_(panel)
    , compare_(compare)
{
    frameTimeout_.setSingleShot(true);
    frameTimeout_.setInterval(8000);

    connect(&frameTimeout_, &QTimer::timeout, this, [this]() {
        if (testRunning_ && awaitingFrame_) {
            failTest(QString::fromUtf8("等待 %1 SliceStitch16 数据帧超时")
                .arg(snapshotStage_ == FullFrameSnapshot
                         ? QString::fromUtf8("全幅") : QString::fromUtf8("开窗")));
        }
    });
    connect(panel_, &RoiTestPanel::startRequested,
            this, &RoiTestController::startTest);
    connect(panel_, &RoiTestPanel::cancelRequested,
            this, &RoiTestController::cancelTest);
    connect(device_, &DeviceClient::frameReady,
            this, &RoiTestController::handleFrame, Qt::QueuedConnection);
    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool connected, const QString&) {
        if (!connected) {
            handleDisconnected();
            return;
        }
        if (restorePending_) retryPendingRestore();
    });
}

bool RoiTestController::roiMatches(const RoiConfig& lhs, const RoiConfig& rhs)
{
    return lhs.sliceBegin == rhs.sliceBegin
        && lhs.sliceEnd == rhs.sliceEnd
        && lhs.sliceHBegin == rhs.sliceHBegin
        && lhs.sliceHEnd == rhs.sliceHEnd;
}

bool RoiTestController::gateMatches(const CollectGateConfig& lhs,
                                    const CollectGateConfig& rhs)
{
    return lhs.discardFrontMs == rhs.discardFrontMs
        && lhs.discardBackMs == rhs.discardBackMs
        && lhs.forwardOffsetFrames == rhs.forwardOffsetFrames
        && lhs.reverseOffsetFrames == rhs.reverseOffsetFrames
        && lhs.staticCollectMode == rhs.staticCollectMode;
}

QSize RoiTestController::roiSize(const RoiConfig& config)
{
    const int width = config.sliceEnd - config.sliceBegin;
    const int height = config.sliceHEnd - config.sliceHBegin;
    return width > 0 && height > 0 ? QSize(width, height) : QSize();
}

QString RoiTestController::errorText(const QString& prefix, const QString& detail)
{
    return detail.isEmpty() ? prefix : QStringLiteral("%1：%2").arg(prefix, detail);
}

bool RoiTestController::validateRoi(const RoiConfig& config,
                                    const QSize& resolution,
                                    QString* error) const
{
    if (!resolution.isValid()) {
        if (error) *error = QString::fromUtf8("相机分辨率无效");
        return false;
    }
    if (config.sliceBegin < 0 || config.sliceBegin >= config.sliceEnd
        || config.sliceEnd > resolution.width()) {
        if (error) {
            *error = QString::fromUtf8("光谱列区间必须满足 0 <= begin < end <= %1")
                .arg(resolution.width());
        }
        return false;
    }
    if (config.sliceHBegin < 0 || config.sliceHBegin >= config.sliceHEnd
        || config.sliceHEnd > resolution.height()) {
        if (error) {
            *error = QString::fromUtf8("空间行区间必须满足 0 <= begin < end <= %1")
                .arg(resolution.height());
        }
        return false;
    }
    return true;
}

bool RoiTestController::prepareForClose()
{
    if (testRunning_) cancelTest();
    return !testRunning_ && !restoring_ && !restorePending_;
}

QString RoiTestController::closeBlockReason() const
{
    if (restorePending_) {
        return QString::fromUtf8("开窗测试前配置尚未恢复。请保持或重新连接设备，等待恢复完成后再关闭。");
    }
    return QString::fromUtf8("正在停止静态采集并恢复开窗测试前配置，请稍后再关闭。");
}

void RoiTestController::startTest()
{
    if (testRunning_ || restoring_) return;
    if (restorePending_) {
        panel_->setStatusText(QString::fromUtf8("测试前配置尚未恢复，请先点击“读取”重试"), true);
        return;
    }
    if (!device_->isConnected()) {
        panel_->setStatusText(QString::fromUtf8("设备未连接，无法开始测试"), true);
        return;
    }

    applyWindowing_ = panel_->applyWindowing();
    targetConfig_ = defaultWindowConfig();
    testRunning_ = true;
    originalRoiValid_ = false;
    originalGateValid_ = false;
    roiChanged_ = false;
    gateChanged_ = false;
    collectionStarted_ = false;
    awaitingFrame_ = false;
    snapshotStage_ = NoSnapshot;
    compare_->clearSnapshots();
    panel_->resetResults();
    panel_->setBusy(true, true);
    panel_->setProgress(0, QString::fromUtf8("检查 Binning 配置"));
    panel_->setStatusText(QString::fromUtf8("正在确认 Binning 为透传/1x1..."));

    const quint64 operation = ++operation_;
    device_->binning()->getConfig(this,
        [this, operation](bool ok, const BinningConfig& config, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok) {
            failTest(errorText(QString::fromUtf8("读取 Binning 配置失败"), err));
            return;
        }
        if (config.spectralFactor != 1 || config.spatialFactor != 1) {
            failTest(QString::fromUtf8("开窗测试要求 Binning 为透传/1x1，请先调整 Binning 配置"));
            return;
        }

        panel_->setStatusText(QString::fromUtf8("正在读取全幅分辨率..."));
        device_->camera()->getResolution(this,
            [this, operation](bool resolutionOk, int width, int height, const QString& resolutionErr) {
            if (operation != operation_ || !testRunning_) return;
            if (!resolutionOk || width <= 0 || height <= 0) {
                failTest(errorText(QString::fromUtf8("读取相机分辨率失败"), resolutionErr));
                return;
            }
            resolution_ = QSize(width, height);
            QString validationError;
            if (applyWindowing_
                && !validateRoi(targetConfig_, resolution_, &validationError)) {
                failTest(QString::fromUtf8("默认开窗参数无效：%1").arg(validationError));
                return;
            }
            fullFrameConfig_ = RoiConfig();
            fullFrameConfig_.sliceEnd = width;
            fullFrameConfig_.sliceHEnd = height;
            readOriginalRoi(operation);
        });
    });
}

void RoiTestController::cancelTest()
{
    if (!testRunning_) return;
    testRunning_ = false;
    awaitingFrame_ = false;
    frameTimeout_.stop();
    beginRestore(QString::fromUtf8("测试已取消，已恢复测试前配置"));
}

void RoiTestController::readOriginalRoi(quint64 operation)
{
    panel_->setStatusText(QString::fromUtf8("正在保存测试前开窗配置..."));
    device_->roi()->getConfig(this,
        [this, operation](bool ok, const RoiConfig& config, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok) {
            failTest(errorText(QString::fromUtf8("读取测试前开窗配置失败"), err));
            return;
        }
        if (config.collecting) {
            failTest(QString::fromUtf8("设备正在采集，请先停止当前采集后再运行开窗测试"));
            return;
        }
        originalRoi_ = config;
        originalRoiValid_ = true;
        readOriginalGate(operation);
    });
}

void RoiTestController::readOriginalGate(quint64 operation)
{
    panel_->setStatusText(QString::fromUtf8("正在保存采集门控配置..."));
    device_->collect()->getGateConfig(this,
        [this, operation](bool ok, const CollectGateConfig& config, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok) {
            failTest(errorText(QString::fromUtf8("读取采集门控配置失败"), err));
            return;
        }
        if (config.collecting) {
            failTest(QString::fromUtf8("设备正在采集，请先停止当前采集后再运行开窗测试"));
            return;
        }
        originalGate_ = config;
        originalGateValid_ = true;
        enableStaticMode(operation);
    });
}

void RoiTestController::enableStaticMode(quint64 operation)
{
    CollectGateConfig staticConfig = originalGate_;
    staticConfig.staticCollectMode = true;
    gateChanged_ = originalGateValid_;
    panel_->setStatusText(QString::fromUtf8("正在启用静态采集模式..."));
    device_->collect()->setGateConfig(this, staticConfig,
        [this, operation, staticConfig](bool ok, const CollectGateConfig& updated, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok || !gateMatches(updated, staticConfig) || updated.pendingConfig) {
            failTest(errorText(QString::fromUtf8("启用静态采集模式失败"), err));
            return;
        }
        configureFullFrame(operation);
    });
}

void RoiTestController::configureFullFrame(quint64 operation)
{
    roiChanged_ = originalRoiValid_;
    panel_->setStatusText(QString::fromUtf8("正在设置并回读全幅配置..."));
    device_->roi()->setConfig(this, fullFrameConfig_,
        [this, operation](bool ok, const RoiConfig& updated, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok || !roiMatches(updated, fullFrameConfig_) || updated.pendingApply) {
            failTest(errorText(QString::fromUtf8("设置全幅配置失败"), err));
            return;
        }
        device_->roi()->getConfig(this,
            [this, operation](bool readOk, const RoiConfig& readback, const QString& readErr) {
            if (operation != operation_ || !testRunning_) return;
            if (!readOk || !roiMatches(readback, fullFrameConfig_) || readback.pendingApply) {
                failTest(errorText(QString::fromUtf8("全幅配置回读不一致"), readErr));
                return;
            }
            startStaticCollection(operation);
        });
    });
}

void RoiTestController::startStaticCollection(quint64 operation)
{
    panel_->setProgress(0, QString::fromUtf8("启动静态采集"));
    panel_->setStatusText(QString::fromUtf8("正在启动静态采集..."));
    collectionStarted_ = true;
    device_->collect()->start(this, [this, operation](bool ok, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok) {
            failTest(errorText(QString::fromUtf8("启动静态采集失败"), err));
            return;
        }
        waitForSnapshot(FullFrameSnapshot, fullFrameConfig_);
    });
}

void RoiTestController::configureTargetRoi(quint64 operation)
{
    if (operation != operation_ || !testRunning_) return;
    roiChanged_ = originalRoiValid_;
    panel_->setProgress(1, QString::fromUtf8("应用默认开窗"));
    panel_->setStatusText(QString::fromUtf8("正在设置并回读默认开窗..."));
    device_->roi()->setConfig(this, targetConfig_,
        [this, operation](bool ok, const RoiConfig& updated, const QString& err) {
        if (operation != operation_ || !testRunning_) return;
        if (!ok || !roiMatches(updated, targetConfig_)) {
            failTest(errorText(QString::fromUtf8("设置默认开窗失败"), err));
            return;
        }
        device_->roi()->getConfig(this,
            [this, operation](bool readOk, const RoiConfig& readback, const QString& readErr) {
            if (operation != operation_ || !testRunning_) return;
            if (!readOk || !roiMatches(readback, targetConfig_)) {
                failTest(errorText(QString::fromUtf8("默认开窗配置回读不一致"), readErr));
                return;
            }
            waitForSnapshot(TargetRoiSnapshot, targetConfig_);
        });
    });
}

void RoiTestController::waitForSnapshot(SnapshotStage stage, const RoiConfig& config)
{
    snapshotStage_ = stage;
    awaitedConfig_ = config;
    awaitingFrame_ = true;
    frameFence_ = lastSliceFrameId_;
    frameTimeout_.start();
    const QSize expected = roiSize(config);
    panel_->setStatusText(QString::fromUtf8("等待 %1 %2x%3 后续数据图像...")
        .arg(stage == FullFrameSnapshot ? QString::fromUtf8("全幅") : QString::fromUtf8("开窗"))
        .arg(expected.width()).arg(expected.height()));
}

void RoiTestController::handleFrame(const StreamFrame& frame)
{
    using namespace cli::proto;
    if (frame.channel != SliceStitch16) return;
    lastSliceFrameId_ = frame.streamFrameId;
    if (!testRunning_ || !awaitingFrame_) return;
    if (frame.pixfmt != Mono16
        || (frame.frameType != DataFrame && frame.frameType != TailFrame)) return;
    if (frame.streamFrameId != 0 && frameFence_ != 0
        && frame.streamFrameId <= frameFence_) return;

    const QSize expected = roiSize(awaitedConfig_);
    if (frame.width != expected.width() || frame.height != expected.height()) return;
    captureFrame(frame);
}

void RoiTestController::captureFrame(const StreamFrame& frame)
{
    awaitingFrame_ = false;
    frameTimeout_.stop();

    RoiSnapshot snapshot;
    snapshot.width = frame.width;
    snapshot.height = frame.height;
    snapshot.streamFrameId = frame.streamFrameId;
    snapshot.data = frame.data;
    snapshot.stats = makeChannelImageStats(frame.width, frame.height, frame.pixfmt, frame.data);
    if (!snapshot.stats.valid) {
        failTest(QString::fromUtf8("收到的开窗测试图像数据长度或格式无效"));
        return;
    }

    const QSize actual(frame.width, frame.height);
    const QSize expected = roiSize(awaitedConfig_);
    if (snapshotStage_ == FullFrameSnapshot) {
        compare_->setFullFrameSnapshot(snapshot);
        panel_->setCaptureResult(true, expected, actual);
        panel_->setProgress(1, QString::fromUtf8("全幅图采集完成"));
        if (!applyWindowing_) {
            panel_->setProgress(2, QString::fromUtf8("全幅图采集完成（未应用开窗）"));
            testRunning_ = false;
            beginRestore(QString::fromUtf8("全幅图像采集完成，未应用开窗，已恢复测试前配置"));
            return;
        }
        const quint64 operation = operation_;
        QTimer::singleShot(0, this, [this, operation]() { configureTargetRoi(operation); });
        return;
    }

    compare_->setRoiSnapshot(snapshot);
    panel_->setCaptureResult(false, expected, actual);
    panel_->setProgress(2, QString::fromUtf8("全幅与开窗图采集完成"));
    testRunning_ = false;
    beginRestore(QString::fromUtf8("全幅与开窗图像采集完成，已恢复测试前配置"));
}

void RoiTestController::failTest(const QString& message)
{
    testRunning_ = false;
    awaitingFrame_ = false;
    frameTimeout_.stop();
    const bool needsRestore = collectionStarted_ || roiChanged_ || gateChanged_;
    if (!needsRestore) {
        ++operation_;
        originalRoiValid_ = false;
        originalGateValid_ = false;
        panel_->setBusy(false);
        panel_->setStatusText(message, true);
        return;
    }
    beginRestore(QString::fromUtf8("%1；已恢复测试前配置").arg(message), true);
}

void RoiTestController::beginRestore(const QString& completionText, bool error)
{
    awaitingFrame_ = false;
    frameTimeout_.stop();
    completionText_ = completionText;
    completionError_ = error;
    restoring_ = true;
    restorePending_ = false;
    panel_->setBusy(true, false);
    panel_->setStatusText(QString::fromUtf8("正在停止静态采集并恢复测试前配置..."));
    const quint64 operation = ++operation_;
    if (!device_->isConnected()) {
        markRestorePending(QString::fromUtf8("设备已断开；重连后将停止采集并恢复测试前配置"));
        return;
    }
    stopCollectionForRestore(operation, false);
}

void RoiTestController::retryPendingRestore()
{
    if (!device_->isConnected() || restoring_) return;
    restoring_ = true;
    restorePending_ = false;
    panel_->setBusy(true, false);
    panel_->setStatusText(QString::fromUtf8("正在重连后停止残留采集并恢复配置..."));
    const quint64 operation = ++operation_;
    stopCollectionForRestore(operation, true);
}

void RoiTestController::stopCollectionForRestore(quint64 operation, bool queryFirst)
{
    if (operation != operation_ || !restoring_) return;
    if (queryFirst) {
        device_->collect()->status(this,
            [this, operation](bool ok, bool collecting, const QString& err) {
            if (operation != operation_ || !restoring_) return;
            if (!ok) {
                markRestorePending(errorText(QString::fromUtf8("查询采集状态失败"), err));
                return;
            }
            collectionStarted_ = collecting;
            stopCollectionForRestore(operation, false);
        });
        return;
    }

    if (!collectionStarted_) {
        restoreOriginalRoi(operation);
        return;
    }
    device_->collect()->stop(this, [this, operation](bool ok, const QString& err) {
        if (operation != operation_ || !restoring_) return;
        if (!ok) {
            markRestorePending(errorText(QString::fromUtf8("停止静态采集失败"), err));
            return;
        }
        collectionStarted_ = false;
        restoreOriginalRoi(operation);
    });
}

void RoiTestController::restoreOriginalRoi(quint64 operation)
{
    if (operation != operation_ || !restoring_) return;
    if (!roiChanged_ || !originalRoiValid_) {
        restoreOriginalGate(operation);
        return;
    }
    device_->roi()->setConfig(this, originalRoi_,
        [this, operation](bool ok, const RoiConfig& updated, const QString& err) {
        if (operation != operation_ || !restoring_) return;
        if (!ok || !roiMatches(updated, originalRoi_) || updated.pendingApply) {
            markRestorePending(errorText(QString::fromUtf8("恢复测试前开窗配置失败"), err));
            return;
        }
        roiChanged_ = false;
        restoreOriginalGate(operation);
    });
}

void RoiTestController::restoreOriginalGate(quint64 operation)
{
    if (operation != operation_ || !restoring_) return;
    if (!gateChanged_ || !originalGateValid_) {
        finishRestore(operation);
        return;
    }
    device_->collect()->setGateConfig(this, originalGate_,
        [this, operation](bool ok, const CollectGateConfig& updated, const QString& err) {
        if (operation != operation_ || !restoring_) return;
        if (!ok || !gateMatches(updated, originalGate_) || updated.pendingConfig) {
            markRestorePending(errorText(QString::fromUtf8("恢复采集门控配置失败"), err));
            return;
        }
        gateChanged_ = false;
        finishRestore(operation);
    });
}

void RoiTestController::finishRestore(quint64 operation)
{
    if (operation != operation_ || !restoring_) return;
    restoring_ = false;
    restorePending_ = false;
    originalRoiValid_ = false;
    originalGateValid_ = false;
    panel_->setBusy(false);
    panel_->setStatusText(completionText_, completionError_);
}

void RoiTestController::markRestorePending(const QString& message)
{
    restoring_ = false;
    restorePending_ = true;
    panel_->setBusy(false);
    panel_->setStatusText(message, true);
}

void RoiTestController::handleDisconnected()
{
    if (!testRunning_ && !restoring_) return;
    ++operation_;
    testRunning_ = false;
    restoring_ = false;
    awaitingFrame_ = false;
    frameTimeout_.stop();
    restorePending_ = collectionStarted_ || roiChanged_ || gateChanged_;
    panel_->setBusy(false);
    if (restorePending_) {
        completionText_ = QString::fromUtf8("设备重连后已停止采集并恢复测试前配置");
        completionError_ = false;
        panel_->setStatusText(QString::fromUtf8("设备连接中断；重连后将停止采集并恢复测试前配置"), true);
    } else {
        originalRoiValid_ = false;
        originalGateValid_ = false;
        panel_->setStatusText(QString::fromUtf8("设备连接中断"), true);
    }
}
