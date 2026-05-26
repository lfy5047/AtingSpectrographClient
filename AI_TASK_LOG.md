# AI 任务日志

> 最新记录放在最上方。后续 AI 变更项目时，请补充本文件。

## 2026-05-26 Raw/Slice 图像右下角坐标与统计显示

- 目标：为 Raw16 / SliceStitch16 图像页增加右下角浮层，显示鼠标悬停坐标 `x/y` 和当前通道最新完整帧的 `min/max/avg`，同时保留左下角 `WxH | zoom%` 现有信息。
- 文件变更：
  - `src/Ui/widgets/ImageView.h`
  - `src/Ui/widgets/ImageView.cpp`
  - `src/Ui/MainWindow.h`
  - `src/Ui/MainWindow.cpp`
  - `resources/style/industrial.qss`
- 实现要点：
  - 为 `ImageView` 增加鼠标坐标信号、`leaveEvent`、`showEvent` 与缩放/拖拽后的坐标同步，负责把 widget 坐标换算成图像像素坐标。
  - 将统计浮层从原先的底部工具条语义中拆出，独立挂在 `viewerContainer_` 上并定位到右下角。
  - 在 `frameReady` 路径中仅对 Raw16 / SliceStitch16 的 Mono16 帧计算原始像素统计，播放帧不参与这份缓存。
  - `avg` 使用 64 位累加后转为浮点显示，保留 1 位小数。
- 验证：
  - 已执行 `.\make.bat`，Release 构建通过。

## 2026-05-26 光谱图像改为 Tail 后统一渲染

- 目标：取消 Spectral 页的逐帧/逐列实时渲染，只在一帧完整光谱扫描由 `TailFrame` 收齐后再更新图像；同时保留参数切换时对最近完整图像的即时重画。
- 文件变更：
  - `src/Ui/SpectralScanBuilder.h`
  - `src/Ui/SpectralScanBuilder.cpp`
  - `src/Ui/MainWindow.cpp`
- 实现要点：
  - 将 `SpectralScanBuilder` 拆成“当前接收中的扫描”和“最近完整扫描”两套状态，`HeaderFrame` 只重置当前扫描，不清空已完成图像。
  - `TailFrame` 成功追加最后一列后，把当前扫描提交为可渲染图像，并让 `feedFrame(...)` 返回 `true`；`MainWindow` 仅在该时机标记 Spectral 视图为 dirty。
  - `render()`、范围平均缓存和 SpinBox 范围都改为以最近完整扫描为准；如果还没有完整图像，则临时回退到当前接收中的 band count 供预配范围使用。
  - `reset()` 仍保留为“完全清空”，用于回放启动等必须丢弃旧完成图的场景。
- 验证：
  - 运行 `.\make.bat`，构建通过并成功链接 `Release\AtingSpectrographClient.exe`。

## 2026-05-26 红外机芯控制面板补齐全部命令

- 目标：将 IrPanel 从约 10 条命令扩展到全部 ~41 条 IrService 命令，实现完整红外机芯控制界面。
- 文件变更：
  - `src/Ui/panels/IrPanel.h` — 新增 30+ 控件成员变量和 10 个 setup 方法声明
  - `src/Ui/panels/IrPanel.cpp` — 重构为 setup 方法模式，新增 7 个分组
- 分组设计：
  - 亮度/对比度/DDE/AB模式（保留+扩展）、积分时间（保留+扩展）、图像显示（新增）
  - 滤波（新增）、翻转与同步（新增）、模式控制（新增）
  - 查询与操作（保留+扩展）、维护与校正（新增，附风险提示）、坏元管理（新增）
  - 原始命令（保留不变）
- 验证：`cmake --build` Release 编译通过。

## 2026-05-26 升级至控制协议 v2 与新增 IR 命令

- 目标：根据最新 `doc/客户端命令接口.md` 将客户端代码同步到控制协议 v2，补齐新增通道和 IR 命令。
- 文件变更：
  - `src/Client/rpc/Protocol.h`：`kCtrlProtoVersion` 1→2；`StreamChannel` 枚举新增 `Preview8=2`、`RegionStitch16=4`
  - `src/Client/rpc/RpcCommands.h`：`Ir` 命名空间新增 30 条 v2 命令（版本、图像选择/显示、DDE/滤波、翻转/同步、积分档位、模式控制、维护校正、坏元管理）
  - `src/Client/services/IrService.h`：声明全部新增 IR 命令方法
  - `src/Client/services/IrService.cpp`：实现全部新增 IR 命令方法
  - `src/Ui/panels/StreamPanel.h` / `.cpp`：新增 Preview8 和 RegionStitch16 通道复选框及 `selectedChannels()` 逻辑
- 验证：`cmake --build` Release 编译通过。

## 2026-05-22 新增 Spectral 波段显示与来源选择

- 目标：新增 Spectral 主图像 Tab 与“光谱显示”面板，支持单波段、范围平均、RGB 合成，并可选择 Auto/Live/Playback 数据来源；打通 `frameType + streamFrameId` 在实时流、录制、回放链路中的透传，支持从回放帧重建 Spectral 扫描。
- 文件变更：
  - 新增 `src/Client/stream/StreamFrame.h`
  - 新增 `src/Ui/SpectralScanBuilder.*`
  - 新增 `src/Ui/panels/SpectralPanel.*`
  - 修改 `src/Client/stream/FrameAssembler.*`、`src/Client/stream/StreamClient.*`、`src/Client/core/DeviceClient.h`
  - 修改 `src/Client/recording/RecordedFrame.h`、`FrameRecorder*`、`FrameRecorderWriterWorker*`、`FramePlaybackController*`、`FramePlaybackScanWorker.cpp`
  - 修改 `src/Ui/MainWindow.*`、`src/Ui/widgets/SidebarWidget.*`、`src/main.cpp`
  - 修改 `PROJECT_CONTEXT.md`、`ARCHITECTURE.md`、`AI_TASK_LOG.md`
- 验证：多次运行 `.\make.bat` 构建通过（Release 链接成功）。
- 备注：
  - 实时 Raw/Slice 图像仍渲染到旧图像页，Spectral 另行消费 `HeaderFrame/DataFrame/TailFrame`。
  - 双通道订阅时，Spectral 构建器会学习同通道 `streamFrameId` 正常步长，避免把另一个通道的帧号间隔误判为缺列导致宽度翻倍。
  - `StreamClient` 维护按通道 FPS，顶部 FPS 在 Spectral 页按所选源通道显示。
  - 回放期间 Live Spectral 缓存继续更新；用户可在 Spectral 面板强制选择 Live 或 Playback。
  - 旧 `window/panel` 设置增加了索引迁移（旧值 7/8 对应新版 8/9）。

## 2026-05-22 实施 UDP v2 协议升级

- 目标：将客户端 UDP 流头从 32 字节升级到 v2 64 字节，拆分 TCP/UDP 协议版本常量，并增加 UDP 丢包限频日志。
- 文件变更：
  - `src/Client/rpc/Protocol.h`
  - `src/Client/rpc/ControlClient.cpp`
  - `src/Client/stream/StreamClient.h`
  - `src/Client/stream/StreamClient.cpp`
  - `AI_TASK_LOG.md`
- 验证：运行 `.\make.bat` 构建通过。
- 备注：`StreamHeader` 新增 `meta_flags` 与 RawFrame 元数据字段，保持前 32 字节关键偏移不变并新增 `offsetof` 断言；当前仅解析/校验元数据，不向 UI 和录制层透传。

## 2026-05-21 拆分 FramePlaybackController 扫描 Worker

- 目标：把 `FramePlaybackController` 内联 `ScanWorker` 拆出为独立头/源文件，去掉 `#include "FramePlaybackController.moc"`。
- 文件变更：
  - 修改 `src/Client/recording/FramePlaybackController.h`
  - 修改 `src/Client/recording/FramePlaybackController.cpp`
  - 新增 `src/Client/recording/FramePlaybackScanWorker.h`
  - 新增 `src/Client/recording/FramePlaybackScanWorker.cpp`
  - 修改 `AI_TASK_LOG.md`
- 验证：运行 `.\make.bat` 构建通过；确认不再包含 `FramePlaybackController.moc`。
- 备注：扫描索引、回放接口和损坏帧统计逻辑保持不变，仅做类定义位置调整。

## 2026-05-20 修复循环播放功能

- 目标：修复循环播放模式下，播放到文件末尾后再次点击播放无法正常工作的 bug。
- 文件变更：
  - `src/Client/recording/FramePlaybackController.cpp`
- 验证：未运行构建（构建环境不可用）；逻辑审查确认修复正确。
- 备注：根因是 `play()` 中 `windowStartIndex_ = currentIndex_` 未做越界检查。当上一次播放走到文件末尾（非循环模式）后，`currentIndex_` 停留在 `index_.size()` 位置，再次点击播放时 `windowStartIndex_` 被设为非法值，导致循环播放陷入空转（循环模式）或立即停止（非循环模式）。修复方案：`play()` 开头若 `currentIndex_ >= index_.size()` 则重置为 0。

## 2026-05-20 补充项目上下文与架构文档
- 目标：补齐前序录制/回放、`.asrec` 文件格式、Playback 视图、录制 worker 拆分等未同步到项目文档的内容。
- 文件变更：
  - `PROJECT_CONTEXT.md`
  - `ARCHITECTURE.md`
  - `AI_TASK_LOG.md`
- 验证：文档变更未运行构建；已检查文档中不再保留“设备数据流不持久化”等过时描述。
- 备注：补充了 `FrameRecorderWriterWorker`、`FramePlaybackController`、`RecordPlaybackPanel`、QSettings 录制/回放配置和 `CMAKE_AUTOMOC` 相关说明。

## 2026-05-20 拆分 FrameRecorder MOC 实现
- 目标：将 `FrameRecorder` 中带 `Q_OBJECT` 的写入工作类从 cpp 内联定义拆出，使 `FrameRecorder.cpp` 不再 `#include "FrameRecorder.moc"`。
- 文件变更：
  - 修改 `src/Client/recording/FrameRecorder.h`
  - 修改 `src/Client/recording/FrameRecorder.cpp`
  - 新增 `src/Client/recording/FrameRecorderWriterWorker.h`
  - 新增 `src/Client/recording/FrameRecorderWriterWorker.cpp`
- 验证：运行 `.\make.bat` 构建通过；确认源码中无 `FrameRecorder.moc` 引用，CMake `AUTOMOC` 已生成 `moc_FrameRecorderWriterWorker.cpp`。
- 备注：保持原有录制队列、写入、停止和统计信号逻辑不变，仅调整类定义位置与 include 关系。

## 2026-05-20 修复录制文件播放帧头定位
- 目标：修复回放录制文件时无画面，并连续报 `bad frame magic` 的问题。
- 文件变更：
  - `src/Client/recording/FramePlaybackController.h`
  - `src/Client/recording/FramePlaybackController.cpp`
- 验证：运行 `.\make.bat d` 构建通过；运行 `.\make.bat` 时 Release 链接失败，原因是 `Release\AtingSpectrographClient.exe` 被占用，编译阶段已通过。
- 备注：帧头字段实际序列化长度与声明的 `kAsrecFrameHeaderSize` 不一致，回放改为使用扫描阶段记录的真实帧头偏移，兼容已录制文件。

## 2026-05-20 增加图像数据录制与回放
- 目标：新增完整原始帧录制（.asrec）与回放（Playback ImageView），支持 FPS、播放帧数、循环播放，开始回放自动切换到 Playback。
- 文件变更：
  - 新增 `src/Client/recording/RecordingFileFormat.*`、`src/Client/recording/RecordedFrame.h`
  - 新增 `src/Client/recording/FrameRecorder.*`
  - 新增 `src/Client/recording/FramePlaybackController.*`
  - 新增 `src/Ui/panels/RecordPlaybackPanel.*`
  - 修改 `src/Ui/widgets/SidebarWidget.cpp`
  - 修改 `src/Ui/MainWindow.*`
- 验证：运行 `.\make.bat` 构建通过。
- 备注：终端显示可能乱码，未做全局编码修复以避免引入额外风险。

## 2026-05-20 创建项目上下文文档

- 目标：扫描项目结构，生成后续 AI 会话可复用的项目上下文、架构和操作规则。
- 文件变更：
  - `PROJECT_CONTEXT.md`
  - `ARCHITECTURE.md`
  - `AI_TASK_LOG.md`
  - `.codex/instructions.md`
- 验证：通过文件路径检查确认文档引用的关键源码、脚本和配置存在；未运行构建，因为本次仅新增文档。
- 备注：创建文档前工作区已有未提交业务改动，涉及相机/设备/RPC/UI 文件；本任务未修改这些业务文件。

## 任务记录模板

### YYYY-MM-DD 任务标题

- 目标：
- 文件变更：
- 验证：
- 备注/后续风险：
## 2026-05-22 Spectral range render performance optimization

- Goal: reduce UI freeze when selecting large spectral band ranges.
- Files changed:
  - `src/Ui/SpectralScanBuilder.h`
  - `src/Ui/SpectralScanBuilder.cpp`
  - `src/Ui/MainWindow.cpp`
  - `AI_TASK_LOG.md`
- Implementation:
  - Added `columnVersion_` and `mutable RangeAverageCache` in `SpectralScanBuilder`.
  - Incremented version on normal append, reset, and each gap-fill append.
  - Reworked non-RGB render path to incremental column cache + per-frame min/max rescale.
  - Kept `render(...) const` unchanged and updated cache inside mutable members.
  - Gated spectral timer render by `currentChannel_ == 2`.
  - Removed synchronous `updateSpectralView()` on `SpectralPanel::settingsChanged`.
  - Refreshed spectral source/stats when switching back to Spectral tab.
  - Limited per-frame `refreshSpectralStats()` to visible and source/channel-matched cases.
- Verification:
  - Ran `.\make.bat`, build passed and linked `Release\AtingSpectrographClient.exe`.
