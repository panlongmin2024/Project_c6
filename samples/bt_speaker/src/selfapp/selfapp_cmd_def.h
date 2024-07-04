#ifndef _SELFAPP_CMD_DEF_H_
#define _SELFAPP_CMD_DEF_H_

#include "selfapp_internal.h"

#define GROUP_PACK(x)   (((x) & 0x0F) << 4)
#define GROUP_UNPACK(x) (((x) & 0xF0) >> 4)

enum CmdGroup_e {
	CMDGROUP_GENERAL = 0x0,
	CMDGROUP_DEVICE_INFO = 0x1,
	CMDGROUP_REMOTE_CONTROL = 0x3,	//0x2 reserved
	CMDGROUP_OTADFU = 0x4,
	CMDGROUP_LED_PATTERN_CONTROL = 0x5,
	CMDGROUP_SPEAKER_SETTINGS = 0x6,
	CMDGROUP_OPEN_API = 0x7,
	CMDGROUP_LED_PACKAGE = 0x8,
};

enum Command_e {
	GENCMD_DevACK = GROUP_PACK(CMDGROUP_GENERAL) | 0x0,
	GENCMD_AppACK,		// 0x01
	GENCMD_DevByeBye,
	GENCMD_AppByeBye,

	DEVCMD_ReqDevInfo = GROUP_PACK(CMDGROUP_DEVICE_INFO) | 0x1,
	DEVCMD_RetDevInfo,	// 0x12
	DEVCMD_ReqDevInfoToken,
	DEVCMD_SetDevInfo = GROUP_PACK(CMDGROUP_DEVICE_INFO) | 0x5,
	DEVCMD_RetRoleInfo,	// 0x16
	DEVCMD_ReqRoleCheck,

#ifdef SPEC_REMOTE_CONTROL
	REMCMD_IdentDev = GROUP_PACK(CMDGROUP_REMOTE_CONTROL) | 0x1,
	REMCMD_SetMFBStatus,	// 0x32, Multi-Function Button, need DevACK
	REMCMD_ReqMFBStatus,	// 0x33, need RetMFBStatus/DevACK
	REMCMD_RetMFBStatus,
#endif

	DFUCMD_ReqVer = GROUP_PACK(CMDGROUP_OTADFU) | 0x1,	// 0x41, need RetVer
	DFUCMD_RetVer,		// 0x42
	DFUCMD_ReqDfuStart,	// 0x43, need DevACK
	DFUCMD_SetDfuData,	// 0x44, need DevACK
	DFUCMD_NotifyDfuStatusChange,	// 0x45, device should report whenever changed
	DFUCMD_NotifySecStart,	// 0x46, need DevACK
	DFUCMD_NotifyDfuCancel,	// 0x47, need DevACK
	DFUCMD_ReqDfuResume,	// 0x48, need RetDfuResume
	DFUCMD_RetDfuResume,

	DFUCMD_ReqDfuVer,	// 0x4a, need RetDfuVer
	DFUCMD_RetDfuVer,	// 0x4b, 3bytes, big endian
	DFUCMD_RetImageCRCChecked,	// 0x4c, 0x1 ready, 0x2 crc error, 0x3 unknown error
	DFUCMD_AppTriggerUpgrade,	// 0x4d, Device should reset

#ifdef LED_PULSE_2
	LEDCMD_ReqLedPatternInfo =
	    GROUP_PACK(CMDGROUP_LED_PATTERN_CONTROL) | 0x1,
	LEDCMD_RetLedPatternInfo,	// 0x52
	LEDCMD_SetLedPattern,	// 0x53, need DevACK
	LEDCMD_NotifyInsLelChange,
	LEDCMD_NotifyLedPattern,
	LEDCMD_SetBrightness,
	LEDCMD_RetColorSniffer,
#elif LED_PULSE_3
	LEDCMD_ReqLedPatternInfo =
	    GROUP_PACK(CMDGROUP_LED_PATTERN_CONTROL) | 0x1,
	LEDCMD_RetLedPatternInfo,
	LEDCMD_SetLedPattern,
	LEDCMD_NotifyLedPattern,
	LEDCMD_SetPatternBrightness,	// 0x55
	LEDCMD_ReqPatternBrightness,
	LEDCMD_RetPatternBrightness,
	LEDCMD_NotifyPatternBrightness,
#endif

#ifdef SPEC_EQ_BOOMBOX
	SPKCMD_ReqEQMode = GROUP_PACK(CMDGROUP_SPEAKER_SETTINGS) | 0x1,
	SPKCMD_RetEQMode,	// 0x62
	SPKCMD_SetEQMode,	// 0x63, 0 Indoor, 1 Outdoor, need DevACK
	SPKCMD_NotifyEQChangeMode,	// 0x64, device should send this cmd unaskedly when EQMode changed
#endif

	SPKCMD_ReqFeedbackToneStatus = 0x65,
	SPKCMD_RetFeedbackToneStatus = 0x66,
	SPKCMD_SetFeedbackTone,	// 0x67, 0 off, 1 on, need DevACK
	SPKCMD_ReqHFPStatus, //0x68
	SPKCMD_RetHFPStatus,	// 0x69
	SPKCMD_SetLinkMode,	// 0x6a, 0x0 or 0x1
	SPKCMD_RetLinkMode,	// 0x6b, 0x0 or 0x1

#ifdef SPEC_EQ_GENERAL
	SPKCMD_ReqEQ = 0x6C,		// 0x6c, need RetEQ
	SPKCMD_RetEQ,
	SPKCMD_SetCurrentEQ,	// 0x6e, need RetEQ
#endif

	SPKCMD_SetHFPMode = 0x70,	// 0x70, 0 off, 1 on, need DevACK. The value is a spec problem!

	APICMD_ReqLightStatus = 0x71,	// 0x71
	APICMD_RetLightStatus,	// 0x72
	APICMD_SetLightStatus,	// 0x73, need DevACK

	APICMD_ReqImageStart,	// 0x74, need DevACK
	APICMD_SetImageData,	// 0x75, need DevACK

	APICMD_SetBassVolume,	// 0x76, need DevACK
	APICMD_ReqBassVolume,	// 0x77, need RetBassVolume
	APICMD_RetBassVolume,	// 0x78, 1~21

	APICMD_ReqWaterOverHeating = 0x79,	// 0x79
	APICMD_RetWaterOverHeating = 0x80,	// 0x80, bit[0] whether water in charging port, bit[1] whether charging port overheating

	APICMD_ReqSerialNumber = 0x7a,	// 0x7a
	APICMD_RetSerialNumber,	// 0x7b, max 64 bytes

#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
	APICMD_SetOneTouchMusicButton = 0x7C,	// 0x7c
	APICMD_ReqOneTouchMusicButton,	// 0x7d
	APICMD_RetOneTouchMusicButton,	// 0x7e
	APICMD_RetOneTouchMusicTriggered,	// 0x7f
#endif

#if 0
	APICMD_ReqAnalyticsData_deprecated = 0x81,    // 0x81, deprecated
	APICMD_RetAnalyticsData_deprecated,   // 0x82, deprecated
#endif

#ifdef SPEC_LED_PATTERN_CONTROL
	LEDPCMD_ReqLedPackageInfo = GROUP_PACK(CMDGROUP_LED_PACKAGE) | 0x3,
	LEDPCMD_RetLedPackageInfo,	// 0x84
	LEDPCMD_SetLedPackage,	// 0x85, need NotifyLedPackageInfo
	LEDPCMD_SetLedCanvasPackage,	// 0x86, need NotifyLedPackageInfo, 2bytes big endian
	LEDPCMD_NotifyLedPackageInfo,	// 0x87, payload differs from normal and canvas package
	LEDPCMD_NotifyLedPatternInfo,	// 0x88, payload differs
	LEDPCMD_EnableNotifyLedPatternInfo,	// 0x89, 0 disable, 1 enable NotifyLedPatternInfo
	LEDPCMD_SetLedBrightness,	// 0x8A, need RetLedBrightness
	LEDPCMD_ReqLedBrightness,	// 0x8B
	LEDPCMD_RetLedBrightness,	// 0x8C
	LEDPCMD_SetLedMovementSpeed,	// 0x8D, also named SetLedDynamicMovement, need RetLedDynamicMovement
	LEDPCMD_ReqLedMovementSpeed,	// 0x8E, also named ReqLedDynamicMovement
	LEDPCMD_RetLedMovementSpeed,	// 0x8F, also named RetLedDynamicMovement
	LEDPCMD_SwitchLedPackage = 0x90,	// 0x90, need NotifyLedPackageInfo
	LEDPCMD_PreviewPattern,	// 0x91, need NotifyLedPackageInfo
	LEDPCMD_PreviewCanvasPattern,	// 0x92, need NotifyLedPackageInfo, 2bytes big endian(0x01D1)
#endif

	APICMD_ReqAnalyticsData = 0x93,	// 0x93
	APICMD_RetAnalyticsCmd,	// 0x94, PayloadLen in 2 bytes, big endian, AppACK
	APICMD_ReqPlayAnalyticsData,	// 0x95
	APICMD_RetPlayAnalyticsCmd,	// 0x96, PayloadLen in 2 bytes, big endian, AppACK

	CMD_NTI_EQ_Set = 0x97,	//need RetEQs
	CMD_NTI_EQ_Req,	//need RetEQs
	CMD_NTI_EQ_Ret,

	APICMD_SetStandaloneMode = 0x9A,
	APICMD_ReqStandaloneMode,
	APICMD_RetStandaloneMode,	// 0x9c, 0x0 off, 0x1 on

	CMD_ReqBatteryStatus = 0x9D,
	CMD_RetBatteryStatus = 0x9E,

	CMD_SetAuracastGroup = 0xA0,
	CMD_ReqAuracastGroup = 0xA1,
	CMD_RetAuracastGroup = 0xA2,

	CMD_SetLeAudioStatus = 0xA3,	//0-off, 1-on
	CMD_ReqLeAudioStatus = 0xA4,
	CMD_RetLeAudioStatus = 0xA5,	//0-off, 1-on

	CMD_RetPlayerInfo = 0xA6,
	CMD_SetPlayerInfo = 0xA7,
	CMD_ReqPlayerInfo = 0xA8,

	CMD_UNSUPPORTED = 0xEE,
};
// - - -

/* - - - Group DeviceInfo - - - */
enum BatteryStatusFeatureKeyId {
	BSFKI_All = 0x00,	// 0x00 None This field is defined to fetch value of all features.
	BSFKI_BatteryID,	// 0x01 Max to 16 bytes ASCII.
	BSFKI_Remainingplaytime,	// 0x02 2 bytes, big endian 0 ~ 65535 minutes
	BSFKI_TemperatureMax,	// 0x03 2 bytes, big endian Celsius degree X 10
	BSFKI_RemainingCapacity,	// 0x04 2 bytes, big endian 0 ~ 65536 mAh
	BSFKI_FullChargeCapacity,	// 0x05 2 bytes, big endian 0 ~ 65536 mAh
	BSFKI_DesignCapacity,	// 0x06 2 bytes, big endian 0 ~ 65536 mAh
	BSFKI_CycleCount,	// 0x07 2 bytes, big endian 0 ~ 65536 cycles
	BSFKI_StateOfHealth,	// 0x08 1 byte 0 ~ 100 %
	BSFKI_ChargingStatus,	// 0x09 1 byte 0x01: AC charging.  0x02: DC.  0x03: full charged.  0x04: full depleted.
	BSFKI_BatteryhealthNotification,	// 0x0A 1 byte 0x00: Health 0x01: Low Battery Health
	BSFKI_TotalPowerOnDuration,	// 0x0B 4 bytes, big endian Total power on duration in minutes.
	BSFKI_TotalPlaybackTimeDuration,	// 0x0C 4 btes, big endian Total playback duration in minutes.
	BSFKI_NUM,
};

#if 0
static u8_t BatterStatusKeyLengthTable[BSFKI_NUM] = {
	0, 16, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 4, 4,
};
#endif

// - - -

enum AuracastGroupInformationKey {
	AGIK_GroupAction = 0x01,
	AGIK_GroupType,
	AGIK_Role,
	AGIK_StereoChannel,
	AGIK_StereoGroupId,
	AGIK_StereoGroupName,
	AGIK_PartnerMac,
	AGIK_NUM,
};

enum PlayerInfoFeatureKey {
	PIFK_PlayPause = 0x01,
	PIFK_PrevNext = 0x02,
	PIFK_VolumeLevel = 0x03,
};

enum AGI_stereo_channel {
	AGI_STRERO_CHANNEL_FULL_CHANNEL = 0x0,
	AGI_STRERO_CHANNEL_LEFT_CHANNEL,
	AGI_STRERO_CHANNEL_RIGHT_CHANNEL,
};

#endif
