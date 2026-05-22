# AtingSpectrographClient 架构说明

## 启动路径

1. `src/main.cpp` 初始化日志、Qt 应用、全局字体和 QSS。
2. 创建 `MainWindow` 并进入 `QApplication::exec()`。
3. `MainWindow` 构造 `DeviceClient`，再调用 `setupUi()`、`setupPanels()`、`setupConnections()`、`loadSettings()`。
4. 用户在连接面板发起连接后，网络层与设备交互开始。

## 主要子系统

### UI 层

- `MainWindow` 是应用组合根，负责布局、面板切换、设备信号汇总与图像显示。
- `src/Ui/panels/` 中每个面板处理一个业务域：
  - `ConnectionPanel`：设备 IP、TCP 端口、本地 UDP 端口、连接/断开/Ping。
  - `CameraPanel`：相机采集设备选择、分辨率、采集启动/停止。
  - `MirrorPanel`：转镜角度、速度、归零、预设位等控制。
  - `IrPanel`：红外参数、校准、状态/温度/模块查询、原始命令发送。
  - `CollectPanel`：采集流程开始/停止/状态查询。
  - `StreamPanel`：Raw16/SliceStitch16 通道订阅、取消订阅、状态轮询。
  - `RecordPlaybackPanel`：实时帧录制、`.asrec` 文件选择、回放 FPS/帧数/循环控制、进度跳转与损坏帧统计。
  - `DashboardPanel`：连接、转镜、流统计、运行时间等摘要信息。
  - `LogPanel`：展示 TCP raw log。
- `src/Ui/widgets/` 提供侧栏、顶栏、图像显示等复用部件。
- 主图像区包含 Raw16、SliceStitch16、Playback 三个 `ImageView` 页面；回放开始时自动切到 Playback。

### 设备聚合层

`DeviceClient` 是 UI 与设备通讯之间的门面：

- 持有 `ControlClient` 与 `StreamClient`。
- 持有 `SystemService`、`MirrorService`、`CameraService`、`IrService`、`CollectService`、`StreamControlService`。
- 转发 TCP 连接状态为 `connectionChanged`。
- 把控制事件 `mirror.angle` 转发为 `mirrorAngleEvent`。
- 把 UDP 完整帧转发为 `frameReady`。

### TCP 控制面

`ControlClient` 负责 TCP 连接与 RPC 帧：

- 使用 `QTcpSocket`，显式设置 `QNetworkProxy::NoProxy`。
- 请求帧结构：`CtrlHeader` 后接 JSON body，body 形如 `{"cmd": "...", "params": ...}`。
- 每个请求分配递增 seq，并在 `pending_` 中保存回调与超时定时器。
- 响应与错误按 seq 查找 pending，生成 `RpcResult`。
- 事件包读取 `evt` 和 `data`，通过 `eventReceived` 发给上层。
- 断线时按指数退避重连，延迟从 1000 ms 增长到上限 5000 ms。

### Service 层

Service 类继承或使用 `RpcServiceBase`，把业务 API 封装为命令名和参数：

- `SystemService`：`system.ping`、`system.version`、`system.status`
- `StreamControlService`：`stream.subscribe`、`stream.unsubscribe`、`stream.status`
- `MirrorService`：角度查询、速度、相对/绝对目标、启动/停止、home、set home、preset
- `CameraService`：start/stop stream、分辨率、设备选择与设备列表
- `IrService`：原始命令、校准、亮度/对比度/积分时间、模块/自检/温度查询
- `CollectService`：采集开始/停止/状态

`RpcServiceBase` 使用 `QPointer<QObject>` 保护 UI context：回调返回时如果面板对象已销毁，则丢弃回调。

### UDP 数据面

`StreamClient` 负责绑定本地 UDP 端口并读取 datagram：

- 默认由连接面板提供端口，当前默认 `1400`。
- 成功绑定后把接收缓冲区尝试设置为 64 MiB。
- 每秒根据 `FrameAssembler::framesReceived()` 增量计算 FPS。
- 每个 UDP 包先检查 `StreamHeader` 的 magic 与版本，再交给 `FrameAssembler`。

`FrameAssembler` 负责分片重组：

- 以 `(channel, frame_id)` 作为帧 key。
- 用 `QBitArray` 跟踪已收到分片。
- 完整后发出 `frameReady(channel, width, height, pixfmt, data)`。
- 每 100 ms 清理超过 2000 ms 未更新的帧。
- 最多缓存 256 个 frame slot，总缓冲上限 256 MiB。
- slot 溢出时丢弃最早创建的 slot 并增加丢帧计数。

### 录制与回放

`src/Client/recording/` 负责本地 `.asrec` 文件的写入与读取：

- `RecordingFileFormat.*` 定义文件头、帧头、读写函数和 CRC32。
- `RecordedFrame.h` 是回放向 UI 传递的帧结构，并注册为 Qt metatype。
- `FrameRecorder` 是 UI 可见的录制门面，管理 worker 线程生命周期与统计查询。
- `FrameRecorderWriterWorker` 在独立 `QThread` 中写文件，避免实时帧写入阻塞 UI。
- `FramePlaybackController` 异步扫描录制文件建立索引，然后按定时器 FPS 读取并发出回放帧。

`.asrec` 文件结构：

- 文件头 magic 为 `ASRC`，版本为 `1`，保存创建时间、帧数、时长和 flags。
- 每帧由 `AFRM` 帧头加 payload 组成，帧头保存通道、宽高、像素格式、帧序号、时间戳、payload 长度和 CRC32。
- 正常停止录制会重写文件头并设置 `FileFlagClosedOk`。
- 回放扫描会用真实帧头/payload 偏移建立索引；未正常关闭或文件头统计为空时，使用扫描结果恢复帧数和时长。
- 回放读取时校验 payload CRC，失败帧计入损坏帧统计。

## 数据流

### 控制请求

1. 用户点击 UI 控件。
2. 面板调用 `DeviceClient` 暴露的对应 service。
3. service 组装命令名和 JSON 参数。
4. `ControlClient::request()` 分配 seq、安装超时定时器、发送 `CtrlHeader + JSON`。
5. 设备返回响应或错误。
6. `ControlClient::tryConsume()` 解析 JSON，构造 `RpcResult` 并调用 pending 回调。
7. 面板根据结果刷新 UI 或弹出错误。

### 事件通知

1. 设备通过 TCP 发送 `MsgType::Event`。
2. `ControlClient` 解析 `evt` 与 `data`。
3. `DeviceClient` 当前识别 `mirror.angle`。
4. `MainWindow`、`MirrorPanel` 等订阅 `mirrorAngleEvent` 并更新界面。

### 图像流

1. 用户在 `StreamPanel` 勾选通道并应用订阅。
2. `StreamControlService::subscribe()` 通知设备把指定通道推送到本地 UDP 端口。
3. `MainWindow` 确保 `StreamClient` 已绑定该端口。
4. `StreamClient` 持续读取 UDP 包并交给 `FrameAssembler`。
5. 完整帧产生后发出 `frameReady`。
6. `MainWindow` 按通道选择 `ImageView`，Mono8 直接显示，Mono16 做当前帧 min/max 拉伸到 8-bit 灰度。
7. 顶栏与仪表盘更新 FPS、接收帧数、丢帧数。

### 录制流程

1. 用户在 `RecordPlaybackPanel` 选择或生成 `.asrec` 路径并开始录制。
2. `FrameRecorder` 启动 `FrameRecorderWriterWorker`，先写入占位文件头。
3. `MainWindow` 每次收到 `DeviceClient::frameReady` 时，把原始帧数据同时交给 recorder。
4. worker 为每帧生成帧头、CRC 和相对时间戳，进入队列后由写入线程落盘。
5. 停止录制时 worker drain 队列，重写文件头并发出停止信号。

### 回放流程

1. 用户在 `RecordPlaybackPanel` 选择 `.asrec` 文件。
2. `FramePlaybackController` 的扫描线程读取文件头和所有帧头，建立随机访问索引。
3. 用户设置 FPS、播放帧数限制和循环开关后开始回放。
4. controller 按定时器读取当前索引对应的 payload，校验 CRC，发出 `RecordedFrame`。
5. `RecordPlaybackPanel` 转发帧，`MainWindow` 渲染到 Playback `ImageView`。
6. 进度条释放时调用 `seekTo()`；播放结束按循环设置回到窗口起点或停止。

## 存储与持久化

- 实时设备帧默认只显示到 UI；用户开启录制后，原始帧会持久化为 `.asrec`。
- `RecordPlaybackPanel` 默认在 `recordings/` 下生成 `record_yyyyMMdd_HHmmss.asrec`。
- 录制/回放设置通过 `QSettings` 保存最近路径、FPS、播放帧数限制和循环开关。
- 运行日志由 plog 写入 `log/log.txt`。
- 用户界面状态由 `QSettings` 保存到平台默认位置。

## 常见修改入口

- 新增 RPC 命令：先改 `src/Client/rpc/RpcCommands.h`，再在 `src/Client/services/` 添加 service 方法，最后接入对应 panel。
- 新增设备事件：在 `DeviceClient.cpp` 的 `eventReceived` lambda 中识别 `evt`，新增 signal 并在 UI 层连接。
- 新增图像通道：更新 `Protocol.h` 的 `StreamChannel`，`StreamPanel` 的通道选择，`MainWindow::frameReady` 的通道分发与图像视图。
- 调整录制文件格式：优先改 `RecordingFileFormat.*`，再同步 `FrameRecorderWriterWorker` 写入逻辑与 `FramePlaybackController` 扫描/读取逻辑；注意兼容已录制文件。
- 调整录制/回放 UI：改 `RecordPlaybackPanel.*`，必要时同步 `MainWindow` 的 Playback 视图切换和帧渲染连接。
- 调整 UI 视觉：优先改 `resources/style/industrial.qss`；结构性布局改 `MainWindow` 或具体 panel/widget。
- 修改构建环境：优先改 `config.bat`；依赖目录和 include/link 规则在 `libs/libsdefine.cmake` 与顶层 `CMakeLists.txt`。

## 当前状态观察

- 顶层 CMake 使用 `file(GLOB_RECURSE)` 收集 `src/Client` 与 `src/Ui`，因此新增源文件通常会被纳入构建；但 CMake 缓存场景下仍建议重新配置。
- `CMAKE_AUTOMOC` 已启用；带 `Q_OBJECT` 的类优先放在头文件中，让 CMake 自动生成 moc 文件。`FrameRecorderWriterWorker` 已从 `FrameRecorder.cpp` 拆出，`FrameRecorder.cpp` 不再包含 `FrameRecorder.moc`。
- `Common` 子库目前没有接入主目标，若业务代码开始使用 `Common/src` 中的实现，需要同步调整 CMake。
- 部分 `.bat` 和源码注释/字符串在当前终端呈现乱码，疑似编码显示问题；不要盲目“修复”为另一种编码，除非任务明确要求。
- `run.bat` 会 `taskkill /IM AtingSpectrographClient.exe /F`，运行前注意是否有用户正在使用客户端。
