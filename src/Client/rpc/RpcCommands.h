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
// 基础标定
static const char* const TriggerCalibration = "ir.trigger_calibration";
static const char* const ForceShutter = "ir.force_shutter";
// 版本
static const char* const GetVersion = "ir.get_version";
// 图像选择与显示
static const char* const SetImageType = "ir.set_image_type";
static const char* const SetTestPattern = "ir.set_test_pattern";
static const char* const SetColorMode = "ir.set_color_mode";
static const char* const SetBadPixelDisplayMode = "ir.set_bad_pixel_display_mode";
// 图像参数
static const char* const SetBrightness = "ir.set_brightness";
static const char* const SetContrast = "ir.set_contrast";
static const char* const SetAbMode = "ir.set_ab_mode";
static const char* const SetDde = "ir.set_dde";
static const char* const SetTemporalFilter = "ir.set_temporal_filter";
static const char* const SetMedianFilter = "ir.set_median_filter";
// 翻转与同步
static const char* const SetFlipHorizontal = "ir.set_flip_horizontal";
static const char* const SetFlipVertical = "ir.set_flip_vertical";
static const char* const SetExternalSync = "ir.set_external_sync";
// 积分时间
static const char* const SetIntegration = "ir.set_integration";
static const char* const SetManualIntegration = "ir.set_manual_integration";
static const char* const SetIntegrationGearMode = "ir.set_integration_gear_mode";
static const char* const SelectIntegrationGear = "ir.select_integration_gear";
static const char* const QueryIntegrationTime = "ir.query_integration_time";
// 模式控制
static const char* const SetStandby = "ir.set_standby";
static const char* const SetOnboardAutoCalibration = "ir.set_onboard_auto_calibration";
// 读取查询
static const char* const ReadModuleId = "ir.read_module_id";
static const char* const ReadSelfCheck = "ir.read_self_check";
static const char* const ReadFocusPlaneTemp = "ir.read_focus_plane_temp";
static const char* const ReadMean = "ir.read_mean";
static const char* const ReadCorrectionParamGear = "ir.read_correction_param_gear";
static const char* const ReadCoreTemp = "ir.read_core_temp";
static const char* const ReadBadPixelCount = "ir.read_bad_pixel_count";
// 维护与校正
static const char* const MaintenanceUnlock = "ir.maintenance_unlock";
static const char* const MaintenanceExec = "ir.maintenance_exec";
static const char* const TwoPointCalibP1 = "ir.two_point_calib_p1";
static const char* const TwoPointCalibP2 = "ir.two_point_calib_p2";
static const char* const SaveCalibParams = "ir.save_calib_params";
static const char* const ClearK = "ir.clear_k";
static const char* const ClearB = "ir.clear_b";
// 坏元管理
static const char* const BadPixelSearch = "ir.bad_pixel_search";
static const char* const SetBadPixelPosition = "ir.set_bad_pixel_position";
static const char* const SaveBadPixel = "ir.save_bad_pixel";
}

namespace Collect {
static const char* const Start = "collect.start";
static const char* const Stop = "collect.stop";
static const char* const Status = "collect.status";
static const char* const GetOversampling = "collect.get_oversampling";
static const char* const SetOversampling = "collect.set_oversampling";
static const char* const GetGateConfig = "collect.get_gate_config";
static const char* const SetGateConfig = "collect.set_gate_config";
}

namespace Record {
static const char* const GetRetention = "record.get_retention";
static const char* const SetRetention = "record.set_retention";
static const char* const ListRecent = "record.list_recent";
static const char* const Fetch = "record.fetch";
static const char* const Delete = "record.delete";
}
}
