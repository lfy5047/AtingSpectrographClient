# AI 任务日志
## 2026-05-28 MainWindow 拆分文档同步

- 目标：同步项目上下文和架构文档，反映 `MainWindow` 拆分后的模块边界和数据流。
- 文件变更：
  - `PROJECT_CONTEXT.md`
  - `ARCHITECTURE.md`
  - `AI_TASK_LOG.md`
- 实现要点：
  - 更新关键目录、首读文件和主运行流程，加入 `MainWindowChrome`、`MainWindowPanelRegistry`、`DeviceUiCoordinator`、`WindowSettingsStore` 等新入口。
  - 更新图像流、Spectral、光谱分析、录制回放的数据流责任方。
  - 更新常见修改入口，避免继续指向已拆走职责的 `MainWindow` 方法。
- 验证：
  - 文档变更，未运行构建。

## 2026-05-28 MainWindow 第二轮组合根拆分

- 目标：继续拆分 `MainWindow`，把 UI 骨架、Panel 注册、设备信号协调和窗口设置迁移到独立模块。
- 文件变更：
  - `src/Ui/MainWindow.*`
  - `src/Ui/MainWindowChrome.*`
  - `src/Ui/MainWindowPanelRegistry.*`
  - `src/Ui/DeviceUiCoordinator.*`
  - `src/Ui/WindowSettingsStore.*`
- 实现要点：
  - `MainWindowChrome` 接管主窗口静态布局和顶层控件访问。
  - `MainWindowPanelRegistry` 接管业务 Panel 创建、切换、日志 toggle 和光谱分析标题点击。
  - `DeviceUiCoordinator` 接管设备连接、录制回放、Spectral 刷新、stream stats、raw log 和 uptime。
  - `WindowSettingsStore` 集中窗口设置恢复、保存和 Panel index 迁移。
  - `MainWindow.cpp` 从 552 行进一步缩减到 59 行，仅保留初始化顺序和关闭流程。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-28 MainWindow 渐进瘦身重构

- 目标：拆分 `MainWindow` 中的图像显示、Spectral 扫描状态和光谱分析协调逻辑，保留主窗口作为组合根。
- 文件变更：
  - `src/Ui/MainWindow.*`
  - `src/Ui/ImageFrameUtils.*`
  - `src/Ui/SpectralScanController.*`
  - `src/Ui/SpectrumAnalysisCoordinator.*`
  - `src/Ui/widgets/ViewerAreaWidget.*`
- 实现要点：
  - 新增图像转换/统计工具，集中 Mono8/Mono16 显示图和 Mono16 统计计算。
  - 新增 `ViewerAreaWidget` 管理通道 tab、四个 `ImageView`、图像统计 overlay 和 Spectral progress overlay。
  - 新增 `SpectralScanController` 管理 Live/Playback 光谱扫描缓存、进度状态和渲染入口。
  - 新增 `SpectrumAnalysisCoordinator` 管理 SliceStitch16 最新帧缓存、采样线 overlay、曲线窗口和曲线刷新。
  - `MainWindow.cpp` 从 1234 行缩减到 552 行，主要保留窗口装配、信号分发和状态同步。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-28 项目上下文与架构文档更新

- 目标：刷新项目级交接文档，补齐近期光谱分析、曲线处理和 Panel 索引变化。
- 文件变更：
  - `PROJECT_CONTEXT.md`
  - `ARCHITECTURE.md`
  - `AI_TASK_LOG.md`
- 实现要点：
  - 重写项目上下文文档，补充技术栈、首读文件、光谱分析功能、配置持久化和 AI 交接规则。
  - 重写架构说明文档，补充 SliceStitch16 光谱分析数据流、QCustomPlot 集成、Panel index v3 和常见修改入口。
- 验证：
  - 文档变更，未运行构建。

## 2026-05-28 光谱曲线 Y 轴锚定与最小跨度

- 目标：让光谱曲线的最小值固定在 Y 轴指定百分比位置，并避免小幅噪声被过度放大。
- 文件变更：
  - `src/Ui/panels/SpectrumAnalysisPanel.*`
  - `src/Ui/MainWindow.cpp`
  - `src/Ui/SpectrumCurveDialog.*`
- 实现要点：
  - 光谱分析 Panel 新增最小值位置百分比和最小数据跨度参数，并持久化到 `spectrumAnalysis/`。
  - 曲线窗口按所有实际绘制点计算全局 `minY/maxY`，小于最小跨度时使用固定跨度，大于等于阈值时再应用 `yRangeMultiplier`。
  - Y 轴下限按最小值位置百分比锚定，空数据仍使用 `0-65535` 安全范围。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-27 光谱曲线滤波抽点与 Y 轴倍率

- 目标：降低光谱曲线噪声观感并改善高刷新率下的绘图性能。
- 文件变更：
  - `src/Ui/panels/SpectrumAnalysisPanel.*`
  - `src/Ui/MainWindow.cpp`
  - `src/Ui/SpectrumCurveDialog.*`
- 实现要点：
  - 光谱分析 Panel 新增移动平均窗口、每条曲线最大绘制点数、Y 轴范围倍率，并持久化到 `spectrumAnalysis/`。
  - 曲线数据按最大点数做等间距抽点，首尾点保留；每个绘制点使用原始 Mono16 行数据做居中移动平均。
  - 曲线窗口按实际绘制数据计算全局 Y 范围，并使用用户设置倍率放大，空数据时保持安全默认范围。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-27 SliceStitch16 光谱刻度显示优化

- 目标：SliceStitch16 光谱分析图像页不再绘制网格线，只保留坐标刻度，并增强刻度数字可读性。
- 文件变更：
  - `src/Ui/widgets/ImageView.cpp`
- 实现要点：
  - 移除分析 overlay 的贯穿式横/竖网格线。
  - Y 轴刻度数字改为绘制在图像左侧，刻度文字使用亮色并增加深色阴影。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-27 光谱曲线 Y 轴范围修正

- 目标：光谱强度曲线窗口的 Y 轴按所有曲线数据的全局 `Y(max)-Y(min)` 计算，并留 10% 范围余量。
- 文件变更：
  - `src/Ui/SpectrumCurveDialog.cpp`
- 实现要点：
  - 曲线刷新时扫描全部采样线的 DN 数据，使用全局最小/最大值计算 Y 轴范围。
  - X 轴继续自动缩放，Y 轴不再强制下限包含 0。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

## 2026-05-27 光谱强度曲线分析

- 目标：新增只针对 SliceStitch16 的光谱分析 Panel、图像水平采样线和独立光谱强度曲线窗口。
- 文件变更：
  - `src/Ui/MainWindow.h`
  - `src/Ui/MainWindow.cpp`
  - `src/Ui/widgets/ImageView.h`
  - `src/Ui/widgets/ImageView.cpp`
  - `src/Ui/widgets/SidebarWidget.cpp`
  - `src/Ui/panels/SpectrumAnalysisPanel.*`
  - `src/Ui/SpectrumCurveDialog.*`
  - `src/Ui/SpectrumAnalysisTypes.h`
  - `src/Ui/widgets/qcustomplot.*`
- 实现要点：
  - 新增光谱分析导航项和 Panel index v3 迁移，系统日志后移为特殊 index 10。
  - 在 SliceStitch16 图像页启用坐标刻度、点击添加线、拖动改 y、右键删除线，并把采样线状态持久化。
  - 缓存最新 SliceStitch16 Mono16 原始帧，按线性波长映射和刷新率绘制多条 DN 强度曲线。
  - 复制 QCustomPlot 到 `src/Ui/widgets`，避免链接 `LineProfileReusable` 和 C++ 标准冲突。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。

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
