# AtingSpectrographClient 架构说明

## 启动路径

1. `src/main.cpp` 初始化 plog、Qt 应用、全局字体，并通过 `ThemeManager` 加载已保存的应用主题。
2. 创建 `MainWindow` 并进入 `QApplication::exec()`。
3. `MainWindow` 构造 `DeviceClient`、`MainWindowChrome`、`MainWindowPanelRegistry`、`SpectrumAnalysisCoordinator` 和 `DeviceUiCoordinator`。
4. `WindowSettingsStore` 恢复窗口 geometry、splitter、当前 Panel 和侧边栏折叠状态，`DeviceUiCoordinator` 启动 uptime、Spectral 渲染和进度刷新定时器。
5. 用户在连接面板发起连接后，TCP 控制层和 UDP 流接收层开始与设备交互。

## 主要子系统

### UI 层

- `MainWindow` 是轻量组合根，只负责顶层对象创建顺序、窗口设置恢复/保存和关闭生命周期。
- `MainWindowChrome` 创建主窗口静态 UI 骨架：侧边栏、顶部栏、主 splitter、`ViewerAreaWidget`、右侧 Panel stack 和底部日志。
- `MainWindowPanelRegistry` 创建并注册右侧业务 Panel，集中维护 Panel index、标题、系统日志 toggle 和光谱分析 Panel 激活。
- `DeviceUiCoordinator` 连接设备层与 UI 层，负责连接状态、UDP bind、帧分发、录制回放、Spectral 刷新、stream stats、raw log 和 uptime。
- `ThemeManager` 维护应用主题目录，集中加载深色/户外亮色 QSS，并保存 `ui/theme` 用户偏好。
- `ViewerAreaWidget` 管理 Raw16、SliceStitch16、Spectral、Playback 四个图像页，以及图像统计 overlay 和 Spectral progress overlay。
- `ImageFrameUtils` 提供 Mono8/Mono16 显示图转换与 Mono16 统计计算。
- `SpectralScanController` 管理 Live/Playback 的 Raw16/SliceStitch16 光谱扫描缓存、进度状态和渲染入口。
- `SpectrumAnalysisCoordinator` 管理 SliceStitch16 光谱分析的最新帧缓存、采样线 overlay、曲线窗口和曲线刷新。
- `WindowSettingsStore` 集中处理窗口级 QSettings 和 Panel index 迁移。
- `src/Ui/panels/` 每个面板处理一个业务域：
  - `ConnectionPanel`：设备 IP、TCP 端口、本地 UDP 端口、连接/断开/Ping。
  - `CameraPanel`：相机采集设备选择、分辨率、采集启动/停止。
  - `MirrorPanel`：转镜角度、速度、归零、预设位等控制。
  - `IrPanel`：红外参数、校准、状态/温度/模块查询。
  - `CollectPanel`：采集流程开始/停止/状态查询。
  - `StreamPanel`：Raw16/SliceStitch16 通道订阅、取消订阅、状态轮询。
  - `SpectralPanel`：光谱显示来源、源通道、单波段/范围平均/RGB 合成参数与扫描状态。
- `RecordPlaybackPanel`：远程录制数据查询、保留时间设置、`raw/tif` 下载缓存、Playback 播放控制和 tif 渲染参数。
  - `SpectrumAnalysisPanel`：SliceStitch16 光谱分析参数、水平采样线列表、曲线处理和曲线窗口入口。
  - `DashboardPanel`：连接、转镜、流统计、运行时间等摘要信息。
  - `LogPanel`：显示 TCP raw log。
- `src/Ui/widgets/` 提供侧栏、顶部栏、图像视图、QCustomPlot 等复用部件。
- 主图像区由 `ViewerAreaWidget` 承载，包含 Raw16、SliceStitch16、Spectral、Playback 四个 `ImageView` 页面。

### 设备聚合层

`DeviceClient` 是 UI 与设备通信之间的门面：
- 持有 `ControlClient` 和 `StreamClient`。
- 持有 `SystemService`、`MirrorService`、`CameraService`、`IrService`、`CollectService`、`StreamControlService`。
- 转发 TCP 连接状态为 `connectionChanged`。
- 把控制事件 `mirror.angle` 转发为 `mirrorAngleEvent`。
- 把 UDP 完整帧以 `StreamFrame` 结构转发为 `frameReady`。

### TCP 控制层

`ControlClient` 负责 TCP 连接中的 RPC 帧：
- 使用 `QTcpSocket`，显式设置 `QNetworkProxy::NoProxy`。
- 请求帧结构为 `CtrlHeader` 后接 JSON body，body 形如 `{"cmd": "...", "params": ...}`。
- 每个请求分配递增 seq，并在 `pending_` 中保存回调与超时定时器。
- 响应与错误按 seq 查找 pending，生成 `RpcResult`。
- 事件包读取 `evt` 和 `data`，通过 `eventReceived` 发给上层。
- 断线时按指数退避重连，延迟从 1000 ms 增长到上限 5000 ms。

### Service 层

Service 类继承或使用 `RpcServiceBase`，把业务 API 封装为命令名和参数：
- `SystemService`：`system.ping`、`system.version`、`system.status`
- `StreamControlService`：`stream.subscribe`、`stream.unsubscribe`、`stream.status`
- `MirrorService`：角度查询、速度、相对/绝对目标、启动/停止、home、set home、preset
- `CameraService`：start/stop stream、分辨率、设备选择、设备列表
- `IrService`：版本/图像/参数/滤波/翻转/积分/模式/查询/维护/坏元管理
- `CollectService`：采集开始/停止/状态

`RpcServiceBase` 使用 `QPointer<QObject>` 保护 UI context；回调返回时如果面板对象已销毁，则丢弃回调。

### UDP 数据层

`StreamClient` 负责绑定本地 UDP 端口并读取 datagram：
- 默认端口来自连接面板，当前默认 `1400`。
- 成功绑定后尝试把接收缓冲区设置为 64 MiB。
- 每秒根据 `FrameAssembler::framesReceived()` 增量计算总体 FPS，并维护 Raw16/SliceStitch16 的按通道 FPS。
- 每个 UDP 包先检查 `StreamHeader` 的 magic 和版本，再交给 `FrameAssembler`。
- `StreamHeader` 当前是 UDP v2 64 字节头，携带通道、帧号、宽高、像素格式、`meta_flags`、时间戳、转镜元数据和 `frame_type`。

`FrameAssembler` 负责分片重组：
- 以 `(channel, frame_id)` 作为帧 key。
- 用 `QBitArray` 跟踪已收到分片。
- slot 记录并跨分片校验宽高、像素格式、通道和 `frameType`。
- 完整后发出 `frameReady(StreamFrame)`，其中包含 `channel`、`width`、`height`、`pixfmt`、`streamFrameId`、`frameType` 和 payload。
- 每 100 ms 清理超过 2000 ms 未更新的帧。
- 最多缓存 256 个 frame slot，总缓冲上限 256 MiB。
- slot 溢出时丢弃最早创建的 slot 并增加丢帧计数。

## 数据流

### 控制请求

1. 用户点击 UI 控件。
2. 面板调用 `DeviceClient` 暴露的对应 service。
3. service 组装命令名和 JSON 参数。
4. `ControlClient::request()` 分配 seq、安装超时定时器、发送 `CtrlHeader + JSON`。
5. 设备返回响应或错误。
6. `ControlClient::tryConsume()` 解析 JSON，构造 `RpcResult` 并调用 pending 回调。
7. 面板根据结果刷新 UI 或弹出错误。

### 图像流

1. 用户在 `StreamPanel` 勾选通道并应用订阅。
2. `StreamControlService::subscribe()` 通知设备把指定通道推送到本地 UDP 端口。
3. `DeviceUiCoordinator` 确保 `StreamClient` 已绑定该端口。
4. `StreamClient` 持续读取 UDP 包并交给 `FrameAssembler`。
5. 完整帧产生后发出 `StreamFrame`。
6. `DeviceUiCoordinator` 按通道选择 `ViewerAreaWidget` 中的 Raw/Slice 页面；Mono8 直接显示，Mono16 通过 `ImageFrameUtils` 做当前帧 min/max 拉伸到 8-bit 灰度。
7. 对 `HeaderFrame/DataFrame/TailFrame`，`DeviceUiCoordinator` 同时将帧送入 `SpectralScanController` 的 Live 扫描缓存。
8. 顶栏与仪表盘更新按通道 FPS、接收帧数、丢帧数。

### Spectral 光谱显示

1. 服务端把高光谱数据拆成单列 UDP 帧，payload 排布为 `height x bands`，`StreamHeader.width` 表示 bands/通道数。
2. `FrameAssembler` 透传 `frame_type` 和服务端 `frame_id`，UI 侧用 `streamFrameId` 识别列顺序和缺口。
3. `SpectralScanController` 持有 Live/Playback 两组 `SpectralScanBuilder`；构建器遇到 `HeaderFrame` 开始或重置扫描，`DataFrame` 追加列，`TailFrame` 结束当前扫描。
4. 同一源通道出现缺列时，构建器用上一列补齐；双通道订阅造成的正常 `streamFrameId` 步长会被学习。
5. `SpectralPanel` 控制源通道、显示模式和来源模式：Auto、Live、Playback。
6. Auto 模式在回放活跃时显示 Playback Spectral，停止后显示 Live；强制 Live 或 Playback 时按用户选择显示。
7. 回放期间 Live 缓存仍持续接收实时帧，Playback 缓存由 `RecordedFrame` 构建。

### SliceStitch16 光谱分析

1. 激活侧边栏“光谱分析”时，`MainWindowPanelRegistry` 自动切换到 SliceStitch16 图像页，打开或置顶 `SpectrumCurveDialog`。
2. `DeviceUiCoordinator` 在 `frameReady` 中把最新 SliceStitch16 Mono16 原始 payload、宽高和 `streamFrameId` 交给 `SpectrumAnalysisCoordinator` 缓存。
3. `ViewerAreaWidget` 启用 SliceStitch16 分析 overlay；底层 `ImageView` 绘制坐标刻度和水平采样线，支持点击添加线、拖动修改 y、右键删除线。
4. `SpectrumAnalysisPanel` 保存采样线、波长映射范围、刷新率和曲线处理参数。
5. `SpectrumCurveDialog` 内部定时器按刷新率发出 `sampleRefreshRequested`。
6. `SpectrumAnalysisCoordinator` 从最新原始 Mono16 帧中按采样线取单行数据。
7. 曲线数据按最大绘制点数做等间距抽点，并对每个绘制点做居中移动平均滤波。
8. X 轴使用原始 x 在 `xStart..xEnd` 内的线性位置映射为波长，Y 轴使用滤波后的 DN 值。
9. `SpectrumCurveDialog` 用所有实际绘制点计算全局 Y 范围；小于最小跨度时固定跨度，超过阈值时应用 `yRangeMultiplier`，并把最小值锚定到指定百分比位置。

### 远程录制数据下载流程

1. 用户在 `RecordPlaybackPanel` 查询 `record.get_retention` 或设置 `record.set_retention`。
2. 用户按 `raw` 或 `tif` 调用 `record.list_recent` 查询最近记录，列表以服务端返回的 `record_id` 和文件清单展示。
3. 用户多选记录后，客户端先检查 `recordings/remote_cache/<type>/<record_id>/` 是否已有完整缓存；已缓存且大小校验通过的记录直接加入播放序列。
4. 未缓存记录通过 `record.fetch` 获取一次性 `transfer_id` 和 `file_port`。
5. `RemoteFileDownloader` 异步连接文件 TCP 端口，发送 `{"transfer_id":"..."}` 握手，并按 `FileChunkHeader + filename + payload` 接收文件。
6. 下载时按 `record.fetch.files[].record_id + name` 分发到对应 record 缓存目录；文件名必须匹配 fetch 响应且不能包含绝对路径、`..` 或路径分隔符。
7. 校验 magic、version、file index、file size、offset、LastChunk/LastFile 和累计字节数；失败、取消、断连或关闭窗口都会删除本次临时缓存。

### 远程回放流程

1. raw 记录由 `.raw + .json` 组成，客户端读取 `.json` 的 `width`、`height`、`frame_count` 和 `frames`，并用 `width * height * 2 * frame_count` 校验 `.raw` 文件大小。
2. raw 多选记录按 `record_id` 的 unsigned 64-bit 数值从小到大合并为播放序列；播放时按帧偏移读取当前帧，不整文件载入内存。
3. tif 记录是 BigTIFF，一个 tif 文件是 Playback 播放序列中的一张投影图；客户端用 libtiff 遍历 page，校验所有 page 尺寸和 16bit 单通道灰度格式一致。
4. tif 渲染参数位于 `RecordPlaybackPanel` 内部，包括单波段、范围平均、单波段 RGB 合成和范围平均 RGB 合成；范围按 `[begin, end)`，`end=0` 表示到最后一个 page。
5. `RecordPlaybackPanel` 将 raw/tif 渲染结果作为 `QImage` 发给 `DeviceUiCoordinator`，只显示到 `ViewerAreaWidget::PlaybackView`。
6. 远程回放不再进入 `SpectralScanController` 的 Playback 缓存；`SpectralPanel` 只保留 Live/Auto 光谱显示。

## 存储与持久化

- 客户端不再本地录制 `.asrec`；历史数据由服务端保存并通过 `record.*` 查询与拉取。
- 远程下载缓存默认位于 `recordings/remote_cache/<type>/<record_id>/`。
- raw 缓存必须包含同名 `.raw + .json`；tif 缓存为 BigTIFF `.tif`。
- 成功缓存会被复用；失败、取消、断连和关闭窗口会删除本次不完整缓存。
- 光谱分析参数、采样线和曲线窗口 geometry 使用 `QSettings` 的 `spectrumAnalysis/` 前缀。
- 应用主题使用 `QSettings` 的 `ui/theme` 保存；启动时恢复，顶部栏右侧可即时切换。
- 运行日志由 plog 写入 `log/log.txt`。
- 用户界面状态由 `QSettings` 保存到平台默认位置。

## 常见修改入口

- 新增 RPC 命令：先改 `src/Client/rpc/RpcCommands.h`，再在 `src/Client/services/` 添加 service 方法，最后接入对应 panel。
- 新增设备事件：在 `DeviceClient.cpp` 的 `eventReceived` lambda 中识别 `evt`，新增 signal 并在 UI 层连接。
- 新增图像通道：更新 `Protocol.h` 的 `StreamChannel`、`StreamPanel` 的通道选择、`DeviceUiCoordinator` 的通道分发和 `ViewerAreaWidget` 的图像视图。
- 调整 Spectral 重建：优先看 `src/Ui/SpectralScanController.*`、`src/Ui/SpectralScanBuilder.*`、`src/Ui/panels/SpectralPanel.*` 和 `DeviceUiCoordinator` 的 Spectral 刷新逻辑。
- 调整光谱分析：优先看 `src/Ui/SpectrumAnalysisCoordinator.*`、`src/Ui/panels/SpectrumAnalysisPanel.*`、`src/Ui/SpectrumCurveDialog.*`、`src/Ui/widgets/ImageView.*`。
- 调整远程录制数据接口：优先改 `RecordService.*` 和 `RemoteFileDownloader.*`，再同步 `RecordPlaybackPanel.*`。
- 调整远程回放 UI：改 `RecordPlaybackPanel.*`，必要时同步 `DeviceUiCoordinator` 的 Playback `QImage` 渲染连接。
- 调整 UI 视觉：优先改 `resources/style/industrial.qss` 或 `resources/style/outdoor_light.qss`；主题目录和加载逻辑改 `src/Ui/ThemeManager.*`；结构性布局改 `MainWindowChrome`、`ViewerAreaWidget` 或具体 panel/widget。
- 调整软件发布版本：创建 Git tag（推荐 `vX.Y.Z`）；`cmake/GitVersion.cmake` 会派生运行时版本头、Windows 资源版本和安装包脚本。
- 修改构建环境：优先改 `config.bat`；依赖目录和 include/link 规则在 `libs/libsdefine.cmake` 与顶层 `CMakeLists.txt`。

## 当前状态观察

- Panel 索引当前为：常规 panel `0-9`，光谱分析是 `9`，系统日志是特殊 index `10`；`window/panelVersion` 当前为 `3`。
- 顶层 CMake 使用 `file(GLOB_RECURSE)` 收集 `src/Client` 和 `src/Ui`，新源文件通常会被纳入构建。
- `CMAKE_AUTOMOC` 已启用；带 `Q_OBJECT` 的类优先放在头文件中，让 CMake 自动生成 moc 文件。
- QCustomPlot 已复制到 `src/Ui/widgets/qcustomplot.*`，主项目不链接 `LineProfileReusable`。
- `Common` 子库当前没有接入主目标；如果业务代码开始使用 `Common/src` 中的实现，需要同步调整 CMake。
- 部分 `.bat` 和源码注释/字符串在当前终端可能显示乱码，疑似编码显示问题；不要盲目转换编码，除非任务明确要求。
- `run.bat` 会 `taskkill /IM AtingSpectrographClient.exe /F`，运行前注意是否有用户正在使用客户端。
