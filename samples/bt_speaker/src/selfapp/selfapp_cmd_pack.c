#include "selfapp_internal.h"
#include "selfapp_cmd_def.h"
#include <string.h>

#define _IDENTIFIER_ (0xaa)

struct CmdPacket_header {
	u8_t Identifier;	// always 0xaa
	u8_t Command;		// bit[7-4] is CmdGroup, bit[3-0] is SubCmd
	u16_t PayloadLen;	// some Cmd might need 2bytes
};

int selfapp_playload_len_is_2(u8_t cmd)
{
	switch (cmd) {
	case DFUCMD_SetDfuData:{
		selfstream_t *sfstream = self_find_current_sfstream();
		if (sfstream && sfstream->connect_type != BLE_CONNECT_TYPE) {
			return 1;  // BR_EDR use 2 bytes PayloadLen, BLE use 1 byte
		}
		return 0;
	}

	case APICMD_RetAnalyticsCmd:
	case APICMD_RetPlayAnalyticsCmd:
	case CMD_NTI_EQ_Set:
	case CMD_NTI_EQ_Req:
	case CMD_NTI_EQ_Ret:
		return 1;

#ifdef SPEC_LED_PATTERN_CONTROL
	case LEDPCMD_SetLedCanvasPackage:
		return 1;
#endif
	}			//switch

	return 0;
}

int selfapp_check_status_response(u8_t cmd)
{
   // un-tested, don't know which cmd should send, so don't do this now, liushuihua 20140913
	switch (cmd) {
	//case DEVCMD_RetRoleInfo:      // 0x16
	case SPKCMD_RetFeedbackToneStatus:  // 0x66
	//case APICMD_RetLightStatus:   // 0x72
	//case APICMD_RetBassVolume:    // 0x78
	//case APICMD_RetWaterOverHeating:    // 0x80
	//case CMD_RetBatteryStatus:    // 0x9E
	//case CMD_RetLeAudioStatus:    // 0xA5
		return 1;

	} // switch
	return 0;
}


bool selfapp_cmd_check_id(u8_t val)
{
	if (val == _IDENTIFIER_) {
		return true;
	} else {
		return false;
	}
}

int selfapp_check_header(u8_t * buf, u16_t len, u8_t * cmdid,
		u8_t ** payload, u16_t * paylen)
{
	int ret = -1;

	// Identifier Cmd PayLen Payload[], eg: aa 11 00 (ReqDevInfo)
	if (len < 3 || buf[0] != _IDENTIFIER_) {
		return ret;
	}

	*cmdid = buf[1];

	if (selfapp_playload_len_is_2(*cmdid)) {
		*paylen = (buf[2] << 8) | buf[3];	//big endian
		*payload = &buf[4];
	} else {
		*paylen = buf[2];
		*payload = &buf[3];
	}

	ret = 0;

	return ret;
}

u16_t selfapp_get_header_len(u8_t command)
{
	if (selfapp_playload_len_is_2(command)) {
		return 4;
	} else {
		return 3;
	}
}

u16_t selfapp_pack_header(u8_t * buf, u8_t cmd, u16_t len)
{
	buf[0] = _IDENTIFIER_;
	buf[1] = cmd;

	if (selfapp_playload_len_is_2(cmd)) {
		buf[2] = (u8_t) (len >> 8);
		buf[3] = (u8_t) len;
		return 4;
	} else {
		buf[2] = (u8_t) len;
		return 3;
	}
}

u16_t selfapp_pack_bytes(u8_t * buf, u8_t * data, u16_t len)
{
	if (data && len > 0) {
		memcpy(buf, data, len);
		return len;
	} else {
		return 0;
	}
}

/*Little endian int to MSB first order.*/
u8_t selfapp_pack_int(u8_t *buf, u32_t data, u8_t len)
{
	u8_t i;

	__ASSERT(len >= 1 && len <= 4, "wrong len\n");

	for (i = 0; i < len; i++) {
		buf[i] = (u8_t)((data >> (len - 1 - i) * 8) & 0xFF);
	}

	return len;
}

u16_t selfapp_pack_cmd_with_bytes(u8_t * buf, u8_t cmd, u8_t * data, u16_t len)
{
	u16_t size = 0;

	if (data == NULL) {
		len = 0;
	}

	if (4 + len > SELF_SENDBUF_SIZE) {
		selfapp_log_wrn("Wrong data size\n");
		len = 0;
	}

	size += selfapp_pack_header(buf, cmd, len);
	size += selfapp_pack_bytes(buf + size, data, len);
	return size;
}

u16_t selfapp_pack_cmd_with_int(u8_t * buf, u8_t cmd, u32_t data, u8_t len)
{
	u16_t size = 0;
	size += selfapp_pack_header(buf, cmd, len);
	size += selfapp_pack_int(buf + size, data, len);
	return size;
}

u16_t selfapp_pack_devack(u8_t * buf, u8_t ackcmd, u8_t statuscode)
{
#ifdef SELFAPP_DEBUG
	selfapp_log_inf("DevACK=0x%x_%d\n", ackcmd, statuscode);
#endif
	return selfapp_pack_cmd_with_int(buf, GENCMD_DevACK,
				    (ackcmd<< 8) | statuscode, 2);
}

u16_t selfapp_pack_unsupported(u8_t * buf, u8_t ackcmd)
{
	return selfapp_pack_cmd_with_int(buf, CMD_UNSUPPORTED, ackcmd, 1);
}

u8_t selfapp_has_large_payload(u8_t cmdid)
{
	switch (cmdid) {
		//cmds whose payload is larger than recvbuf
	case DFUCMD_SetDfuData:{
			return 1;
		}
	}			//switch

	return 0;
}
