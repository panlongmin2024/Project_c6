#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include "selfapp_config.h"
#include <app_ui.h>
#include <mem_manager.h>
#include <property_manager.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#include "broadcast.h"
#include <media_player.h>
#include <fw_version.h>
#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

enum DeviceInfo_TokenID_e {
	TokenID_DeviceName = 0xc1,	// RW dynamic 32 bytes at most
	TokenID_Serial_Number = 0xc2,	// R dynamic 64 bytes at most
	TokenID_ProductID = 0x42,	// RW 2bytes
	TokenID_ModelID = 0x43,	// RW 1byte
	TokenID_BatteryStatus = 0x44,	// R  1byte, bit[7] charging or not, bit[6-0] battery level 0-100%
	TokenID_ActiveChannel = 0x46,	// RW 1byte, 0 stereo, 1 left, 2 right
	TokenID_AudioSource = 0x47,	// R  1byte, 0 no audio playing, 1 a2dp, 2 Aux-in, 3 receiver playing
	TokenID_MACAddress = 0x48,	// R  6bytes
	TokenID_StereoLock = 0x49,	// RW 1byte, 0 not locked, 1 locked
	TokenID_BTAddressCRC16 = 0x4a,	// R  2bytes
	TokenID_WaterInChargingPort = 0x4b,	// R 1byte, 0 no, 1 there is water in charging port
	TokenID_SupportAuracast = 0x4c,	// R 1byte, 0-doesn't support Auracast, 1-support Auracast
	TokenID_Firmware_Version = 0x4d,	// R 4byte
	TokenID_lasting_Stereo_Connected = 0x4f,	// R 1byte, 0-normal, 1-stereo
};

u8_t cmdgroup_special(u8_t CmdID, u8_t * Payload, u16_t PayloadLen, int *result)
{
	u8_t done = 0;
	int ret = 0;

	switch (CmdID) {
	case SPKCMD_SetHFPMode:	// 0x70
	case APICMD_ReqSerialNumber:	// 0x7a
#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
	case APICMD_SetOneTouchMusicButton:	// 0x7c
	case APICMD_ReqOneTouchMusicButton:	// 0x7d
#endif
	case CMD_NTI_EQ_Set:
	case CMD_NTI_EQ_Req:
	case APICMD_ReqStandaloneMode:
	case CMD_ReqBatteryStatus:
	case CMD_SetAuracastGroup:
	case CMD_ReqAuracastGroup:
	case CMD_SetLeAudioStatus:
	case CMD_ReqLeAudioStatus:
	case CMD_ReqPlayerInfo:
	case CMD_SetPlayerInfo:
	case APICMD_ReqLightStatus:
	case APICMD_SetLightStatus:
	case APICMD_ReqImageStart:
	case APICMD_SetImageData:
	case APICMD_ReqBassVolume:
	case APICMD_SetBassVolume:
	case APICMD_ReqWaterOverHeating:
	case CMD_ReqAuracast_SQ_Status:
	case CMD_SetAuracast_SQ_Status:
		{
			ret =
			    cmdgroup_speaker_settings(CmdID, Payload,
						      PayloadLen);
			done = 1;
			break;
		}


#ifdef CONFIG_DATA_ANALY
	case APICMD_ReqAnalyticsData:	// 0x93
	case APICMD_ReqPlayAnalyticsData:	// 0x95
		{
			ret = cmdgroup_analytics(CmdID, Payload, PayloadLen);
			done = 1;
			break;
		}
#endif

#ifdef SPEC_LED_PATTERN_CONTROL
	case LEDPCMD_SwitchLedPackage:	// 0x90
	case LEDPCMD_PreviewPattern:	// 0x91
	case LEDPCMD_PreviewCanvasPattern:	// 0x92
		{
			ret = cmdgroup_led_package(CmdID, Payload, PayloadLen);
			done = 1;
			break;
		}
#endif
	}

	if (done && result) {
		*result = ret;
	}

	return done;
}

int cmdgroup_general(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	int ret = -1;

	switch (CmdID) {
	case GENCMD_DevACK:
		selfapp_log_inf("DevAck\n");
		ret = 0;
		break;
	case GENCMD_DevByeBye:
		selfapp_log_inf("DevByeBye\n");
		ret = 0;
		break;
	case GENCMD_AppACK:
		selfapp_log_inf("AppAck 0x%x %d", Payload[0], Payload[1]);
		if (PayloadLen != 2) {
			selfapp_log_wrn("Wrong AppAck\n");
		}
		switch (Payload[0])
		{
		case APICMD_RetAnalyticsCmd:
			if (Payload[1] == 0) {
				selfapp_log_inf("reset Analytics\n");
#ifdef CONFIG_DATA_ANALY
				data_analy_data_clear();
#endif
			} else {
				selfapp_log_wrn("Fail\n");
			}
			break;
		case APICMD_RetPlayAnalyticsCmd:
			if (Payload[1] == 0) {
				selfapp_log_inf("reset play Analytics");
#ifdef CONFIG_DATA_ANALY
				data_analy_play_clear();
#endif
			} else {
				selfapp_log_wrn("Fail\n");
			}
			break;
		default:
			selfapp_log_wrn("Unknow AppACK");
			break;
		}
		ret = 0;
		break;
	case GENCMD_AppByeBye:
		selfapp_log_inf("AppByeBye\n");
		ret = 0;
		break;
	default:
		selfapp_log_wrn("Unknow command 0x%x\n", CmdID);
		break;
	}

	return ret;
}

static int devinfo_set_forward_token(u8_t id, u8_t *value, u16_t *token_len)
{
	u16_t paylen = 0;
	int ret = -1;

	switch (id) {
	case TokenID_DeviceName:
		selfapp_log_inf("Device name:");
		// dynamic token len
		paylen = 1 + 1 + value[0];
		break;

	case TokenID_ActiveChannel:
		//TODO: Set channel output like auracast stereo channel.
		selfapp_log_inf("ActiveChan=0x%x\n", value[0]);
		paylen = 1 + 1;
		ret = 0;
		break;

	case TokenID_StereoLock:
		selfapp_log_inf("StereoLock=0x%x", value[0]);
		paylen = 1 + 1;
		ret = 0;
		break;

	case TokenID_ProductID:
	case TokenID_ModelID:
	case TokenID_BatteryStatus:
	case TokenID_AudioSource:
	case TokenID_MACAddress:
	case TokenID_BTAddressCRC16:
	case TokenID_WaterInChargingPort:
	case TokenID_SupportAuracast:
		selfapp_log_wrn("token 0x%x can not be set.", id);
		if (id == TokenID_BTAddressCRC16 || id == TokenID_ProductID) {
			paylen = 1 + 2;
		} else if (id == TokenID_MACAddress) {
			paylen = 1 + 6;
		} else {
			paylen = 1 + 1;
		}
		ret = 0;
		break;

	default:
		selfapp_log_wrn("unknow token 0x%x", id);
		break;
	}

	if(paylen > 0 ) {
		//TODO: forward token data
		//forwad_token(id, value, paylen);
	}

	*token_len = paylen;
	return ret;
}

static int devinfo_set_handle_token(u8_t id, u8_t *value, u16_t *token_len)
{
	u16_t paylen = 0;
	int ret = -1;
#ifdef CONFIG_BT_LETWS
	struct AURACAST_GROUP group;
#endif

	switch (id) {
	case TokenID_DeviceName:
		selfapp_log_inf("Device name:");
		if(value[0]> 0) {
			selfapp_log_dump(value + 1, value[0]);
			ret = bt_manager_bt_name_update(value + 1, value[0]);
		} else {
			selfapp_log_wrn("NULL name");
		}
		// dynamic token len
		paylen = 1 + 1 + value[0];
		break;

	case TokenID_ActiveChannel:
		selfapp_log_inf("ActiveChan=0x%x\n", value[0]);
#ifdef CONFIG_BT_LETWS
		selfapp_config_get_ac_group(&group);
		if(value[0] != group.ch){
			group.ch = value[0];
			selfapp_config_set_ac_group(&group);
			if(group.ch){
				group.ch = group.ch == 1 ? 2 : 1;
			}
			selfapp_set_lasting_stereo_group_info_to_slave(&group);
			spk_ret_auracast_group(NULL,0);
			selfapp_reset_player();
		}else{
			selfapp_log_inf("same channel\n");
		}
#endif
		paylen = 1 + 1;
		ret = 0;
		break;

	case TokenID_StereoLock:
		selfapp_log_inf("StereoLock=0x%x", value[0]);
		paylen = 1 + 1;
		ret = 0;
		break;

	case TokenID_ProductID:
	case TokenID_ModelID:
	case TokenID_BatteryStatus:
	case TokenID_AudioSource:
	case TokenID_MACAddress:
	case TokenID_BTAddressCRC16:
	case TokenID_WaterInChargingPort:
	case TokenID_SupportAuracast:
		selfapp_log_wrn("token 0x%x can not be set.", id);
		if (id == TokenID_BTAddressCRC16 || id == TokenID_ProductID) {
			paylen = 1 + 2;
		} else if (id == TokenID_MACAddress) {
			paylen = 1 + 6;
		} else {
			paylen = 1 + 1;
		}
		ret = 0;
		break;

	default:
		selfapp_log_wrn("unknow token 0x%x", id);
		break;
	}

	*token_len = paylen;
	return ret;
}

static u16_t devinfo_pack_fixed_token(u8_t * buf, u8_t id, u32_t val, u8_t len, u8_t device_id)
{
	u16_t size = 0;
	size += selfapp_pack_header(buf, DEVCMD_RetDevInfo, 1+1+len);
	buf[size++] = device_id;
	buf[size++] = id;
	selfapp_pack_int(buf+size, val, len);
	size += len;

	return size;
}
static u16_t devinfo_pack_token(u8_t * buf, u8_t tokenid, u8_t device_id)
{
	u16_t sendlen = 0;
	u16_t size = 0;
	selfapp_context_t *selfctx = self_get_context();

	if (NULL == selfctx) {
		return -EINVAL;
	}
	switch (tokenid) {
	case TokenID_DeviceName:
	{
		u16_t size = 0;
		u8_t len;
		char *name = mem_malloc(SELF_BTNAME_LEN + 1);
		if(NULL == name) {
			selfapp_log_err("malloc fails");
			break;
		}

		memset(name, 0, SELF_BTNAME_LEN + 1);
		if(!device_id){
#ifdef CONFIG_PROPERTY
			property_get(CFG_APP_NAME, name, SELF_BTNAME_LEN);
#endif
		}else{
			if(selfctx->secondary_device.validate && strlen(selfctx->secondary_device.bt_name)){
				memcpy(name, selfctx->secondary_device.bt_name, strlen(selfctx->secondary_device.bt_name));
				name[SELF_BTNAME_LEN] = '\0';
			}
		}
		len = strlen(name);
		size += selfapp_pack_header(buf, DEVCMD_RetDevInfo, 1+1+1+len);
		buf[size++] = device_id;
		buf[size++] = tokenid;
		buf[size++] = len;
		size += selfapp_pack_bytes(buf+size, name, len);
		sendlen = size;

		mem_free(name);
		break;
	}
	case TokenID_Serial_Number:
	{
		u16_t size = 0;
		u8_t len;
		//64 bytes at most
		char *serial_num = mem_malloc(SELF_SN_LEN + 1);
		if(NULL == serial_num) {
			selfapp_log_err("malloc fails");
			break;
		}
		memset(serial_num, 0, SELF_SN_LEN + 1);
		if(!device_id){
#ifdef CONFIG_PROPERTY
			len = property_get(CFG_ATS_DSN_ID, serial_num, SELF_SN_LEN);
#endif
						if (len <= 0 || len > SELF_SN_LEN) {
				memcpy(serial_num, SELF_DEFAULT_SN, SELF_DEFAULT_SN_LEN);
				// len = SELF_DEFAULT_SN_LEN;
			}
			len = strlen(serial_num);
		}else{
			if(selfctx->secondary_device.validate && strlen(selfctx->secondary_device.serial_num)){
				memcpy(serial_num, selfctx->secondary_device.serial_num, strlen(selfctx->secondary_device.serial_num));
				len = strlen(selfctx->secondary_device.serial_num);
			}else{
				memcpy(serial_num, SELF_DEFAULT_SN, SELF_DEFAULT_SN_LEN);
				len = strlen(serial_num);
			}
		}
		size += selfapp_pack_header(buf, DEVCMD_RetDevInfo, 1+1+1+len);
		buf[size++] = device_id;
		buf[size++] = tokenid;
		buf[size++] = len;
		size += selfapp_pack_bytes(buf+size, serial_num, len);
		sendlen = size;

		mem_free(serial_num);
		break;
	}
	case TokenID_ProductID:
		sendlen = devinfo_pack_fixed_token(buf, tokenid, BOX_PRODUCT_ID, 2, device_id);
		break;

	case TokenID_ModelID:
		if(device_id){
			selfapp_log_err("unsupport right now");
		}
		sendlen = devinfo_pack_fixed_token(buf, tokenid, selfapp_get_model_id(), 1, device_id);
		break;

	case TokenID_BatteryStatus:
	{
		u8_t value;
		if(!device_id){
			value = selfapp_get_bat_power();
		}else{
			if(selfctx->secondary_device.validate){
				value = selfctx->secondary_device.bat_status;
			}else{
				value = selfapp_get_bat_power();
			}
		}

		selfapp_log_inf("BAT=0x%x %d", value,device_id);

		sendlen = devinfo_pack_fixed_token(buf, tokenid, value, 1, device_id);
		break;
	}

	case TokenID_ActiveChannel:
	{
		u8_t value;

		// 0x0 stereo, 0x1 left, 0x2 right
		value = selfapp_get_channel();
		if(!device_id){
			if(!selfapp_get_lasting_stereo_mode() && selfapp_get_lasting_stereo_role()){
				value = 0;
			}
			selfapp_log_inf("ActiveChan=%d\n", value);
			sendlen = devinfo_pack_fixed_token(buf, tokenid, value, 1, device_id);
		}else{
			if(value)
				value = value == 1?2:1;
			sendlen = devinfo_pack_fixed_token(buf, tokenid, value, 1, device_id);
		}
		break;
	}

	case TokenID_AudioSource:
	{
		u8_t value;

		value = selfapp_get_audio_source();
		selfapp_log_inf("AudioSource=%d\n", value);
		sendlen = devinfo_pack_fixed_token(buf, tokenid, value, 1, device_id);
		break;
	}

	case TokenID_MACAddress:
	{
		size = 0;
		u8_t mac[6];

		if(device_id && selfapp_get_lasting_stereo_mode()){
			struct AURACAST_GROUP group;
			selfapp_config_get_ac_group(&group);
			for (uint8_t i  = 0; i < 6; i++) {
				mac[i] = group.addr[5-i];
			}
		}else{
			selfapp_get_mac(mac);
		}

		selfapp_log_inf("MAC=%02X%02X%02X-%02X%02X%02X\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

		size += selfapp_pack_header(buf, DEVCMD_RetDevInfo, 1+1+6);
		buf[size++] = device_id;
		buf[size++] = tokenid;
		size += selfapp_pack_bytes(buf+size, mac, 6);
		sendlen = size;

		break;
	}

	case TokenID_StereoLock:
		selfapp_log_inf("StereoLock=%d\n", 0);
		sendlen = devinfo_pack_fixed_token(buf, tokenid, 0, 1, device_id);
		break;

	case TokenID_BTAddressCRC16:
	{
		u16_t crc16;
		u8_t mac[6];

		if(device_id){
			selfapp_log_err("unsupport right now");
		}

		selfapp_get_mac(mac);

		crc16 = self_crc16(0, mac, 6);
		selfapp_log_inf("BTCRC16=0x%x\n", crc16);
		//Note: VIMICRO uses little endian to transfer CRC16, and we follows it.
		crc16 = (crc16 >> 8) | (crc16 << 8);
		sendlen = devinfo_pack_fixed_token(buf, tokenid, crc16, 2, device_id);
		break;
	}

	case TokenID_WaterInChargingPort:
		// TODO: check if water in charging port.
		sendlen = devinfo_pack_fixed_token(buf, tokenid, 0, 1, device_id);
		break;

	case TokenID_SupportAuracast:
		sendlen = devinfo_pack_fixed_token(buf, tokenid, 1, 1, device_id);
		break;

	case TokenID_Firmware_Version:
		size = 0;
		u8_t vercode[4];	// 3Bytes is sw version, big endian, 1Byte is hw version
		extern u32_t fw_version_get_sw_code(void);
		extern u8_t fw_version_get_hw_code(void);
		u32_t hwver = fw_version_get_hw_code();
		u32_t swver = fw_version_get_sw_code();

		if(!device_id){
			vercode[0] = hex2dec_digitpos((swver >> 16) & 0xFF);
			vercode[1] = hex2dec_digitpos((swver >>  8) & 0xFF);
			vercode[2] = hex2dec_digitpos( swver & 0xFF);
			vercode[3] = (u8_t)hwver;
		}else{
			if(selfctx->secondary_device.validate){
				memcpy(vercode, selfctx->secondary_device.firmware_version, 4);
			}
		}
		SYS_LOG_INF("version %x %x %x %d %d\n",vercode[0],vercode[1],vercode[2],vercode[3],device_id);
		size += selfapp_pack_header(buf, DEVCMD_RetDevInfo, 1+1+4);
		buf[size++] = device_id;
		buf[size++] = tokenid;
		size += selfapp_pack_bytes(buf+size, vercode, 4);
		sendlen = size;
		break;

	case TokenID_lasting_Stereo_Connected:
		sendlen = devinfo_pack_fixed_token(buf, tokenid, selfapp_get_lasting_stereo_mode(), 1, device_id);
		break;

	default:
		selfapp_log_wrn("unknow token %x\n", tokenid);
		break;
	}

	return sendlen;
}

static int devinfo_req_devinfo(void)
{
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;
	int i;

	if (buf == NULL) {
		return ret;
	}

	u8_t token_array[] = {
	    TokenID_DeviceName,
	    TokenID_Serial_Number,
	    TokenID_ProductID,
	    TokenID_ModelID,
	    TokenID_BatteryStatus,
	    TokenID_ActiveChannel,
	    TokenID_AudioSource,
	    TokenID_MACAddress,
	    TokenID_StereoLock,
	    TokenID_BTAddressCRC16,
	    TokenID_WaterInChargingPort,
	    TokenID_SupportAuracast,
	    TokenID_Firmware_Version,
#ifdef CONFIG_BT_LETWS
	    TokenID_lasting_Stereo_Connected,
#endif
	};

	for (i = 0; i < sizeof(token_array); i++) {
		sendlen = devinfo_pack_token(buf, token_array[i], 0);
		if (sendlen > 0) {
			self_send_data(buf, sendlen);
		}
	}
#ifdef CONFIG_BT_LETWS
	if(selfapp_get_lasting_stereo_mode()){
		selfapp_report_secondary_device_info();
	}
#endif

	return 0;
}

static int devinfo_req_devinfo_token(u8_t dev, u8_t token, u8_t device_id)
{
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;

	if (buf == NULL) {
		return ret;
	}

	sendlen = devinfo_pack_token(buf, token, device_id);
	if (sendlen > 0) {
		ret = self_send_data(buf, sendlen);
	}
	return ret;
}

static int devinfo_set_devinfo(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u8_t dev_idx = 0;
	u16_t offset, sendlen;
	u8_t ack;
	u16_t toklen = 0;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	ack = 0; // assume it success
	offset = 0;
	while(offset < PayloadLen) {
		// the 1st dev_idx might be neglected.
		//is token id or device id?
		if ((Payload[offset] & 0xC0) == 0) {
			// DeviceIndex token
			dev_idx = Payload[offset];
			selfapp_log_inf("device id %d\n", dev_idx);
			offset++;
			if (dev_idx != 0 && dev_idx != 1) {
				selfapp_log_wrn("wrong device id %d\n", dev_idx);
				ack = dev_idx + 1;
				break;
			}
			continue;
		}

		if (dev_idx == 0) {
			ret = devinfo_set_handle_token(Payload[offset], Payload+offset+1, &toklen);
		} else {
			ret = devinfo_set_forward_token(Payload[offset], Payload+offset+1, &toklen);
		}
		if (ret < 0) {
			ack = dev_idx + 1; // report the fail device
			selfapp_log_wrn("set token 0x%x fail", Payload[offset]);
			break;
		}
		offset += toklen;
	}

	sendlen = selfapp_pack_devack(buf, DEVCMD_SetDevInfo, ack);
	ret = self_send_data(buf, sendlen);

	return ret;
}

int cmdgroup_device_info(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;
	if (buf == NULL) {
		return ret;
	}

	switch (CmdID) {
	case DEVCMD_ReqDevInfo:
		ret = devinfo_req_devinfo();
		break;
	case DEVCMD_ReqDevInfoToken:
		ret = devinfo_req_devinfo_token(Payload[0], Payload[1], 0);
		break;
	case DEVCMD_SetDevInfo:
		ret = devinfo_set_devinfo(Payload, PayloadLen);
		break;
	case DEVCMD_ReqRoleCheck:
		{
			u8_t role = selfapp_get_role();
			selfapp_log_inf("ReqRoleCheck %d\n", role);
			sendlen +=
			    selfapp_pack_cmd_with_int(buf, DEVCMD_RetRoleInfo, role, 1);
			ret = self_send_data(buf, sendlen);
			break;
		}
	default:
		selfapp_log_wrn("NoCmd DevInfo 0x%x\n", CmdID);
		break;
	}

	return ret;
}

#ifdef SPEC_REMOTE_CONTROL
int cmdgroup_remote_control(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	selfapp_log_wrn("Not supported cmd 0x%x\n", CmdID);

	switch (CmdID) {
	case REMCMD_IdentDev:{
			u8_t dev_idx = Payload[0];
			ret = selfapp_set_indication(dev_idx);
			selfapp_log_inf("devInd %d(%d)\n", dev_idx, ret);
			break;
		}

	case REMCMD_SetMFBStatus:{
			selfapp_log_inf("setsta=%d\n", Payload[0]);
			selfapp_config_set_mfbstatus(Payload[0]);
			sendlen += selfapp_pack_devack(buf, REMCMD_SetMFBStatus, 0);
			ret = self_send_data(buf, sendlen);
			break;
		}

	case REMCMD_ReqMFBStatus:{
			u8_t mfb_status = selfapp_config_get_mfbstatus();
			selfapp_log_inf("reqsta=%d\n", mfb_status);
			sendlen +=
			    selfapp_pack_cmd_with_int(buf, REMCMD_RetMFBStatus,
						 (u32_t)mfb_status, 1);
			ret = self_send_data(buf, sendlen);
			break;
		}

	default:{
			selfapp_log_wrn("NoCmd RemoteCtrl 0x%x\n", CmdID);
			break;
		}
	}

	return ret;
}
#endif

static int skp_req_player_info(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u16_t i;
	u16_t offset = selfapp_get_header_len(CMD_ReqPlayerInfo);

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	for (i = 0; i < PayloadLen; i++) {
		switch (Payload[i]) {
		case PIFK_PlayPause:
			buf[offset++] = PIFK_PlayPause;
			buf[offset++] = 1;
			buf[offset++] = 1;	// 0-pause, 1-play
			break;
		case PIFK_PrevNext:
			buf[offset++] = PIFK_PrevNext;
			buf[offset++] = 1;
			buf[offset++] = 1;	// 0-prev, 1-next
			break;
		case PIFK_VolumeLevel:
			buf[offset++] = PIFK_VolumeLevel;
			buf[offset++] = 1;
			buf[offset++] = 15;	// 0-32
			break;
		}
	}
	selfapp_pack_header(buf, CMD_RetPlayerInfo, len);
	ret = self_send_data(buf, offset + len);

	return ret;
}

static int spk_req_standby_mode(u8_t * Payload, u16_t PayloadLen)
{
	u8_t buf[8];
	int ret = -1;
	u16_t len = 0;
	u8_t standby_mode = 0;

#ifdef CONFIG_FLIP7_SUNITEC
	extern bool flip7_is_standby_mode(void);
	if (flip7_is_standby_mode()) {
		standby_mode = 1;
	}
#endif
	selfapp_log_inf("standby mode: %d\n", standby_mode);
	len = selfapp_pack_cmd_with_int(buf, APICMD_RetStandaloneMode, standby_mode, 1);
	ret = self_send_data(buf, len);

	return ret;
}

static int spk_req_battery_status(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u16_t i;
	u16_t offset = selfapp_get_header_len(CMD_RetBatteryStatus);

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	for (i = 0; i < PayloadLen; i++) {
		switch (Payload[i]) {
		case BSFKI_All:
			selfapp_log_wrn("skip feature %d", Payload[i]);
			break;
		case BSFKI_TotalPowerOnDuration:
			{
				u32_t duration;

#ifdef CONFIG_DATA_ANALY
				duration = data_analy_get_product_history_pwr_on_min();
#else
				duration = 70;
#endif
				buf[offset + len] = BSFKI_TotalPowerOnDuration;
				len += 1;
				buf[offset + len] = 4;
				len += 1;
				buf[offset + len] = (duration >> 24) & 0xFF;
				buf[offset + len + 1] = (duration >> 16) & 0xFF;
				buf[offset + len + 2] = (duration >> 8) & 0xFF;
				buf[offset + len + 3] = (duration) & 0xFF;
				len += 4;
				break;
			}
		case BSFKI_TotalPlaybackTimeDuration:
			{
				u32_t duration;

#ifdef CONFIG_DATA_ANALY
				duration = data_analy_get_product_history_play_min();
#else
				duration = 30;
#endif
				buf[offset + len] =
				    BSFKI_TotalPlaybackTimeDuration;
				len += 1;
				buf[offset + len] = 4;
				len += 1;
				buf[offset + len] = (duration >> 24) & 0xFF;
				buf[offset + len + 1] = (duration >> 16) & 0xFF;
				buf[offset + len + 2] = (duration >> 8) & 0xFF;
				buf[offset + len + 3] = (duration) & 0xFF;
				len += 4;
				break;
			}
		default:
			selfapp_log_wrn("skip feature %d", Payload[i]);
			break;
		}
	}

	if (len > 0) {
		selfapp_pack_header(buf, CMD_RetBatteryStatus, len);
	} else {
		offset = selfapp_pack_unsupported(buf, CMD_ReqBatteryStatus);
	}
	ret = self_send_data(buf, offset + len);

	return ret;
}

static int spk_req_auracast_sq_mode(void)
{
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u8_t sq_mode = 0;

	if (buf == NULL) {
		return ret;
	}

	sq_mode = broadcast_get_sq_mode();

	selfapp_log_inf("%d", sq_mode);
	len =
	    selfapp_pack_cmd_with_int(buf, CMD_RetAuracast_SQ_Status, sq_mode,
				 1);
	ret = self_send_data(buf, len);

	return ret;
}

void self_creat_group_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	selfapp_context_t *selfctx = self_get_context();
	if (NULL == selfctx) {
		return;
	}
	selfapp_log_inf("time out");
	memset(&selfctx->creat_group,0,sizeof(selfctx->creat_group));
}

int spk_set_auracast_group(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u16_t i,j;
	u8_t action = 0;
	u8_t is_lasting_stereo = 0;
	u8_t set_name = 0;
	struct AURACAST_GROUP group;
	struct AURACAST_GROUP last_group;
	bd_address_t bd_addr;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	selfapp_config_get_ac_group(&last_group);
	selfapp_config_get_ac_group(&group);

	for (i = 0; i < PayloadLen;) {
		selfapp_log_inf("id %d", Payload[i]);
		switch (Payload[i]) {
		case AGIK_GroupAction:
			if (1 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong val length.");
				break;
			}
			action = Payload[i + 2];
			selfapp_log_inf("action %d 0x%x", action,last_group.id);
			if (action == 1) {
				//creat group
			} else if (action == 2) {
				//destroy group
				memset(&group, 0, sizeof(struct AURACAST_GROUP));
			} else if (action == 3) {
				//TODO: Prepare to create stereo group,play tone.
				selfapp_log_wrn("Prepare.");
			}
			i += 3;
			break;
		case AGIK_GroupType:
			if (1 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong val length.");
				break;
			}
			group.mode = Payload[i + 2];
			i += 3;
			break;
		case AGIK_Role:
			if (1 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong val length.");
				break;
			}
			group.role = Payload[i + 2];
			selfapp_log_inf("group.role %d.",group.role);
			i += 3;
			break;
		case AGIK_StereoChannel:
			if (1 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong val length.");
				break;
			}
			group.ch = Payload[i + 2];
			i += 3;
			break;
		case AGIK_StereoGroupId:
			if (4 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong cmd parameter.\n");
				break;
			}
			group.id = Payload[i + 2] << 24 |
			    Payload[i + 3] << 16 |
			    Payload[i + 4] << 8 | Payload[i + 5];
			i += 6;
			break;
		case AGIK_StereoGroupName:
			if (Payload[i + 1] > 30) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong cmd parameter.\n");
				break;
			}
			set_name = 1;
			memset(group.name,0,sizeof(group.name));
			memcpy(group.name,&Payload[i + 2],Payload[i + 1]);
			i += Payload[i + 1] + 2;
			break;
		case AGIK_PartnerMac:
			if (6 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong cmd parameter.\n");
				break;
			}
			memcpy(bd_addr.val,&Payload[i + 2],Payload[i + 1]);
			for (j = 0; j < 6; j++) {
				group.addr[j] = bd_addr.val[5-j];
			}
			is_lasting_stereo = 1;
			i += 8;
			break;
		case AGIK_CreatGroupTimeOut:
			if (1 != Payload[i + 1]) {
				i += Payload[i + 1] + 2;
				selfapp_log_wrn("Wrong val length.");
				break;
			}
			selfctx->creat_group_time_out = Payload[i + 2];
			selfapp_log_inf("time out %d.",selfctx->creat_group_time_out);
			i += 3;
			break;
		default:
			i += Payload[i + 1] + 2;
			break;
		}
	}

	len = selfapp_pack_header(buf, CMD_RetAuracastGroup, PayloadLen);
	if(action == 1){
#ifdef CONFIG_BT_LETWS
		if(set_name){
			memcpy(buf + len, Payload, PayloadLen);
		}else{
			memcpy(buf + len, Payload, PayloadLen);
			buf[len + PayloadLen++] = AGIK_StereoGroupName;
			buf[len + PayloadLen++] = strlen(group.name);
			memcpy(&buf[len + PayloadLen],group.name,strlen(group.name));
			PayloadLen += strlen(group.name);
			selfapp_pack_header(buf, CMD_RetAuracastGroup, PayloadLen);
		}
#else
		memcpy(buf + len, Payload, PayloadLen);
#endif
		if(is_lasting_stereo){
			if(group.id != last_group.id){
				memcpy(&selfctx->creat_group,&group,sizeof(group));
				selfapp_log_inf("creat group 0x%x", group.id);
				selfapp_switch_lasting_stereo_mode(1);
				ret = 0;
				if(!selfctx->creat_group_time_out)
					selfctx->creat_group_time_out = 20;
				selfapp_cmd_thread_timer_start(&selfctx->creat_group_timer);
			}else{
				selfapp_log_inf("set group param %s %d", group.name,group.ch);
				selfapp_config_set_ac_group(&group);
				if(group.ch){
					group.ch = group.ch == 1 ? 2 : 1;
				}
				selfapp_set_lasting_stereo_group_info_to_slave(&group);
				ret = self_send_data(buf, len + PayloadLen);
			}
		}else{
			selfapp_config_set_ac_group(&group);
			ret = self_send_data(buf, len + PayloadLen);
		}
	}else if(action == 2){
		memcpy(buf + len, Payload, PayloadLen);
		//destory group cmd also set a lot param,clear them
		memset(&group, 0, sizeof(struct AURACAST_GROUP));
		memcpy(group.name,DEDAULT_STEREO_GROUP_NAME,strlen(DEDAULT_STEREO_GROUP_NAME));
		selfapp_config_set_ac_group(&group);
		//ret = self_send_data(buf, len + PayloadLen);
		ret = spk_ret_auracast_group(NULL,0);
		if(last_group.role){
			selfapp_set_lasting_stereo_group_info_to_slave(&group);
			selfapp_switch_lasting_stereo_mode(0);
		}
	}

	return ret;
}

int spk_ret_auracast_group(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t buf[100];
	int ret = -1;
	u16_t len = 0;
	u16_t hdr_len = selfapp_get_header_len(CMD_RetAuracastGroup);
	u8_t *data = buf;
	struct AURACAST_GROUP group;

	if (NULL == selfctx) {
		return ret;
	}

	selfapp_config_get_ac_group(&group);
	len = 0;
	data += hdr_len;

	data[len++] = AGIK_GroupAction;
	data[len++] = 0x01;
	if (0 == group.ch) {
		data[len++] = 0x02;	//destroy group
	} else {
		data[len++] = 0x01;	//create group
	}

	data[len++] = AGIK_GroupType;
	data[len++] = 0x01;
	data[len++] = 0x01;

	data[len++] = AGIK_Role;
	data[len++] = 0x01;
	data[len++] = group.role;

	data[len++] = AGIK_StereoChannel;
	data[len++] = 0x01;
	if(!selfapp_get_lasting_stereo_mode() && group.role){
		data[len++] = 0;
	}else{
		data[len++] = group.ch;
	}

	// stereo Group ID
	data[len++] = AGIK_StereoGroupId;
	data[len++] = 0x04;
	data[len++] = (group.id >> 24 & 0xFF);
	data[len++] = (group.id >> 16 & 0xFF);
	data[len++] = (group.id >> 8 & 0xFF);
	data[len++] = (group.id & 0xFF);

#ifdef CONFIG_BT_LETWS
	// stereo Group name
	data[len++] = AGIK_StereoGroupName;
	data[len++] = strlen(group.name);
	memcpy(&data[len],group.name,strlen(group.name));
	len += strlen(group.name);
#endif

	// mac address
	if(group.role){
		data[len++] = AGIK_PartnerMac;
		data[len++] = 6;
		for (uint8_t i  = 0; i < 6; i++) {
			data[len + i] = group.addr[5-i];
		}
		len += 6;
	}

	selfapp_pack_header(buf, CMD_RetAuracastGroup, len);
	ret = self_send_data(buf, hdr_len + len);

	return ret;
}

int spk_set_feedback_tone(u8_t *payload, u16_t payload_len)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	selfapp_log_inf("feedback=%d", payload[0]);
	selfapp_set_feedback_tone(payload[0]);

	len = selfapp_pack_devack(buf, SPKCMD_SetFeedbackTone, 0);
	ret = self_send_data(buf, len);

	return ret;
}

int spk_req_feedback_tone_status(u8_t * Payload, u16_t PayloadLen)
{
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u8_t tonestatus = 0;

	if (buf == NULL) {
		return ret;
	}

	tonestatus = selfapp_get_feedback_tone();
	selfapp_log_inf("spk reqFeedback=%d\n", tonestatus);
	len =
	    selfapp_pack_cmd_with_int(buf, SPKCMD_RetFeedbackToneStatus, tonestatus,
				 1);
	ret = self_send_data(buf, len);

	return ret;
}

static int spk_req_sn(u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	int ret = -1;
	u16_t len = 0;
	u16_t sendlen = 0;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	sendlen = 3;

#ifdef CONFIG_PROPERTY
	len = property_get(CFG_ATS_DSN_ID, buf + sendlen, SELF_SN_LEN);
#endif
		if (len <= 0 || len > SELF_SN_LEN) {
		memcpy(buf + 3, SELF_DEFAULT_SN, SELF_DEFAULT_SN_LEN);
		len = SELF_DEFAULT_SN_LEN;
	}
	selfapp_pack_header(buf, APICMD_RetSerialNumber, (u16_t) len);
	sendlen += len;

	ret = self_send_data(buf, sendlen);

	return ret;
}

void lea_status_update_cb(int err){
	selfapp_log_inf("ret %d\n", err);
	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_LEAUDIO_STATUS_UPDATE, 0, 0);
}

int cmdgroup_speaker_settings(u8_t CmdID, u8_t * Payload, u16_t PayloadLen)
{
	selfapp_context_t *selfctx = self_get_context();
	u8_t *buf = self_get_sendbuf();
	u16_t sendlen = 0;
	int ret = -1;

	if (buf == NULL) {
		return ret;
	}

	if (NULL == selfctx) {
		return ret;
	}

	switch (CmdID) {
#ifdef SPEC_EQ_BOOMBOX
	case SPKCMD_ReqEQMode:
	case SPKCMD_SetEQMode:
		//Boombox EQ mode
		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		break;
#endif
	case SPKCMD_ReqFeedbackToneStatus:
		ret = spk_req_feedback_tone_status(Payload, PayloadLen);
		break;
	case SPKCMD_SetFeedbackTone:
		ret = spk_set_feedback_tone(Payload, PayloadLen);
		break;
	case SPKCMD_ReqHFPStatus:
	case SPKCMD_SetHFPMode:
		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		break;

	case SPKCMD_SetLinkMode:
		sendlen = selfapp_pack_header(buf, SPKCMD_RetLinkMode, 1);
		buf[sendlen] = Payload[0];
		sendlen += 1;
		ret = self_send_data(buf, sendlen);

		selfapp_set_link_mode(Payload[0]);
		break;

	case APICMD_ReqSerialNumber:
		ret = spk_req_sn(Payload, PayloadLen);
		break;
#ifdef SPEC_ONE_TOUCH_MUSIC_BUTTON
	case APICMD_SetOneTouchMusicButton:	//0x7c
	case APICMD_ReqOneTouchMusicButton:	//0x7d
	{
		u16_t button;

		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		if (CmdID == APICMD_SetOneTouchMusicButton && Payload) {
			selfapp_config_set_one_touch_music_button(Payload[0]<<8|Payload[1]);
		}
		button = selfapp_config_get_one_touch_music_button();
		selfapp_log_inf("onetouch: action=%d, id=%d\n", button>>8, button&0xff);
		sendlen = selfapp_pack_cmd_with_int(buf, APICMD_RetOneTouchMusicButton,
					     button, 2);
		ret = self_send_data(buf, sendlen);
		break;
	}
#endif
	case CMD_NTI_EQ_Set:
	case CMD_NTI_EQ_Req:
		if (CmdID == CMD_NTI_EQ_Set) {
			spkeq_SetNTIEQ(Payload, PayloadLen);
		}
		sendlen = spkeq_RetNTIEQ(buf, EQCATEGORY_CUSTOM_2);
		ret = self_send_data(buf, sendlen);
		break;
	case APICMD_ReqStandaloneMode:
		ret = spk_req_standby_mode(Payload, PayloadLen);
		break;
	case CMD_ReqBatteryStatus:
		ret = spk_req_battery_status(Payload, PayloadLen);
		break;
	case CMD_SetAuracastGroup:
		ret = spk_set_auracast_group(Payload, PayloadLen);
		break;
	case CMD_ReqAuracastGroup:
		ret = spk_ret_auracast_group(Payload, PayloadLen);
		break;
	case CMD_SetLeAudioStatus:
		if(Payload[0]){
			ret = bt_manager_audio_lea_enable(lea_status_update_cb);
		}else{
			ret = bt_manager_audio_lea_disable(lea_status_update_cb);
		}
		if(-EALREADY == ret){
			sendlen = selfapp_pack_cmd_with_int(buf, CMD_RetLeAudioStatus, selfapp_get_leaudio_status(), 1);
			ret = self_send_data(buf, sendlen);
		}
		break;
	case CMD_ReqLeAudioStatus:
		sendlen = selfapp_pack_cmd_with_int(buf, CMD_RetLeAudioStatus,selfapp_get_leaudio_status() , 1);
		ret = self_send_data(buf, sendlen);
		break;
	case CMD_ReqPlayerInfo:
		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		ret = skp_req_player_info(Payload, PayloadLen);
		break;
	case CMD_SetPlayerInfo:
		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		break;
	case CMD_SetAuracast_SQ_Status:
		if (Payload[0] == 1) {
			broadcast_set_sq_mode(true);
		} else {
			broadcast_set_sq_mode(false);
		}
	case CMD_ReqAuracast_SQ_Status:
		ret = spk_req_auracast_sq_mode();
		break;
	default:
		selfapp_log_wrn("Not implement cmd 0x%x\n", CmdID);
		sendlen = selfapp_pack_unsupported(buf, CmdID);
		ret = self_send_data(buf, sendlen);
		break;
	}

	return ret;
}

int selfapp_cmd_handler(u8_t * buf, u16_t len)
{
	int ret = -1;
	u8_t CmdID = CMD_UNSUPPORTED, CmdGroup = 0xF;
	u16_t PayloadLen = 0;
	u8_t *Payload = NULL;

#ifdef SELFAPP_DEBUG
	if (buf[1] != DFUCMD_SetDfuData) {
		selfapp_log_inf("RX: %02x %02x %02x", buf[0], buf[1], buf[2]);
		//selfapp_log_dump(buf, len > 64 ? 64 : len);
	}
#endif

	if (self_get_sendbuf() == NULL) {
		selfapp_log_wrn("not init\n");
		goto label_done;
	}

	if (selfapp_check_header(buf, len, &CmdID, &Payload, &PayloadLen) != 0) {
		return ret;
	}
	// some grouped commands are special
	if (cmdgroup_special(CmdID, Payload, PayloadLen, &ret)) {
		goto label_done;
	}

	CmdGroup = GROUP_UNPACK(CmdID);
	switch (CmdGroup) {
	case CMDGROUP_GENERAL:{
			ret = cmdgroup_general(CmdID, Payload, PayloadLen);
			break;
		}
	case CMDGROUP_DEVICE_INFO:{
			ret = cmdgroup_device_info(CmdID, Payload, PayloadLen);
			break;
		}
#ifdef SPEC_REMOTE_CONTROL
	case CMDGROUP_REMOTE_CONTROL:{
			ret =
			    cmdgroup_remote_control(CmdID, Payload, PayloadLen);
			break;
		}
#endif
	case CMDGROUP_OTADFU:{
			ret = cmdgroup_otadfu(CmdID, Payload, PayloadLen);
			break;
		}
#ifdef SPEC_LED_PATTERN_CONTROL
	case CMDGROUP_LED_PATTERN_CONTROL:{
			ret =
			    cmdgroup_led_pattern_control(CmdID, Payload,
							 PayloadLen);
			break;
		}
	case CMDGROUP_LED_PACKAGE:{
			ret = cmdgroup_led_package(CmdID, Payload, PayloadLen);
			break;
		}
#endif
	case CMDGROUP_SPEAKER_SETTINGS:{
			ret =
			    cmdgroup_speaker_settings(CmdID, Payload,
						      PayloadLen);
			break;
		}
	default:{
			selfapp_log_wrn("Unknow Cmd: 0x%x\n", CmdID);
			break;
		}
	}

 label_done:
	if (ret) {
		selfapp_log_wrn("Command 0x%x fail:\n", CmdID);
#ifdef SELFAPP_DEBUG
		selfapp_log_dump(buf, (len > 0x10 ? 0x10 : len));
#endif
	}

	return ret;
}

void selfapp_notify_role(void)
{
	u8_t buf[8];
	int len;
	u8_t role;

	role = selfapp_get_role();
	selfapp_log_inf("role=%d", role);
	len = selfapp_pack_cmd_with_int(buf, DEVCMD_RetRoleInfo, role, 1);
	self_send_data(buf, len);
}

void selfapp_notify_lasting_stereo_status2(void)
{
	u8_t buf[16];
	u16_t sendlen = 0;

	sendlen = devinfo_pack_token(buf, TokenID_lasting_Stereo_Connected,0);
	if (sendlen > 0) {
		self_send_data(buf, sendlen);
	}
}

void selfapp_notify_lasting_stereo_status(void)
{
	selfapp_context_t *selfctx = self_get_context();

	//TO DO:creat failed bug last group device connected sucessful,we should not
	//clear the info
	if(selfapp_get_lasting_stereo_mode() && selfctx->creat_group.id){
#ifdef CONFIG_BT_LETWS
		bt_addr_le_t *le_addr =  btif_get_le_addr_by_handle(bt_manager_letws_get_handle());
		if(le_addr && !memcmp(le_addr->a.val,selfctx->creat_group.addr,sizeof(le_addr->a.val))){
			selfapp_log_inf("update group info id 0x%x 0x%x", selfctx->creat_group.id,selfapp_get_group_id());
			selfapp_config_set_ac_group(&selfctx->creat_group);
		}else{
			selfapp_log_inf("creat group but another device connected");
		}

		memset(&selfctx->creat_group,0,sizeof(selfctx->creat_group));
		selfapp_resume_player();
		if (thread_timer_is_running(&selfctx->creat_group_timer)){
			thread_timer_stop(&selfctx->creat_group_timer);
		}
#endif
	}

	selfapp_notify_lasting_stereo_status2();
	spk_ret_auracast_group(NULL,0);
}

int selfapp_report_bat(void)
{
	u8_t buf[8];
	u16_t sendlen = 0;
	int ret = -1;

	sendlen = devinfo_pack_token(buf, TokenID_BatteryStatus, 0);
	if (sendlen > 0) {
		ret = self_send_data(buf, sendlen);
	}

	return ret;
}

int selfapp_update_bat_for_secondary_device(u8_t value){
	selfapp_context_t *selfctx = self_get_context();
	u8_t buf[8];
	u16_t sendlen = 0;
	int ret = -1;

	if (NULL == selfctx || NULL == buf) {
		return ret;
	}
	selfapp_log_inf("%d\n",value);
	if(selfctx->secondary_device.validate){
		selfctx->secondary_device.bat_status = value;
		sendlen = devinfo_pack_token(buf, TokenID_BatteryStatus, 1);
		if (sendlen > 0) {
			ret = self_send_data(buf, sendlen);
		}
	}
	return ret;
}

void selfapp_report_secondary_device_info(void)
{
	u8_t buf[32];
	u16_t sendlen = 0;

	selfapp_log_inf("");

	u8_t token_array[] = {
	    TokenID_BatteryStatus,
	    TokenID_Serial_Number,
	    TokenID_ActiveChannel,
	    TokenID_SupportAuracast,
	    TokenID_Firmware_Version,
	    TokenID_lasting_Stereo_Connected,
	};
	for (int i = 0; i < sizeof(token_array); i++) {
		sendlen = devinfo_pack_token(buf, token_array[i], 1);
		if (sendlen > 0) {
			self_send_data(buf, sendlen);
		}
	}
}

int selfapp_report_leaudio_status(void)
{
	u8_t buf[8];
	u16_t sendlen = 0;
	int ret = -1;

	int status = selfapp_get_leaudio_status();
	selfapp_log_inf("leaudio status=%d", status);

	sendlen = selfapp_pack_cmd_with_int(buf, CMD_RetLeAudioStatus, status, 1);
	ret = self_send_data(buf, sendlen);

	return ret;
}