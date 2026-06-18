# Temp Control Panel Design

## Goal

Add a client-side temperature-control module for the new `tempctrl.*` RPC namespace. The first version focuses on daily operation: status monitoring, target temperature setting, switch control, saving key settings, and an advanced fallback area for query/set/save/raw commands.

## Scope

- Add a `TempControlService` under `src/Client/services/`.
- Expose the service from `DeviceClient`.
- Add a `TempControlPanel` under `src/Ui/panels/`.
- Add a sidebar entry named `温控控制`.
- Place the panel between `红外热像` and `数据采集`.
- Refresh `tempctrl.status` automatically every 1 second while connected.
- Display parameter choices in Chinese in the UI while sending protocol keys such as `adjust_temperature` to the server.
- Persist only user inputs in `QSettings`; do not persist server status values.

## Out Of Scope

- A full editor for all 57 temperature-controller parameters.
- Local decoding of low-level serial frames.
- Persisting live status, switch state, measured temperature, or server errors.
- Charting temperature history.

## Architecture

### RPC Layer

`RpcCommands.h` gets a `TempControl` namespace containing:

- `tempctrl.params`
- `tempctrl.status`
- `tempctrl.set_adjust_temperature`
- `tempctrl.set_switch`
- `tempctrl.query`
- `tempctrl.set`
- `tempctrl.save`
- `tempctrl.send_raw`

`TempControlService` wraps these commands and follows existing service patterns:

- Use `RpcServiceBase::request()` for JSON-returning calls.
- Use `RpcServiceBase::simpleCmd()` where only success/failure matters.
- Return typed status data for `tempctrl.status`.
- Preserve raw JSON for advanced query/set/save/raw results so the panel can show useful diagnostics.

### UI Layer

`TempControlPanel` contains three sections:

1. Status section
   - Actual temperature
   - Adjust temperature
   - Switch state
   - Output enabled state
   - Error status
   - Timestamp
   - The panel starts a 1 second timer after connection and stops it when disconnected.

2. Common control section
   - Target temperature input
   - Set target temperature button
   - Switch on/off control
   - Save target temperature button

3. Advanced section
   - Chinese parameter selector backed by protocol keys.
   - Query selected key.
   - Set selected key with a value.
   - Save selected key.
   - Optional `module + param` fields for parameters not in the built-in key list.
   - Raw command input for troubleshooting.

The built-in selector shows Chinese labels, for example:

- `调节温度` -> `adjust_temperature`
- `实际温度` -> `actual_temperature`
- `设定温度` -> `set_temperature`
- `温度限速` -> `ramp_speed`
- `最高设定温度` -> `max_temperature`
- `温控开关` -> `switch`
- `功率输出状态` -> `output_enabled`
- `错误状态` -> `error_status`

The panel may show the protocol key or `module:param` in the result text after an operation, but the primary selectable text must be Chinese.

### Panel Index Migration

Adding a panel changes indexes after `Ir`. `MainWindowPanelRegistry::PanelVersion` must increase. `WindowSettingsStore::restore()` must map older saved panel indexes so existing users return to the same logical panel:

- Old `Collect` and later panels shift by one.
- Old `Log` shifts by one.
- Invalid old values still fall back to `Dashboard`.

### Settings

Persist under `panels/tempControl/`:

- Last target temperature input.
- Last selected built-in key.
- Last advanced value input.
- Last module input.
- Last param input.
- Last raw command input.

Do not persist:

- Actual temperature.
- Current switch state.
- Output enabled state.
- Error status.
- Timestamp.
- Last status response.

### Error Handling

- `code=-10` and `code=-11` should surface as clear user-facing text that the temperature controller is unavailable or serial port is not open.
- Other RPC failures should show the server message when available.
- Auto-refresh failures update the status area without modal dialogs.
- User-triggered set/save/raw failures may use warnings or a visible result label, matching existing panel behavior.

## Testing

Use TDD.

- Add `TempControlServiceTest` with a local `QTcpServer + ControlClient`.
- Verify `setAdjustTemperature()` sends `tempctrl.set_adjust_temperature` and `value`.
- Verify `setSwitch()` sends `tempctrl.set_switch` and `enable`.
- Verify `status()` parses `actual_temperature`, `adjust_temperature`, `switch`, `output_enabled`, `error_status`, and `ts`.
- Verify advanced `query/set/save/sendRaw` send the expected params.
- Extend `PanelSettingsTest` so `TempControlPanel` restores only user inputs.

Run focused tests first, then run the normal Debug build and CTest:

```powershell
.\make.bat d
ctest --test-dir build -C Debug --output-on-failure
```
