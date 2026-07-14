# AtingSpectrographClient 项目上下文

## 项目概览

AtingSpectrographClient 是一个 Windows 桌面端光谱仪/成像设备控制客户端。主界面使用 Qt Widgets 构建，通过 TCP JSON-RPC 控制设备，通过 UDP 接收 Raw16、SliceStitch16 和 Spectral 图像流，并通过服务端 `record.*` 接口查询、下载和回放历史 raw/tif 录制数据。

主要运行目标：
- Windows 桌面应用：`AtingSpectrographClient.exe`
- 控制连接：TCP，默认设备 `192.168.10.128:9000`
- 图像流接收：本地 UDP，默认端口 `1400`
- 主要图像页：Raw16、SliceStitch16、Spectral、SpectralPreview、Playback、Binning 对比、ROI 对比

## 技术栈

- C++11
- CMake + Ninja Multi-Config
- Qt 5.15.2：`Widgets`、`PrintSupport`、`Network`
- `CMAKE_AUTOMOC` 和 `CMAKE_AUTORCC` 已启用
- MSVC 编译参数包含 `/utf-8`
- 第三方依赖主要位于 `libs/public`
  - `nlohmann/json`：RPC JSON 负载
  - `plog`：文件和控制台日志
  - `fmt`：header-only 引入
  - `boost/asio`：公共依赖，可用但当前客户端主代码未直接使用
  - OpenCV 4.5.5、libtiff：依赖目录已配置
- `src/Ui/widgets/qcustomplot.*` 是当前主项目直接编译的 QCustomPlot 副本，用于光谱曲线窗口。

## 构建、运行、清理

仓库规则要求在根目录构建：

```bat
.\make.bat
```

相关脚本：
- `config.bat`：配置项目根目录、构建目录、CMake、Qt、Visual Studio DevCmd 路径。
- `make.bat`：调用 `config.bat`，默认构建 Release；传入 `d` 时构建 Debug；使用 `Ninja Multi-Config`。
- `install.bat`：运行 `cmake --install` 到 `build\Release`。
- `run.bat`：先构建/必要时安装，然后结束同名进程并运行 exe。
- `clear.bat`：清理脚本，使用前先确认删除范围。
- `package.bat`：使用 CMake 生成的 `build\setup.iss` 打包，安装包版本跟随 Git tag 派生版本。

自动化测试通过 CTest 运行，常用命令为 `ctest --test-dir build -C Debug --output-on-failure`。业务代码变更建议至少运行 `.\make.bat`；纯文档变更通常不需要构建。

## 版本管理

- 软件发布版本的唯一入口是 Git tag，推荐格式为 `vX.Y.Z`，例如 `v0.1.1`。
- CMake 配置阶段通过 `git describe --tags --dirty --always --long` 查找最近的发布 tag，并生成 `AppVersion.h`、Windows `.rc` 资源和 `build\setup.iss`。
- 显示版本始终只使用最近发布 tag 的 `X.Y.Z`；tag 后提交和 dirty 状态不追加到用户可见版本号。
- 运行时 `QApplication::applicationVersion()`、主窗口标题、exe FileVersion/ProductVersion、安装包 `AppVersion` 和安装包文件名都应从 Git 派生版本生成。

## 关键目录

- `src/main.cpp`：应用入口，初始化 plog、`QApplication`、全局字体和应用主题。
- `src/Ui/MainWindow.*`：主窗口组合根，只负责创建顶层协作者、恢复窗口状态和关闭生命周期。
- `src/Ui/MainWindowChrome.*`：主窗口静态骨架，创建侧边栏、顶部栏、图像区、右侧 Panel 容器和底部日志。
- `src/Ui/MainWindowPanelRegistry.*`：右侧业务 Panel 的创建、注册、切换、标题点击和 Panel index 管理。
- `src/Ui/DeviceUiCoordinator.*`：设备信号与 UI 的绑定层，负责连接状态、帧分发、录制回放、Spectral 刷新、stream stats、raw log 和 uptime。
- `src/Ui/BinningTestController.*`：Binning 测试状态机，负责保存/恢复配置、依次设置 1x1/2x2/4x4、回读确认，并从测试面板所选 Raw16/SliceStitch16 数据源采集稳定帧；默认数据源为 Raw16。
- `src/Ui/RoiTestController.*`：ROI 测试状态机，负责校验 Binning 1x1、启用静态采集、忽略 Header 并采集全幅/ROI 后续数据图像，在完成、取消、失败或重连后恢复 ROI 和采集门控配置。
- `src/Ui/ThemeManager.*`：应用级主题目录、QSS 加载和 `QSettings` 主题偏好保存。
- `src/Ui/WindowSettingsStore.*`：窗口 geometry、splitter、当前 Panel、侧边栏折叠状态和 Panel index 迁移。
- `src/Ui/ImageFrameUtils.*`：Mono8/Mono16 图像显示转换和 Mono16 图像统计。
- `src/Ui/SpectralScanController.*`：Live/Playback Spectral 扫描缓存、进度状态和渲染入口。
- `src/Ui/SpectrumAnalysisCoordinator.*`：SliceStitch16 光谱分析协调，管理最新帧缓存、采样线 overlay、曲线窗口和曲线刷新。
- `src/Ui/panels/`：右侧业务面板，包括连接、相机、转镜、红外、采集、流、Spectral、录制回放、光谱分析等。
- `src/Ui/widgets/`：通用 UI 部件，包括侧边栏、顶部栏、图像视图、ViewerArea、QCustomPlot 副本等。
- `src/Client/core/`：`DeviceClient` 聚合控制连接、流连接和各业务 service。
- `src/Client/rpc/`：TCP 控制协议、JSON-RPC 请求/响应封装、命令名定义。
- `src/Client/services/`：面向 UI 的业务 API 封装，把按钮行为转换为 RPC 命令。
- `src/Client/stream/`：UDP 图像流接收、分片重组、`StreamFrame` 元数据透传、FPS/丢帧统计。
- `src/Client/recording/`：远程文件 TCP 下载器，以及保留的旧 `.asrec` 录制/回放实现文件；当前 UI 已断开旧 `.asrec` 路径。
- `resources/`：Qt 资源文件和 `resources/style/industrial.qss`、`resources/style/outdoor_light.qss` 主题。
- `Common/`：通用工具库源码；当前顶层 CMake 只包含 `Common/include`，没有链接 `Common` 子库。
- `libs/public/`：随仓库提交的第三方依赖，默认不要全量扫描。

## 首读文件

新会话优先读这些文件即可快速建立上下文：

1. `PROJECT_CONTEXT.md`
2. `ARCHITECTURE.md`
3. `CMakeLists.txt`
4. `src/main.cpp`
5. `src/Ui/MainWindow.cpp`
6. `src/Ui/MainWindow.h`
7. `src/Ui/MainWindowChrome.cpp`
8. `src/Ui/MainWindowPanelRegistry.cpp`
9. `src/Ui/DeviceUiCoordinator.cpp`
10. `src/Client/core/DeviceClient.cpp`
11. `src/Client/rpc/Protocol.h`
12. `src/Client/rpc/ControlClient.cpp`
13. `src/Client/stream/StreamClient.cpp`
14. `src/Client/stream/FrameAssembler.cpp`
15. `src/Client/recording/RecordingFileFormat.h`
16. `src/Ui/SpectralScanController.cpp`
17. `src/Ui/SpectralScanBuilder.cpp`
18. `src/Ui/SpectrumAnalysisCoordinator.cpp`
19. `src/Ui/panels/SpectralPanel.cpp`
20. `src/Ui/panels/RecordPlaybackPanel.cpp`
21. `src/Ui/panels/SpectrumAnalysisPanel.cpp`
22. `src/Ui/SpectrumCurveDialog.cpp`

## 主运行流程

1. `main()` 初始化日志、Qt 应用信息、全局字体，并通过 `ThemeManager` 恢复已保存的应用主题。
2. `MainWindow` 创建 `DeviceClient`、`MainWindowChrome`、`MainWindowPanelRegistry`、`SpectrumAnalysisCoordinator`、`BinningTestController`、`RoiTestController` 和 `DeviceUiCoordinator`，再由 `WindowSettingsStore` 恢复窗口状态。
3. `ConnectionPanel` 调用 `DeviceClient::connectTo()`，由 `ControlClient` 建立 TCP 连接。
4. TCP 连接成功后，`DeviceUiCoordinator` 使用连接面板中的 UDP 端口绑定 `StreamClient`，并查询系统版本。
5. 各控制 Panel 通过 `DeviceClient` 下的 service 调用 RPC 命令。
6. `ControlClient` 发送 `CtrlHeader + JSON`，按 seq 管理 pending 回调、超时和断线清理。
7. UDP 包进入 `StreamClient`，`FrameAssembler` 按通道和 frame id 重组完整帧并发出 `StreamFrame`。
8. Raw16/SliceStitch16 的 Mono8/Mono16 数据经 `ImageFrameUtils` 转换为 8-bit `QImage`，由 `ViewerAreaWidget` 显示并更新图像统计 overlay。
9. `HeaderFrame/DataFrame/TailFrame` 同时进入 `SpectralScanController` 管理的 `SpectralScanBuilder`，Spectral 页按单波段、范围平均或 RGB 合成渲染。
10. `RecordPlaybackPanel` 通过 `record.list_recent` 查询服务端历史数据，通过 `record.fetch` 和独立文件 TCP 端口下载 raw/tif 到 `recordings/remote_cache/`。
11. 远程 raw 按 `.json + .raw` 逐帧读取并渲染到 Playback 视图；远程 tif 使用 libtiff 读取 BigTIFF 并按面板内参数渲染为一张投影图。
12. 光谱分析 Panel 激活时由 `MainWindowPanelRegistry` 自动切到 SliceStitch16 页；`SpectrumAnalysisCoordinator` 打开独立曲线窗口，并从最新 SliceStitch16 Mono16 原始帧中采样曲线。
13. Binning 测试 Panel 激活时自动切到三图对比页；数据源默认 Raw16、可切换 SliceStitch16；`BinningTestController` 保存原配置，依次采集 1x1、2x2、4x4 快照并在结束、取消或失败后恢复原配置。
14. ROI 测试 Panel 激活时自动切到双图对比页；`RoiTestController` 保存原 ROI 和采集门控配置，一键启动静态采集，忽略 Header 并依次抓取全幅与目标 ROI 的 SliceStitch16 后续数据图像，随后停止采集并恢复配置。

## 光谱分析功能

当前光谱分析只针对 SliceStitch16：
- 侧边栏 Panel index：光谱分析为 `6`、光谱段测试为 `8`、Binning 测试为 `9`、ROI 测试为 `10`，系统日志为特殊 index `11`；窗口状态版本为 `panelVersion = 8`。
- `SpectrumAnalysisCoordinator` 缓存最新一帧 SliceStitch16 Mono16 原始数据、宽高和 `streamFrameId`。
- `ViewerAreaWidget` 承载 SliceStitch16 图像页；底层 `ImageView` 支持亮色坐标刻度、水平采样线显示、点击添加线、拖动改 y、右键删除线。
- `SpectrumAnalysisPanel` 管理波长映射、采样线列表、刷新率、滤波窗口、最大绘制点数、Y 轴倍率、Y 轴最小值位置和最小数据跨度。
- `SpectrumCurveDialog` 是独立 QDialog，使用 QCustomPlot 绘制曲线，并保存窗口 geometry。
- 曲线 X 轴是按 `xStart..xEnd` 线性映射后的波长，Y 轴是对应行的原始 16-bit DN 值。
- 曲线数据先按最大点数等间距抽点，再对每个绘制点做居中移动平均。
- Y 轴按所有实际绘制曲线的全局 `minY/maxY` 计算；当数据跨度小于阈值时使用固定最小跨度，超过阈值时使用 `dataDiff * yRangeMultiplier`，并把数据最小值锚定到可设置百分比位置。

## 协议与集成点

控制和流协议位于 `src/Client/rpc/Protocol.h`：
- 控制 magic：`0x4E495441`，注释为 `'ATIN' LE`
- 流 magic：`0x4D545341`，注释为 `'ASTM' LE`
- 控制协议版本：`1`
- UDP 流协议版本：`2`
- 控制头：`CtrlHeader`，16 字节
- 流头：`StreamHeader`，64 字节
- 控制消息类型：`Request`、`Response`、`Event`、`Error`
- 图像通道：`Raw16 = 1`，`SliceStitch16 = 3`
- 像素格式：`Mono8 = 1`，`Mono16 = 2`
- 光谱帧类型：`UnknownFrame = 0`、`HeaderFrame = 1`、`DataFrame = 2`、`TailFrame = 3`

RPC 命令名集中在 `src/Client/rpc/RpcCommands.h`，当前分组包括：
- `system.*`
- `stream.*`
- `mirror.*`
- `camera.*`
- `ir.*`
- `collect.*`
- `binning.*`
- `roi.*`
- `record.*`

## 配置与持久化

- UI 状态通过 `WindowSettingsStore` 和 `QSettings` 保存：窗口 geometry、splitter、当前 Panel、侧边栏折叠状态等；应用主题由 `ThemeManager` 使用 `ui/theme` 保存。
- 光谱分析使用 `spectrumAnalysis/` 前缀保存参数、采样线、曲线窗口 geometry。
- 远程录制数据缓存默认写入 `recordings/remote_cache/<type>/<record_id>/`；成功缓存可复用，失败/取消/断连会删除本次不完整缓存。
- 应用组织与名称：`AtingSpectrograph` / `AtingSpectrographClient`。
- 运行时日志写入 `log/log.txt`，最大 1 MiB，保留 10 个滚动文件。
- 客户端不再写入本地 `.asrec` 录制文件。
- Qt 资源入口：`resources/resources.qrc`，当前打包应用图标、深色主题 `resources/style/industrial.qss` 和户外亮色主题 `resources/style/outdoor_light.qss`。

## 当前状态观察

- 源码中部分中文字符串在当前终端可能显示为乱码，但代码和文档应按 UTF-8 处理；不要为“修乱码”盲目转换编码。
- 顶层 CMake 使用 `file(GLOB_RECURSE)` 收集 `src/Client` 和 `src/Ui`，新源码通常会被纳入构建；CMake 缓存场景下仍建议重新配置。
- `Common/CMakeLists.txt` 定义了 `Common` 库，但顶层 CMake 当前没有 `add_subdirectory(Common)`，主目标也未链接该库。
- `libs/public` 很大且多为第三方源码/头文件，除非依赖升级或编译问题，不要默认扫描。
- 工作区可能长期存在未提交业务改动与生成目录；开始新任务前用 `git status --short` 区分已有改动和本次改动。
- libtiff 运行时依赖当前为 `libs/public/libtiff/lib/tiff.lib` 和 `tiff.dll`；CMake 会把 `tiff.dll` 复制到目标输出目录。

## AI 交接规则

- 先读本文件和 `ARCHITECTURE.md`，再按任务定位具体源码。
- 默认避免扫描 `libs/public/**`、`build/**`、`.cache/**`、`.git/**`。
- 修改业务逻辑后优先运行 `.\make.bat`；只改文档时可不构建。
- 不要回滚用户已有未提交改动。
