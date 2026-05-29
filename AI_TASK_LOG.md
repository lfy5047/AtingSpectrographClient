# AI 任务日志
## 2026-05-29 IR 命令语义化响应适配

- 目标：按新版客户端命令接口适配 IR 机芯命令响应，并移除客户端 `ir.send_raw` 透传调试入口。
- 文件变更：
  - `src/Client/rpc/RpcCommands.h`
  - `src/Client/services/IrService.*`
  - `src/Ui/panels/IrPanel.*`
  - `ARCHITECTURE.md`
  - `AI_TASK_LOG.md`
- 实现要点：
  - 从客户端 RPC 常量、service API 和 IR 面板中删除 `ir.send_raw` 入口。
  - IR 查询结果改为显示新版 `data.value`，支持数值、字符串、布尔、数组和对象。
  - `saveCalibParams` 与 `maintenanceExec` 改为 JSON 回调，成功时可在共享结果标签中显示非致命 `warning`。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。
  - 已运行 `.\make.bat d`，Debug 构建通过；仅保留既有 qcustomplot/Qt 相关 STL deprecation warning。
  - 已静态搜索确认客户端源码中不再引用 `ir.send_raw` / `sendRaw` / `setupRawCmd` / raw 命令控件。

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
