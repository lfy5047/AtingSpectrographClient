# AtingSpectrographClient 架构说明

## 启动路径

1. `src/main.cpp` 初始化 plog、Qt 应用、全局字体和 `:/style/industrial.qss`。
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
- `ViewerAreaWidget` 管理 Raw16、SliceStitch16、Spectral、Playback 四个图像页，以及图像统计 overlay 和 Spectral progress overlay。
- `ImageFrameUtils` 提供 Mono8/Mono16 显示图转换与 Mono16 统计计算。
- `SpectralScanController` 管理 Live/Playback 的 Raw16/SliceStitch16 光谱扫描缓存、进度状态和渲染入口。
- `SpectrumAnalysisCoordinator` 管理 SliceStitch16 光谱分析的最新帧缓存、采样线 overlay、曲线窗口和曲线刷新。
- `WindowSettingsStore` 集中处理窗口级 QSettings 和 Panel index 迁移。
- `src/Ui/panels/` 每个面板处理一个业务域：
  - `ConnectionPanel`：设备 IP、TCP 端口、本地 UDP 端口、连接/断开/Ping。
  - `CameraPanel`：相机采集设备选择、分辨率、采集启动/停止。
  - `MirrorPanel`：转镜角度、速度、归零、预设位等控制。
  - `IrPanel`：红外参数、校准、状态/温度/模块查询、原始命令发送。
  - `CollectPanel`：采集流程开始/停止/状态查询。
  - `StreamPanel`：Raw16/SliceStitch16 通道订阅、取消订阅、状态轮询。
  - `SpectralPanel`：光谱显示来源、源通道、单波段/范围平均/RGB 合成参数与扫描状态。
  - `RecordPlaybackPanel`：实时帧录制、`.asrec` 选择、回放 FPS/帧数/循环控制、进度跳转与损坏帧统计。
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
- `IrService`：原始命令、校准、亮度/对比度、积分时间、模块自检/温度查询
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

### 录制流程

1. 用户在 `RecordPlaybackPanel` 选择或生成 `.asrec` 路径并开始录制。
2. `FrameRecorder` 启动 `FrameRecorderWriterWorker`，先写入占位文件头。
3. `DeviceUiCoordinator` 每次收到 `DeviceClient::frameReady` 时，把原始帧数据、`frameType` 和 `streamFrameId` 交给 recorder。
4. worker 为每帧生成帧头、CRC 和相对时间戳，进入队列后由写入线程落盘。
5. 停止录制时 worker drain 队列，重写文件头并发出停止信号。

### 回放流程

1. 用户在 `RecordPlaybackPanel` 选择 `.asrec` 文件。
2. `FramePlaybackController` 的扫描线程读取文件头和所有帧头，建立随机访问索引。
3. 用户设置 FPS、播放帧数限制和循环开关后开始回放。
4. controller 按定时器读取当前索引对应的 payload，校验 CRC，发出 `RecordedFrame`。
5. `RecordPlaybackPanel` 转发帧，`DeviceUiCoordinator` 渲染到 Playback `ImageView`，并把光谱帧送入 `SpectralScanController` 的 Playback 扫描缓存。
6. 进度条释放时调用 `seekTo()`；播放结束按循环设置回到窗口起点或停止。

## 存储与持久化

- 实时设备帧默认只显示到 UI；用户开启录制后，原始帧持久化为 `.asrec`。
- `.asrec` 文件头 magic 为 `ASRC`，帧头 magic 为 `AFRM`，当前版本为 `1`。
- 正常停止录制会重写文件头并设置 `FileFlagClosedOk`。
- 回放扫描会用真实帧头/payload 偏移建立索引；未正常关闭或文件头统计为空时，使用扫描结果恢复帧数和时长。
- 回放读取时校验 payload CRC，失败帧计入损坏帧统计。
- 光谱分析参数、采样线和曲线窗口 geometry 使用 `QSettings` 的 `spectrumAnalysis/` 前缀。
- 运行日志由 plog 写入 `log/log.txt`。
- 用户界面状态由 `QSettings` 保存到平台默认位置。

## 常见修改入口

- 新增 RPC 命令：先改 `src/Client/rpc/RpcCommands.h`，再在 `src/Client/services/` 添加 service 方法，最后接入对应 panel。
- 新增设备事件：在 `DeviceClient.cpp` 的 `eventReceived` lambda 中识别 `evt`，新增 signal 并在 UI 层连接。
- 新增图像通道：更新 `Protocol.h` 的 `StreamChannel`、`StreamPanel` 的通道选择、`DeviceUiCoordinator` 的通道分发和 `ViewerAreaWidget` 的图像视图。
- 调整 Spectral 重建：优先看 `src/Ui/SpectralScanController.*`、`src/Ui/SpectralScanBuilder.*`、`src/Ui/panels/SpectralPanel.*` 和 `DeviceUiCoordinator` 的 Spectral 刷新逻辑。
- 调整光谱分析：优先看 `src/Ui/SpectrumAnalysisCoordinator.*`、`src/Ui/panels/SpectrumAnalysisPanel.*`、`src/Ui/SpectrumCurveDialog.*`、`src/Ui/widgets/ImageView.*`。
- 调整录制文件格式：优先改 `RecordingFileFormat.*`，再同步 `FrameRecorderWriterWorker` 和 `FramePlaybackController`。
- 调整录制/回放 UI：改 `RecordPlaybackPanel.*`，必要时同步 `DeviceUiCoordinator` 的 Playback 视图切换和帧渲染连接。
- 调整 UI 视觉：优先改 `resources/style/industrial.qss`；结构性布局改 `MainWindowChrome`、`ViewerAreaWidget` 或具体 panel/widget。
- 修改构建环境：优先改 `config.bat`；依赖目录和 include/link 规则在 `libs/libsdefine.cmake` 与顶层 `CMakeLists.txt`。

## 当前状态观察

- Panel 索引当前为：常规 panel `0-9`，光谱分析是 `9`，系统日志是特殊 index `10`；`window/panelVersion` 当前为 `3`。
- 顶层 CMake 使用 `file(GLOB_RECURSE)` 收集 `src/Client` 和 `src/Ui`，新源文件通常会被纳入构建。
- `CMAKE_AUTOMOC` 已启用；带 `Q_OBJECT` 的类优先放在头文件中，让 CMake 自动生成 moc 文件。
- QCustomPlot 已复制到 `src/Ui/widgets/qcustomplot.*`，主项目不链接 `LineProfileReusable`。
- `Common` 子库当前没有接入主目标；如果业务代码开始使用 `Common/src` 中的实现，需要同步调整 CMake。
- 部分 `.bat` 和源码注释/字符串在当前终端可能显示乱码，疑似编码显示问题；不要盲目转换编码，除非任务明确要求。
- `run.bat` 会 `taskkill /IM AtingSpectrographClient.exe /F`，运行前注意是否有用户正在使用客户端。
