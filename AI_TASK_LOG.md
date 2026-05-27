# AI 任务日志
## 2026-05-26 Spectral 等待进度条

- 目标：在 Spectral 图像等待整帧完整数据期间显示 10s 进度条，按当前选中的 Spectral 源/通道单独跟踪 Header 到 Tail 的等待状态。
- 文件变更：
  - `src/Ui/MainWindow.h`
  - `src/Ui/MainWindow.cpp`
  - `resources/style/industrial.qss`
- 实现要点：
  - 为 Live/Playback × Raw16/SliceStitch16 维护独立等待状态，Header 开始计时，Tail 完整提交后停止并隐藏浮层。
  - 在 `feedFrame()` 中通过 `wasActive` / `hasActiveScan()` / `committed` 判定扫描中断，几何不匹配或 reset 路径都会清理对应进度。
  - 在 Spectral 主图像上方增加透明浮层进度条，样式沿用当前深色主题，100ms 定时刷新到 10s 封顶。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

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
