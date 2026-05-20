#pragma once

namespace RpcTimeout {
static const int Normal = 3000;
static const int Slow = 5000;
}

namespace RpcCommand {
namespace System {
static const char* const Ping = "system.ping";
static const char* const Version = "system.version";
static const char* const Status = "system.status";
}

namespace StreamControl {
static const char* const Subscribe = "stream.subscribe";
static const char* const UnsubscribeAll = "stream.unsubscribe";
static const char* const Status = "stream.status";
}

namespace Mirror {
static const char* const QueryAngle = "mirror.query_angle";
static const char* const SetSpeed = "mirror.set_speed";
static const char* const SetTarget = "mirror.set_target";
static const char* const SetTargetAbsolute = "mirror.set_target_absolute";
static const char* const StartMove = "mirror.start_move";
static const char* const StopMove = "mirror.stop_move";
static const char* const Home = "mirror.home";
static const char* const SetHome = "mirror.set_home";
static const char* const GotoPreset = "mirror.goto_preset";
}

namespace Camera {
static const char* const StartStream = "camera.start_stream";
static const char* const StopStream = "camera.stop_stream";
static const char* const GetResolution = "camera.get_resolution";
static const char* const SetResolution = "camera.set_resolution";
static const char* const SelectDevice = "camera.select_device";
static const char* const ClearSelectedDevice = "camera.clear_selected_device";
static const char* const GetSelectedDevice = "camera.get_selected_device";
static const char* const DeviceOptions = "camera.device_options";
}

namespace Ir {
static const char* const SendRaw = "ir.send_raw";
static const char* const TriggerCalibration = "ir.trigger_calibration";
static const char* const SetBrightness = "ir.set_brightness";
static const char* const SetContrast = "ir.set_contrast";
static const char* const SetIntegration = "ir.set_integration";
static const char* const ReadModuleId = "ir.read_module_id";
static const char* const ReadSelfCheck = "ir.read_self_check";
static const char* const ReadFocusPlaneTemp = "ir.read_focus_plane_temp";
static const char* const ReadCoreTemp = "ir.read_core_temp";
static const char* const QueryIntegrationTime = "ir.query_integration_time";
}

namespace Collect {
static const char* const Start = "collect.start";
static const char* const Stop = "collect.stop";
static const char* const Status = "collect.status";
}
}
