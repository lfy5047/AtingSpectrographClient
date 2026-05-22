# AI 任务日志

> 最新记录放在最上方。后续 AI 变更项目时，请补充本文件。

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
