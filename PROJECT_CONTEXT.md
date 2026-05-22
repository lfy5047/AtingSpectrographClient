# AtingSpectrographClient 项目上下文

## 项目概览

AtingSpectrographClient 是一个 Windows 桌面端光谱仪/成像设备控制客户端。当前代码显示它以 Qt Widgets 构建主界面，通过 TCP JSON-RPC 控制设备，通过 UDP 接收图像流，并在界面中显示 Raw16、SliceStitch16、Spectral 与 Playback 图像页。客户端支持把实时原始帧录制为 `.asrec` 文件，并从录制文件建立索引后按 FPS 回放；Spectral 页可以把 `HeaderFrame/DataFrame/TailFrame` 单列光谱帧重建为空间宽度 × 高度 × 波段的扫描图，并支持实时流/回放流来源选择。

主要运行目标：

- Windows 桌面应用：`AtingSpectrographClient.exe`
- 控制连接：TCP，默认设备 `192.168.10.128:9000`
- 图像流接收：本地 UDP，默认端口 `1400`

## 技术栈

- C++11
- CMake + Ninja Multi-Config
- Qt 5.15.2：`Widgets`、`PrintSupport`、`Network`
- 第三方库位于 `libs/public`
  - `nlohmann/json`：RPC JSON 负载
  - `plog`：文件与控制台日志
  - `fmt`：以 header-only 方式引入
  - `boost/asio`：当前客户端代码未直接使用，但作为公共依赖可用
  - OpenCV 4.5.5 与 libtiff：依赖目录已配置，当前主目标未显式链接 OpenCV

## 构建、运行、清理

仓库规则要求在根目录构建：

```bat
.\make.bat
```

相关脚本：

- `config.bat`：硬编码项目根目录、构建目录、CMake、Qt、Visual Studio DevCmd 路径。
- `make.bat`：调用 `config.bat`，默认 Release；传入 `d` 时构建 Debug；使用 `Ninja Multi-Config`。
- `install.bat`：运行 `cmake --install` 到 `build\Release`。
- `run.bat`：先调用 `make.bat`，必要时安装，然后杀掉同名进程并运行 exe。
- `clear.bat`：清理脚本，使用前先读内容确认删除范围。

当前未发现测试入口；文档类变更通常不需要运行完整构建。业务代码变更建议至少运行 `.\make.bat`。

## 关键目录

- `src/main.cpp`：应用入口，初始化 plog、`QApplication`、全局字体与 QSS。
- `src/Ui/MainWindow.*`：主窗口布局、面板装配、全局信号连接、图像显示转换。
- `src/Ui/panels/`：右侧控制面板，按功能拆分连接、相机、转镜、红外、采集、流、光谱显示、录制回放、日志、仪表盘。
- `src/Ui/widgets/`：通用 UI 部件，如侧边栏、顶栏、图像视图、LED 指示器。
- `src/Client/core/`：`DeviceClient` 聚合控制连接、流连接与各业务服务。
- `src/Client/rpc/`：TCP 控制协议、JSON-RPC 请求/响应封装、命令名定义。
- `src/Client/services/`：面向 UI 的业务 API 封装，负责把按钮行为转换成 RPC 命令。
- `src/Client/stream/`：UDP 图像流接收、分片重组、`StreamFrame` 元数据透传、FPS/丢帧统计。
- `src/Client/recording/`：`.asrec` 文件格式、实时帧录制、录制文件扫描建索引与回放控制。
- `Common/`：通用工具、串口、队列、TimerManager 等；当前顶层 `CMakeLists.txt` 只包含 `Common/include`，未把 `Common` 子库加入主目标。
- `resources/`：Qt 资源文件与 `industrial.qss` 主题。
- `libs/public/`：随仓库提交的第三方依赖；默认不要全文扫描。

## 首读文件

新会话优先读这些文件即可快速建立上下文：

1. `PROJECT_CONTEXT.md`
2. `ARCHITECTURE.md`
3. `CMakeLists.txt`
4. `src/main.cpp`
5. `src/Ui/MainWindow.cpp`
6. `src/Client/core/DeviceClient.cpp`
7. `src/Client/rpc/Protocol.h`
8. `src/Client/rpc/ControlClient.cpp`
9. `src/Client/stream/StreamClient.cpp`
10. `src/Client/stream/FrameAssembler.cpp`
11. `src/Client/recording/RecordingFileFormat.h`
12. `src/Ui/SpectralScanBuilder.cpp`
13. `src/Ui/panels/SpectralPanel.cpp`
14. `src/Ui/panels/RecordPlaybackPanel.cpp`

## 主运行流程

1. `main()` 初始化 `log/log.txt` 滚动日志、控制台日志、Qt 应用信息和 `:/style/industrial.qss`。
2. `MainWindow` 创建 `DeviceClient`，搭建侧边栏、顶栏、图像区、右侧面板和底部日志。
3. `ConnectionPanel` 调用 `DeviceClient::connectTo()`，由 `ControlClient` 建立 TCP 连接。
4. TCP 连接成功后，`MainWindow` 使用连接面板中的 UDP 端口绑定 `StreamClient`，并查询系统版本。
5. 各控制面板通过 `DeviceClient` 下的 service 调用 RPC 命令，如相机、转镜、红外、采集、流订阅。
6. `ControlClient` 以 `CtrlHeader + JSON` 发送请求，按 seq 管理 pending 回调、超时和断线清理。
7. 设备事件以 `MsgType::Event` 到达；当前 `DeviceClient` 处理 `mirror.angle` 并转发为 Qt signal。
8. UDP 图像包进入 `StreamClient`，`FrameAssembler` 按通道和 frame id 重组完整帧，发出携带通道、尺寸、像素格式、`frameType`、`streamFrameId` 和 payload 的 `StreamFrame`。
9. `MainWindow` 在收到完整帧时，如果录制开启，先把原始帧数据与光谱帧元数据交给 `FrameRecorder` 写入 `.asrec`。
10. `MainWindow` 将 Mono8/Mono16 数据转换为 8-bit `QImage`，显示到 Raw16 或 SliceStitch16 图像视图，并更新按通道 FPS/帧数/丢帧统计。
11. `HeaderFrame/DataFrame/TailFrame` 同时进入 `SpectralScanBuilder`；Spectral 页按单波段、范围平均或 RGB 合成渲染，数据来源可选 Auto、Live 或 Playback。
12. `RecordPlaybackPanel` 选择录制文件后由 `FramePlaybackController` 异步扫描索引；开始回放会自动切到 Playback 图像页，并把回放帧喂给 Playback Spectral 缓存。

## 协议与集成点

控制面协议位于 `src/Client/rpc/Protocol.h`：

- 控制 magic：`0x4E495441`，注释为 `'ATIN' LE`
- 流 magic：`0x4D545341`，注释为 `'ASTM' LE`
- 控制协议版本：`1`
- UDP 流协议版本：`2`
- 控制头：`CtrlHeader`，16 字节
- 流头：`StreamHeader`，64 字节；前 32 字节保留 v1 关键字段偏移，后半部分携带时间戳、转镜元数据、`frame_type` 等信息
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

## 配置与持久化

- 构建环境路径在 `config.bat` 中硬编码，迁移机器时优先检查这里。
- UI 状态通过 `QSettings` 保存：窗口 geometry、splitter、当前面板、侧边栏折叠状态。
- 录制/回放状态通过 `QSettings` 保存：最近录制路径、最近回放路径、回放 FPS、播放帧数限制、循环播放开关。
- 应用组织与名称：`AtingSpectrograph` / `AtingSpectrographClient`。
- 运行日志写入相对路径 `log/log.txt`，最大 1 MiB，保留 10 个滚动文件。
- 录制文件默认写入相对目录 `recordings/record_yyyyMMdd_HHmmss.asrec`，格式由 `RecordingFileFormat.*` 定义。
- Qt 资源入口：`resources/resources.qrc`，当前只打包 `resources/style/industrial.qss`。

## 录制与回放

- `.asrec` 文件头 magic 为 `ASRC`，帧头 magic 为 `AFRM`，当前版本为 `1`。
- `FrameRecorder` 是 UI 层使用的薄封装，实际写入逻辑在 `FrameRecorderWriterWorker`，运行于独立 `QThread`。
- 录制写入完整原始帧数据，保留通道、宽高、像素格式、服务端 `streamFrameId`、`frameType`、相对时间戳、payload 字节数和 CRC32。
- 写入队列有 256 MiB 背压上限，超过后丢弃新帧并增加 `droppedByBackpressure` 统计。
- 正常停止时会重写文件头，填充 frameCount、durationMs，并设置 `FileFlagClosedOk`。
- `FramePlaybackController` 使用扫描线程建立帧索引，再用定时器按 FPS 读取 payload 并发出 `RecordedFrame`。
- 回放扫描记录真实帧头与 payload 偏移；文件头未正常关闭时会用扫描结果恢复 frameCount/durationMs。
- 回放时会校验 payload CRC，损坏帧计入 `damagedFrames`。

## Spectral 光谱显示

- `src/Client/stream/StreamFrame.h` 是实时流向 UI 传递完整帧的结构体，已注册 Qt metatype；它避免继续扩展长参数信号。
- `FrameAssembler` 在 slot 中保存并校验 `frameType`，完成重组后透传 `frameType` 与 `streamFrameId`。
- `StreamClient` 对 `frame_type` 做合法性校验，并按通道统计 FPS；同时订阅 Raw16 与 SliceStitch16 时，顶部 FPS 按当前视图/光谱通道显示。
- `SpectralScanBuilder` 根据 `HeaderFrame/DataFrame/TailFrame` 逐列构建扫描，payload 排布是 `height x bands`；如果同一通道出现帧号间隔，会使用上一列补齐缺口。
- 双通道同时订阅时，服务端全局递增的 `streamFrameId` 可能让单通道帧号步长为 2；构建器会学习同通道正常步长，避免把另一个通道的帧号当作缺列。
- `SpectralPanel` 提供来源选择：`Auto (Playback first)`、`Live`、`Playback`。Auto 在回放中显示 Playback，停止后显示 Live；强制 Live/Playback 时按用户选择显示。
- 回放期间 Live Spectral 缓存继续更新；选择 Playback 时可查看回放构建结果，播放停止后不会立即清空 Playback Spectral 缓存。
- 录制文件中途开始可能缺少前置 `HeaderFrame`，回放重建 Spectral 时无活跃扫描的 `DataFrame/TailFrame` 会被忽略，直到下一个 `HeaderFrame`。

## 当前状态观察

- 源码中部分中文字符串在当前终端显示为乱码，但代码意图多为中文 UI 文案；编辑时应保持文件编码一致，避免二次破坏。
- `Common/CMakeLists.txt` 定义了 `Common` 库，但顶层 CMake 当前未 `add_subdirectory(Common)`，主目标也未链接该库；当前主客户端主要使用 `Common/include`。
- `libs/public` 很大且多为第三方源码/头文件，除非依赖升级或编译问题，不要默认扫描。
- 工作区可能长期存在未提交业务改动与生成目录；开始新任务前用 `git status --short` 区分已有改动和本次改动。

## AI 交接规则

- 先读本文档和 `ARCHITECTURE.md`，再按任务定位具体源文件。
- 默认避免扫描 `libs/public/**`、`build/**`、`.cache/**`、`.git/**`。
- 修改业务逻辑后优先运行 `.\make.bat`；只改文档时可不构建。
- 不要回滚用户已有未提交改动；提交前用 `git status --short` 区分已有改动和本次改动。
- 后续 AI 修改项目后，更新 `AI_TASK_LOG.md` 的最新条目。
