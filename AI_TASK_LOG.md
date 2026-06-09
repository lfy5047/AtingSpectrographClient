# AI 任务日志
## 2026-06-09 软件图标接入

- 目标：为客户端增加合适的软件图标，并同时覆盖 Qt 运行时窗口图标和 Windows exe 图标资源。
- 文件变更：
  - `resources/icons/app_icon.png`
  - `resources/icons/app_icon.ico`
  - `resources/resources.qrc`
  - `cmake/AtingSpectrographClient.rc.in`
  - `src/main.cpp`
  - `docs/superpowers/specs/2026-06-09-app-icon-design.md`
  - `docs/superpowers/plans/2026-06-09-app-icon.md`
- 实现要点：
  - 使用 Image 2 生成“仪器光路”方向图标：深色光谱仪/相机设备、中心镜头和斜向彩色光谱束。
  - 将源图保存为 Qt 资源 `:/icons/app_icon.png`，并在 `QApplication` 初始化时设置为应用窗口图标。
  - 生成多尺寸 `app_icon.ico`，并通过 CMake 配置生成的 Windows `.rc` 嵌入 exe 图标。
- 验证：
  - 已运行 `.\make.bat`，Debug 构建通过，Qt qrc 和 Windows rc 图标资源均参与构建。
  - 构建仅保留既有 qcustomplot/Qt 相关 MSVC STL deprecation warning。

## 2026-06-09 软件版本号管理

- 目标：为客户端建立统一的软件版本号管理，避免运行时、安装包和 exe 元数据各自维护版本号。
- 文件变更：
  - `CMakeLists.txt`
  - `cmake/AppVersion.h.in`
  - `cmake/AtingSpectrographClient.rc.in`
  - `cmake/setup.iss.in`
  - `src/main.cpp`
  - `src/Ui/MainWindow.cpp`
  - `tests/AppVersionTest.cpp`
  - `package.bat`
  - `setup.iss`
- 实现要点：
  - 顶层 `project(AtingSpectrographClient VERSION 0.1.0 ...)` 作为版本号单一入口。
  - CMake 生成 `AppVersion.h`，供 `QApplication` 版本信息和主窗口标题使用。
  - CMake 生成 Windows `.rc` 资源，写入 exe 的 FileVersion/ProductVersion。
  - CMake 生成 `build\setup.iss`，`package.bat` 使用该脚本打包，安装包版本和文件名跟随 CMake 版本。
- 验证：
  - 已先新增 `AppVersionTest` 并观察旧实现下因缺少 `AppVersion.h` 构建失败。
  - 已运行 `.\make.bat`，Debug 构建通过，Windows `.rc` 资源被编译进主程序。
  - 已运行 `ctest --test-dir build -C Debug --output-on-failure`，2 个测试通过。
  - 已检查 `build\generated\AppVersion.h`、`build\setup.iss` 和 `build\Debug\AtingSpectrographClient.exe` 元数据，版本均为 `0.1.0`。

## 2026-06-09 spectral_preview 流通道接入

- 目标：按新版 UDP v3 协议接入服务端 `spectral_preview` 流通道，并作为独立 JPEG 预览页显示。
- 文件变更：
  - `CMakeLists.txt`
  - `tests/ImageFrameUtilsTest.cpp`
  - `src/Client/rpc/Protocol.h`
  - `src/Client/stream/StreamClient.cpp`
  - `src/Ui/ImageFrameUtils.cpp`
  - `src/Ui/DeviceUiCoordinator.cpp`
  - `src/Ui/MainWindowPanelRegistry.cpp`
  - `src/Ui/widgets/ViewerAreaWidget.*`
  - `src/Ui/panels/StreamPanel.*`
  - `src/Ui/panels/DashboardPanel.cpp`
- 实现要点：
  - 新增 `SpectralPreview = 5` 和 `Jpeg = 3` 协议枚举，允许 UDP 通道 1~5。
  - `ImageFrameUtils` 对 JPEG payload 使用 Qt 图像解码，并校验解码尺寸与流头一致。
  - Stream 面板新增 `SpectralPreview` 订阅勾选项，订阅名为 `spectral_preview`。
  - Viewer 新增独立 `SpectralPreview` 页，服务端 JPEG 预览不进入客户端本地 Spectral 扫描重建流程。
- 验证：
  - 已先新增 `ImageFrameUtilsTest` 并观察 JPEG 解码测试在旧实现下失败。
  - 已运行 `.\make.bat`，Debug 构建通过。
  - 已运行 `ctest --test-dir build -C Debug -R ImageFrameUtilsTest --output-on-failure`，新增测试通过。

## 2026-05-29 远程录制数据回放改造

- 目标：移除本地 `.asrec` 录制/回放入口，改为通过服务端 `record.*` 查询、下载并播放 raw/tif 历史数据。
- 文件变更：
  - `src/Client/rpc/RpcCommands.h`
  - `src/Client/services/RecordService.*`
  - `src/Client/core/DeviceClient.*`
  - `src/Client/recording/RemoteFileDownloader.*`
  - `src/Ui/panels/RecordPlaybackPanel.*`
  - `src/Ui/DeviceUiCoordinator.*`
  - `src/Ui/widgets/ViewerAreaWidget.*`
  - `src/Ui/MainWindow*.cpp`
  - `libs/libsdefine.cmake`
  - `CMakeLists.txt`
  - `.gitignore`
  - `ARCHITECTURE.md`
  - `PROJECT_CONTEXT.md`
  - `AI_TASK_LOG.md`
- 实现要点：
  - 新增 `RecordService` 封装 `record.get_retention`、`record.set_retention`、`record.list_recent`、`record.fetch`。
  - 新增异步 `RemoteFileDownloader`，按 `record.fetch.files` 校验文件名、大小、offset 和 LastChunk/LastFile，并写入 `recordings/remote_cache/<type>/<record_id>/`。
  - 重写 `RecordPlaybackPanel`，支持保留时间、远程列表、多选下载、缓存复用、raw 逐帧播放和 BigTIFF 按参数渲染。
  - `DeviceUiCoordinator` 断开旧录制写入与 Playback Spectral 缓存路径，远程回放只渲染到 Playback 视图。
  - CMake 链接 `tiff.lib` 并把 `tiff.dll` 复制到构建输出目录。
- 验证：
  - 已运行 `.\make.bat`，Release 构建通过。
  - 已运行 `.\make.bat d`，Debug 构建通过。
  - 后续仍需结合真实服务端验证 record.* RPC、文件 TCP 下载、raw/tif 实际播放和异常传输场景。

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
