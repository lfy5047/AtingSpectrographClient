# 背景矫正客户端设计

## 目标

在“数据采集”面板中提供一次完整的背景矫正操作。用户确认后，客户端启动服务端任务、持续显示任务阶段，并在任务完成或失败后给出明确结果。

## 范围

- 在 `CollectPanel` 新增“背景矫正”区块，包含状态文本和“开始背景矫正”按钮。
- 点击按钮时显示二次确认，明确该操作会移动转镜并触发红外机芯校正。
- 确认后调用 `system.background_calibration.start`，并以 300 ms 间隔调用 `system.background_calibration.status`。
- 在运行期间禁用启动按钮；当任务结束、启动失败、状态查询失败或面板销毁时，停止轮询并恢复可操作状态。
- 把协议阶段转换为中文：`idle` 为“未启动”，`moving_to_background` 为“正在前往背景位”，`calibrating` 为“正在背景矫正”，`restoring` 为“正在复位”，`completed` 为“背景矫正完成”，`failed` 为“背景矫正失败”。失败状态额外显示服务端返回的 `error`。

不增加独立导航页，不在本地持久化任务状态、任务编号或进度，也不改变服务端既定的转镜和机芯控制流程。

## 架构

`RpcCommands.h` 集中定义两个 `system.background_calibration.*` 命令。已有 `SystemService` 增加类型化的 `startBackgroundCalibration()` 与 `backgroundCalibrationStatus()` 方法：前者解析 `started`、`task_id` 和 `stage`，后者解析 `task_id`、`running`、`stage`、`error`、开始时间和结束时间。

`CollectPanel` 只负责交互状态：确认、发起、轮询、显示和停止定时器。它通过 `DeviceClient::systemApi()` 调用 `SystemService`，不直接构造 JSON-RPC 请求。定时器由面板拥有；每次轮询只在前一次回调返回后安排下一次，避免网络慢时产生并发查询。

## 交互与错误处理

点击“开始背景矫正”后，`QMessageBox::question` 的确认按钮才会发起请求。启动响应 `ok=false` 时，显示服务端错误（包括任务已运行的 `code=-12` 情况），保持按钮可用。启动成功后立即显示响应中的阶段，并开始轮询。

状态响应 `ok=false` 时，停止轮询、恢复按钮，并显示查询错误；`running=false` 时也停止轮询。完成时显示成功状态；失败时把非空 `error` 追加到状态标签并弹出警告。未知阶段保留原始阶段文本，保证协议新增阶段时仍有可见反馈。

## 测试

- `SystemServiceTest` 使用本地 `QTcpServer` 验证启动请求的命令名、空参数和启动响应解析；验证状态请求与所有状态字段解析。
- `PanelSettingsTest` 使用本地控制端模拟器验证：按钮存在；取消确认不会发送请求；确认后发送启动请求；收到运行状态后按钮禁用且显示中文阶段；收到完成或失败状态后停止轮询、恢复按钮，并在失败时显示错误。
- 执行受影响测试、完整 CTest 和 worktree 内独立 Debug 构建。
