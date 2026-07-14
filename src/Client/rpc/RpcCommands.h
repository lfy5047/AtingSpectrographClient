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
static const char* const BackgroundCalibrationStart = "system.background_calibration.start";
static const char* const BackgroundCalibrationStatus = "system.background_calibration.status";
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
namespace Core {
static const char* const Current = "ir.core.current";
}

namespace Legacy {
static const char* const TriggerCalibration = "ir.legacy.trigger_calibration";
static const char* const ForceShutter = "ir.legacy.force_shutter";
static const char* const GetVersion = "ir.legacy.get_version";
static const char* const SetImageType = "ir.legacy.set_image_type";
static const char* const SetTestPattern = "ir.legacy.set_test_pattern";
static const char* const SetColorMode = "ir.legacy.set_color_mode";
static const char* const SetBadPixelDisplayMode = "ir.legacy.set_bad_pixel_display_mode";
static const char* const SetBrightness = "ir.legacy.set_brightness";
static const char* const SetContrast = "ir.legacy.set_contrast";
static const char* const SetAbMode = "ir.legacy.set_ab_mode";
static const char* const SetDde = "ir.legacy.set_dde";
static const char* const SetTemporalFilter = "ir.legacy.set_temporal_filter";
static const char* const SetMedianFilter = "ir.legacy.set_median_filter";
static const char* const SetFlipHorizontal = "ir.legacy.set_flip_horizontal";
static const char* const SetFlipVertical = "ir.legacy.set_flip_vertical";
static const char* const SetExternalSync = "ir.legacy.set_external_sync";
static const char* const SetIntegration = "ir.legacy.set_integration";
static const char* const SetManualIntegration = "ir.legacy.set_manual_integration";
static const char* const SetIntegrationGearMode = "ir.legacy.set_integration_gear_mode";
static const char* const SelectIntegrationGear = "ir.legacy.select_integration_gear";
static const char* const QueryIntegrationTime = "ir.legacy.query_integration_time";
static const char* const SetStandby = "ir.legacy.set_standby";
static const char* const SetOnboardAutoCalibration = "ir.legacy.set_onboard_auto_calibration";
static const char* const ReadModuleId = "ir.legacy.read_module_id";
static const char* const ReadSelfCheck = "ir.legacy.read_self_check";
static const char* const ReadFocusPlaneTemp = "ir.legacy.read_focus_plane_temp";
static const char* const ReadMean = "ir.legacy.read_mean";
static const char* const ReadCorrectionParamGear = "ir.legacy.read_correction_param_gear";
static const char* const ReadCoreTemp = "ir.legacy.read_core_temp";
static const char* const ReadBadPixelCount = "ir.legacy.read_bad_pixel_count";
static const char* const MaintenanceUnlock = "ir.legacy.maintenance_unlock";
static const char* const MaintenanceExec = "ir.legacy.maintenance_exec";
static const char* const TwoPointCalibP1 = "ir.legacy.two_point_calib_p1";
static const char* const TwoPointCalibP2 = "ir.legacy.two_point_calib_p2";
static const char* const SaveCalibParams = "ir.legacy.save_calib_params";
static const char* const ClearK = "ir.legacy.clear_k";
static const char* const ClearB = "ir.legacy.clear_b";
static const char* const BadPixelSearch = "ir.legacy.bad_pixel_search";
static const char* const SetBadPixelPosition = "ir.legacy.set_bad_pixel_position";
static const char* const SaveBadPixel = "ir.legacy.save_bad_pixel";
}

namespace Ci05 {
static const char* const FocusStartPositive = "ir.ci05.focus_start_positive";
static const char* const FocusStartNegative = "ir.ci05.focus_start_negative";
static const char* const FocusStop = "ir.ci05.focus_stop";
static const char* const FocusStepPositive = "ir.ci05.focus_step_positive";
static const char* const FocusStepNegative = "ir.ci05.focus_step_negative";
static const char* const ZoomStartPositive = "ir.ci05.zoom_start_positive";
static const char* const ZoomStartNegative = "ir.ci05.zoom_start_negative";
static const char* const ZoomStop = "ir.ci05.zoom_stop";
static const char* const ZoomStepPositive = "ir.ci05.zoom_step_positive";
static const char* const ZoomStepNegative = "ir.ci05.zoom_step_negative";
static const char* const AutoFocus = "ir.ci05.auto_focus";
static const char* const SetFov = "ir.ci05.set_fov";
static const char* const ShutterOpen = "ir.ci05.shutter_open";
static const char* const ShutterClose = "ir.ci05.shutter_close";
static const char* const SetFocusSpeed = "ir.ci05.set_focus_speed";
static const char* const SetZoomSpeed = "ir.ci05.set_zoom_speed";
static const char* const CallPreset = "ir.ci05.call_preset";
static const char* const SetPreset = "ir.ci05.set_preset";
static const char* const SetFocalLengthMmX10 = "ir.ci05.set_focal_length_mm_x10";
static const char* const QueryFocalLength = "ir.ci05.query_focal_length";
static const char* const QueryFocusMotorPosition = "ir.ci05.query_focus_motor_position";
static const char* const QueryZoomMotorPosition = "ir.ci05.query_zoom_motor_position";
static const char* const MenuUser = "ir.ci05.menu_user";
static const char* const MenuRight = "ir.ci05.menu_right";
static const char* const MenuLeft = "ir.ci05.menu_left";
static const char* const MenuParamInc = "ir.ci05.menu_param_inc";
static const char* const MenuParamDec = "ir.ci05.menu_param_dec";
static const char* const PromptOn = "ir.ci05.prompt_on";
static const char* const PromptOff = "ir.ci05.prompt_off";
static const char* const SetSyncMode = "ir.ci05.set_sync_mode";
static const char* const SetBrightness = "ir.ci05.set_brightness";
static const char* const SetContrast = "ir.ci05.set_contrast";
static const char* const SetOverallBrightness = "ir.ci05.set_overall_brightness";
static const char* const SetOverallContrast = "ir.ci05.set_overall_contrast";
static const char* const SetSharpness = "ir.ci05.set_sharpness";
static const char* const SetY8Level = "ir.ci05.set_y8_level";
static const char* const SetEzoom = "ir.ci05.set_ezoom";
static const char* const SetFreeze = "ir.ci05.set_freeze";
static const char* const SetMirrorMode = "ir.ci05.set_mirror_mode";
static const char* const SetPolarityPalette = "ir.ci05.set_polarity_palette";
static const char* const SetAgcMode = "ir.ci05.set_agc_mode";
static const char* const SaveParams = "ir.ci05.save_params";
static const char* const IntegrationIncrease0p1Ms = "ir.ci05.integration_increase_0p1ms";
static const char* const IntegrationDecrease0p1Ms = "ir.ci05.integration_decrease_0p1ms";
static const char* const SetIntegrationMsX10 = "ir.ci05.set_integration_ms_x10";
static const char* const SetIntegrationMc = "ir.ci05.set_integration_mc";
static const char* const SetFrameRateHzX100 = "ir.ci05.set_frame_rate_hz_x100";
static const char* const ReadFrameRateHz = "ir.ci05.read_frame_rate_hz";
static const char* const SetIntegrationGear = "ir.ci05.set_integration_gear";
static const char* const SetIntegrationGearAuto = "ir.ci05.set_integration_gear_auto";
static const char* const SetBackgroundGear = "ir.ci05.set_background_gear";
static const char* const SetBackgroundGearAuto = "ir.ci05.set_background_gear_auto";
static const char* const TriggerShutterCompensation = "ir.ci05.trigger_shutter_compensation";
static const char* const TriggerSceneCompensation = "ir.ci05.trigger_scene_compensation";
static const char* const TriggerDefocusCompensation = "ir.ci05.trigger_defocus_compensation";
static const char* const TriggerIntegrationCorrection = "ir.ci05.trigger_integration_correction";
static const char* const SetBootCompensationMode = "ir.ci05.set_boot_compensation_mode";
static const char* const SetGearSwitchCompensationMode = "ir.ci05.set_gear_switch_compensation_mode";
static const char* const SetVideoSource = "ir.ci05.set_video_source";
static const char* const SetParamLine = "ir.ci05.set_param_line";
static const char* const SetDigitalFormat = "ir.ci05.set_digital_format";
static const char* const SetTestPattern = "ir.ci05.set_test_pattern";
static const char* const SetImageMode = "ir.ci05.set_image_mode";
static const char* const SetStatusOutputMode = "ir.ci05.set_status_output_mode";
static const char* const SetTmodFilter = "ir.ci05.set_tmod_filter";
static const char* const SetNtmFilter = "ir.ci05.set_ntm_filter";
static const char* const SetVerticalStripeRemoval = "ir.ci05.set_vertical_stripe_removal";
static const char* const ReadSerialNumber = "ir.ci05.read_serial_number";
static const char* const ReadWorkMinutes = "ir.ci05.read_work_minutes";
static const char* const ReadCoolingDoneSeconds = "ir.ci05.read_cooling_done_seconds";
static const char* const ReadStatus1 = "ir.ci05.read_status1";
static const char* const ReadStatus2 = "ir.ci05.read_status2";
static const char* const ReadStatus3 = "ir.ci05.read_status3";
static const char* const ReadStatus4 = "ir.ci05.read_status4";
static const char* const ReadWorkState = "ir.ci05.read_work_state";
static const char* const ReadSelfCheck = "ir.ci05.read_self_check";
}
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

namespace Binning {
static const char* const GetConfig = "binning.get_config";
static const char* const SetConfig = "binning.set_config";
}

namespace Roi {
static const char* const GetConfig = "roi.get_config";
static const char* const SetConfig = "roi.set_config";
}

namespace TempControl {
static const char* const Params = "tempctrl.params";
static const char* const Status = "tempctrl.status";
static const char* const SetAdjustTemperature = "tempctrl.set_adjust_temperature";
static const char* const SetSwitch = "tempctrl.set_switch";
static const char* const Query = "tempctrl.query";
static const char* const Set = "tempctrl.set";
static const char* const Save = "tempctrl.save";
static const char* const SendRaw = "tempctrl.send_raw";
}

namespace Record {
static const char* const GetRetention = "record.get_retention";
static const char* const SetRetention = "record.set_retention";
static const char* const ListRecent = "record.list_recent";
static const char* const Fetch = "record.fetch";
static const char* const Delete = "record.delete";
}
}
