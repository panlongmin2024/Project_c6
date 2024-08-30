/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_DOMAIN "broadcast"
#include <logging/sys_log.h>
#include <audio_system.h>
#include <volume_manager.h>
#include "broadcast.h"
#include "app_common.h"
#include <input_manager.h>
#include <sys_event.h>
#include <bt_manager.h>
#include <property_manager.h>
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#include <app_launch.h>
#include <fw_version.h>

enum DeviceInfo_TokenID_e {
	TokenID_ProductID = 0x31,	     // R  2bytes
	TokenID_ModelID = 0x32,	         // R  1byte
	TokenID_BatteryStatus = 0x33,	 // R  1byte, bit[7] charging or not, bit[6-0] battery level 0-100%
	TokenID_LinkDevCount = 0x34,	 // R  1byte, Count of Linked device count
	TokenID_ActiveChannel = 0x35,	 // RW 1byte, 0 stereo, 1 left, 2 right
	TokenID_AudioSource = 0x36,	     // RW 1byte, 0 no audio playing, 1 bt, 2 Aux-in, 3 usb
	TokenID_MACAddress = 0x37,	     // RW 6bytes
	TokenID_Role = 0x38,	         // RW 1byte
	TokenID_TwsMode = 0x39,	         // RW 1byte, 0 normal, 1 tws connecting, 2 tws connected, 3 wired mode
	TokenID_GroupId = 0x3b,		     // RW 4bytes
	TokenID_AuracastMode = 0x3c,	 // RW 1byte  0 normal, 1 auracast, 2 auracast on and unlinked, 3 auracast on and linked
	TokenID_SyncOnOff = 0x3d,	     // RW 1byte  0 sync on off disable, 1 sync on off enable
	TokenID_DeviceSerialNum = 0x40,	 // R  16bytes
	TokenID_DeviceFirmware = 0x41,	 // R  4bytes
	TokenID_UsbStatus = 0x42,	     // R  1byte
	TokenID_MicStatus = 0x43,	     // R  1byte
	TokenID_DeviceNameCRC16 = 0x44,	 // R  2bytes, A CRC16 value of source device name
	TokenID_SoundDetection = 0x45,	 // RW 1byte
	TokenID_AbsoluteVolume = 0x46,	 // RW 1byte, Range:0-127
	TokenID_DeviceName = 0xc1,	     // RW dynamic
	TokenID_GroupName = 0xc2,        // RW dynamic
};

static uint8_t ready_for_past = 0;
uint8_t broadcast_tws_is_ready_for_past(){
	return ready_for_past;
}

#ifdef CONFIG_OTA_BACKEND_LETWS_STREAM
static void (*letws_ota_data_cbk)(uint8_t *data, uint32_t len);

void broadcast_tws_ota_data_cbk_register(void (*callback)(uint8_t *data, uint32_t len))
{
	uint32_t irq_flag = irq_lock();
	letws_ota_data_cbk = callback;
	irq_unlock(irq_flag);
}


void broadcast_tws_ota_data_save(uint8_t *data, uint32_t len)
{
	if(letws_ota_data_cbk){
		letws_ota_data_cbk(data, len);
	}
}
#endif

int broad_tws_send_message_to_front(uint8_t msg_type, uint8_t cmd, uint8_t reserved)
{
	struct app_msg  msg = {0};

	msg.type = msg_type;
	msg.cmd = cmd;
	msg.reserve = reserved;

	return send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

uint8_t broadcast_tws_is_key_need_transfer(uint32_t key_event){
	u32_t key_type = key_event  & KEY_MAX;
	if(key_type == KEY_VOLUMEUP || key_type == KEY_VOLUMEDOWN || key_type == KEY_NEXTSONG
		|| key_type == KEY_PREVIOUSSONG || key_type == KEY_BT || key_type == KEY_PAUSE_AND_RESUME
		|| (key_event == (KEY_POWER | KEY_TYPE_SHORT_UP))){
		return 1;
	}else{
		return 0;
	}
}

static void key_event_handler(uint8_t key_val)
{
   switch (key_val)
   {
       case KEY_EVENT_PLAY:
	   SYS_LOG_INF("PLAY:");
	   sys_event_report_input(KEY_POWER|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_PAUSE:
	   SYS_LOG_INF("PAUSE:");
	   break;

	   case KEY_EVENT_VOLUME_UP:
	   SYS_LOG_INF("VOL UP:");
	   sys_event_report_input(KEY_VOLUMEUP|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_VOLUNE_DOWN:
	   SYS_LOG_INF("VOL DOWN:");
	   sys_event_report_input(KEY_VOLUMEDOWN|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_PARTY:
	   SYS_LOG_INF("PARTY:");
	   sys_event_report_input(KEY_TBD|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_BT:
	   SYS_LOG_INF("BT:");
	   sys_event_report_input(KEY_BT|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_NEXT:
	   SYS_LOG_INF("NEXT:");
	   sys_event_report_input(KEY_NEXTSONG|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_PREV:
	   SYS_LOG_INF("PREV:");
	   sys_event_report_input(KEY_PREVIOUSSONG|KEY_TYPE_SHORT_UP);
	   break;

	   case KEY_EVENT_POWER_OFF:
	   SYS_LOG_INF("POWER OFF:");
	   sys_event_report_input(KEY_POWER|KEY_TYPE_LONG_DOWN);
	   break;

	   default:
	   SYS_LOG_INF("Unkown key:%d \n",key_val);
	   break;
   }
}

static void broadcast_tws_vnd_handle_set_dev_info(const uint8_t * data,int len)
{
	uint16_t temp_acl_handle = 0;
#ifdef CONFIG_BT_SELF_APP
	struct AURACAST_GROUP group;
	memset(&group,0,sizeof(group));
	group.mode = 1;//stereo mode
	group.role = 2;//Secondary
#endif
	temp_acl_handle = bt_manager_audio_get_letws_handle();

    if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	int index = 0;
	while(index < len){
		int id = data[index];
		switch (id) {
		case TokenID_ActiveChannel:
			SYS_LOG_INF("ActiveChan=0x%x\n", data[index + 1]);
#ifdef CONFIG_BT_SELF_APP
			group.ch = data[index + 1];
#endif
			index += 1 + 1;
			break;

		case TokenID_MACAddress:
#ifdef CONFIG_BT_SELF_APP
			memcpy(group.addr,&data[index + 1],6);
#endif
			index += 1 + 6;
			break;

		case TokenID_GroupName:
			SYS_LOG_INF("group name: %s\n",&data[index + 2]);
#ifdef CONFIG_BT_SELF_APP
			memcpy(group.name,&data[index + 2],data[index + 1]);
#endif
			index += 1 + 1 + data[index + 1];
			break;

		case TokenID_GroupId:
#ifdef CONFIG_BT_SELF_APP
			memcpy(&group.id,&data[index + 1],4);
#endif
			if(!group.id){
				group.mode = 0;
				group.role = 0;
			}
			SYS_LOG_INF("group id 0x%x.\n", group.id);
			index += 1 + 4;
			break;

		default:
			SYS_LOG_INF("unknow token 0x%x\n", id);
			break;
		}
	}

#ifdef CONFIG_BT_SELF_APP
	selfapp_set_lasting_stereo_group_info(&group);
#endif

}

static void broadcast_tws_vnd_handle_dev_info_rsp(const uint8_t * data,int len)
{
	uint16_t temp_acl_handle = 0;
	temp_acl_handle = bt_manager_audio_get_letws_handle();
	selfapp_device_info_t info;
	uint8_t first = 0;
	uint8_t update_bat = 0;

    if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	memset(&info,0,sizeof(info));

	int index = 0;
	while(index < len){
		int id = data[index];
		switch (id) {
		case TokenID_BatteryStatus:
			info.bat_status = data[index + 1];
			index += 1 + 1;
			update_bat = 1;
			break;
		case TokenID_DeviceSerialNum:
			memcpy(info.serial_num,&data[index + 1],SELF_SN_LEN);
			index += 1 + SELF_SN_LEN;
			first = 1;
			break;
		case TokenID_DeviceFirmware:
			memcpy(info.firmware_version,&data[index + 1],4);
			index += 1 + 4;
			break;
		case TokenID_DeviceName:
			memcpy(info.bt_name,&data[index + 2],data[index + 1]);
			index += 1 + 1 + data[index + 1];
			break;
		default:
			SYS_LOG_INF("unknow token 0x%x\n", id);
			break;
		}
	}

	if(first){
#ifdef CONFIG_BT_SELF_APP
		selfapp_set_device_info(&info);
#endif
		if (system_app_get_auracast_mode() == 3) {
			SYS_LOG_INF("sync eqinfo when connected");
			//TODO:
			//broadcast_tws_vnd_send_resp_eqinfo(1);
		}
	}else if(update_bat){
		selfapp_update_bat_for_secondary_device(info.bat_status);
	}
}

static void broadcast_tws_vnd_handle_set_eqinfo(const uint8_t *data, int len)
{
	u8_t i, seteqcmd[15];
	uint16_t temp_acl_handle = bt_manager_audio_get_letws_handle();
	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}
	if (data[0] != 0x01) {
		SYS_LOG_ERR("devidx 0x%x notmatch", data[0]);
		return ;
	}

	SYS_LOG_INF("0x%x, %d", data[1], len);

	seteqcmd[0] = data[1];  // eqname: active eqcategory id

	seteqcmd[1] = EQ_NAME_CUSTOM;  // custom eqcategory id, fixedly
	seteqcmd[2] = 0x06;            // scope: [-6, +6]
	seteqcmd[3] = 5;               // band count, 5 for now

	for (i = 0; i < seteqcmd[3]; i++) {
		seteqcmd[4 + i*2]   = i + 1;        // band[i] type
		seteqcmd[4 + i*2 + 1] = data[3+i];  // band[i] level

		// only 3 band level from primary device for now, the left bands set 0
		if (i >= 3) {
			seteqcmd[4 + i*2 + 1] = 0;
		}
	}
	//print_buffer(seteqcmd, 1, 14, 16, 0x11223344);

#ifdef CONFIG_BT_SELF_APP
	extern int spkeq_SetNTIEQ(u8_t * param, u16_t plen);
	spkeq_SetNTIEQ(seteqcmd, 14);
#endif
}

void broadcast_tws_vnd_send_resp_eqinfo(u8_t send)
{
	u8_t eqcmdbuf[11];
	uint16_t temp_acl_handle = bt_manager_audio_get_letws_handle();
	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	eqcmdbuf[0] = COMMAND_IDENTIFIER;
	eqcmdbuf[1] = COMMAND_RSPUSEREQ;
	eqcmdbuf[2] = 6;            // payloadlen
	if (send) {
		eqcmdbuf[1] = COMMAND_SETUSEREQ;
	}

	eqcmdbuf[3] = 1;            // device index, 1 secondary
	eqcmdbuf[4] = EQ_NAME_OFF;  // active eqid, refer to EQ_Category_Id_e in selfapp_internal.h
	eqcmdbuf[5] = 1;            // ON, of course

#ifdef CONFIG_BT_SELF_APP
	{
		u8_t scope = 0;
		s8_t eqband[5][2];  //refer to customeq_param_nti_t

		extern u8_t selfapp_config_get_eq_id(void);
		extern int selfapp_config_get_customer_eq(u8_t *scope, u8_t *bands);

		eqcmdbuf[4] = selfapp_config_get_eq_id();
		selfapp_config_get_customer_eq(&scope, (u8_t *)eqband);
		//print_buffer(eqband, 1, 10, 16, 0x12345678);

		#define EQScopeLimit(x)   ( ((x) < -2) ? -2 : ((x) > 2) ? 2 : (x) )
		#define EQScopePostive(x) ( (0 - (x)) + 2 )
		eqcmdbuf[6] = EQScopePostive(EQScopeLimit(eqband[0][1]));
		eqcmdbuf[7] = EQScopePostive(EQScopeLimit(eqband[1][1]));
		eqcmdbuf[8] = EQScopePostive(EQScopeLimit(eqband[2][1]));
		//eqcmdbuf[9] = EQScopePostive(EQScopeLimit(eqband[3][1]));
		//eqcmdbuf[10] = EQScopePostive(EQScopeLimit(eqband[4][1]));
	}
#endif

	SYS_LOG_INF("%x", eqcmdbuf[1]);
	print_buffer(eqcmdbuf, 1, eqcmdbuf[2]+3, 16, 0xaabbccdd);

	bt_manager_audio_le_vnd_send(temp_acl_handle, eqcmdbuf, eqcmdbuf[2] + 3);
}

int broadcast_tws_vnd_rx_cb(uint16_t handle,const uint8_t *buf, uint16_t len)
{
	int i;
	u8_t cmd_length = 0;
	u8_t cmd_id = 0;
	u8_t send_ack = 0;

	SYS_LOG_INF("buf: %p, len: %d\n", buf, len);

	if(buf){
		if(buf[1] != COMMAND_SENDOTADATA){
			for (i = 0; i < len; i++) {
				printk("%02x ", buf[i]);
			}
			printk("\n");
		}

		if (buf[0] != COMMAND_IDENTIFIER) {
			SYS_LOG_INF("Unknow data service.");
			return -1;
		}

		cmd_id = buf[1];
		cmd_length = buf[2];

		SYS_LOG_INF("ll_cmd_id:0x%x len:%d", cmd_id, cmd_length);

		switch (cmd_id)
		{
			case COMMAND_SETKEYEVENT:
				SYS_LOG_INF("key_event:%d\n", buf[3]);
				key_event_handler(buf[3]);
				send_ack = 1;
				break;
			case COMMAND_REQ_PASTINFO:
				SYS_LOG_INF("req past info\n");
				ready_for_past = 1;
				broad_tws_send_message_to_front(MSG_TWS_EVENT,TWS_EVENT_REQ_PAST_INFO,0);
				send_ack = 1;
				break;
			case COMMAND_SENDSYSEVENT:
				SYS_LOG_INF("sys_event:%d\n", buf[3]);
				sys_event_notify_single(buf[3]);
				send_ack = 1;
				break;
			case COMMAND_SETDEVINFO:
				SYS_LOG_INF("set dev info\n");
				broadcast_tws_vnd_handle_set_dev_info(&buf[4],cmd_length - 1);
				send_ack = 1;
				break;
			case COMMAND_REQDEVINFO:
				SYS_LOG_INF("req dev info\n");
				broadcast_tws_vnd_notify_dev_info();
				break;
			case COMMAND_RSPDEVINFO:
				SYS_LOG_INF("rsp dev info\n");
				broadcast_tws_vnd_handle_dev_info_rsp(&buf[4],cmd_length - 1);
				break;
			case COMMAND_SETLIGHTINFO:
				SYS_LOG_INF("set light info\n");
				send_ack = 1;
				break;
			case COMMAND_REQUSEREQ:
				SYS_LOG_INF("resp eq info\n");
				broadcast_tws_vnd_send_resp_eqinfo(0);
				break;
			case COMMAND_SETUSEREQ:
				SYS_LOG_INF("set eq info\n");
				broadcast_tws_vnd_handle_set_eqinfo(&buf[3], cmd_length);
				send_ack = 1;
				break;
			case COMMAND_DEVACK:
				SYS_LOG_INF("recv ack for cmd 0x%x \n",buf[3]);
				break;

#ifdef CONFIG_OTA_BACKEND_LETWS_STREAM
			case COMMAND_SENDOTADATA:
				broadcast_tws_ota_data_save((uint8_t *)&buf[2], cmd_length + 1);
				break;

			case COMMAND_REQ_OTA:
				sys_event_notify_single(SYS_EVENT_OTA_REQ_START);
				break;
#endif

			default:
				SYS_LOG_INF("Unkown type");
				break;
		}

		if(send_ack){
			broadcast_tws_vnd_send_ack(cmd_id,0);
		}
	}else if(!len){
		if(handle){
			if(BTSRV_TWS_SLAVE == bt_manager_letws_get_dev_role()){
				broadcast_tws_vnd_notify_dev_info();
			}
		}else{
			ready_for_past = 0;
		}
	}

	return 0;
}


void broadcast_tws_vnd_send_key(uint32_t key)
{
    uint8_t cmd[4] = { 0 };

	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

    if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	switch(key){
		case KEY_POWER|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_PLAY;
			break;
		case KEY_PAUSE_AND_RESUME|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_PLAY;
			break;
		case KEY_POWER|KEY_TYPE_LONG_DOWN:
			key = KEY_EVENT_POWER_OFF;
			break;
		case KEY_VOLUMEUP|KEY_TYPE_SHORT_UP:
		case KEY_VOLUMEUP|KEY_TYPE_LONG:
		case KEY_VOLUMEUP|KEY_TYPE_HOLD:
			key = KEY_EVENT_VOLUME_UP;
			break;
		case KEY_VOLUMEDOWN|KEY_TYPE_SHORT_UP:
		case KEY_VOLUMEDOWN|KEY_TYPE_LONG:
		case KEY_VOLUMEDOWN|KEY_TYPE_HOLD:
			key = KEY_EVENT_VOLUNE_DOWN;
			break;
		case KEY_TBD|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_PARTY;
			break;
		case KEY_BT|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_BT;
			break;
		case KEY_NEXTSONG|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_NEXT;
			break;
		case KEY_PREVIOUSSONG|KEY_TYPE_SHORT_UP:
			key = KEY_EVENT_PREV;
			break;
		default:
		   SYS_LOG_INF("Unkown key:%x \n",key);
		   return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
    cmd[1] = COMMAND_SETKEYEVENT;
	cmd[2] = 1;
	cmd[3] = key;

   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, sizeof(cmd));
}

void broadcast_tws_vnd_send_ack(uint8_t ack_cmdid,uint8_t StatusCode)
{
    uint8_t cmd[5] = { 0 };
	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

    if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
    cmd[1] = COMMAND_DEVACK;
	cmd[2] = 0x02;
	cmd[3] = ack_cmdid;
	cmd[4] = StatusCode;

   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, sizeof(cmd));
}

void broadcast_tws_vnd_request_past_info(){
	uint8_t cmd[3] = { 0 };
	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}
	ready_for_past = 1;

	cmd[0] = COMMAND_IDENTIFIER;
    cmd[1] = COMMAND_REQ_PASTINFO;
	cmd[2] = 0x00;
	SYS_LOG_INF("");
   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, sizeof(cmd));
}

void broadcast_tws_vnd_send_sys_event(uint8_t event){
	uint8_t cmd[4] = { 0 };
	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	if (event != SYS_EVENT_ENTER_PAIR_MODE
		&& event != SYS_EVENT_MAX_VOLUME
		&& event != SYS_EVENT_BT_CONNECTED
		&& event != SYS_EVENT_2ND_CONNECTED){
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SENDSYSEVENT;
	cmd[2] = 0x01;
	cmd[3] = event;
	SYS_LOG_INF("%d\n",event);
	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, sizeof(cmd));
}

void broadcast_tws_vnd_set_dev_info(struct lasting_stereo_device_info *info){
	uint8_t cmd[100] = { 0 };
	uint16_t temp_acl_handle = 0;
	int index = 0;

	if (!info) {
		SYS_LOG_ERR("invalid param");
		return;
	}

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	cmd[index++] = COMMAND_IDENTIFIER;
	cmd[index++] = COMMAND_SETDEVINFO;
	cmd[index++] = 0;

	cmd[index++] = 0;//device index
	cmd[index++] = TokenID_ActiveChannel;
	cmd[index++] = info->ch;

	cmd[index++] = TokenID_MACAddress;
	memcpy(&cmd[index],info->addr,6);
	index += 6;

	cmd[index++] = TokenID_GroupName;
	cmd[index++] = strlen(info->name) + 1;
	memcpy(&cmd[index],info->name,strlen(info->name));
	index += strlen(info->name) + 1;

	cmd[index++] = TokenID_GroupId;
	memcpy(&cmd[index],&info->id,4);
	index += 4;

	cmd[2] = index - 3;

	SYS_LOG_INF("len %d\n",index);
	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, index);
}

void broadcast_tws_vnd_send_light_info(uint8_t *light_info,int len)
{
	uint8_t cmd[100] = { 0 };

	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SETLIGHTINFO;
	cmd[2] = len;
	memcpy(&cmd[3],light_info,len);

   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, len + 3);
}

void broadcast_tws_vnd_send_set_user_eq(struct lasting_stereo_eq_info *eq_info)
{
	uint8_t cmd[20] = { 0 };

	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SETUSEREQ;
	cmd[2] = 6;
	cmd[3] = 0;//device index
	memcpy(&cmd[4],eq_info,6);

   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, 6 + 3);
}

void broadcast_tws_vnd_send_indication()
{
	uint8_t cmd[4] = { 0 };

	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SENDSYSEVENT;
	cmd[2] = 0x01;
	cmd[3] = SYS_EVENT_STEREO_GROUP_INDICATION;
	SYS_LOG_INF("\n");

   	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, 4);
}

void broadcast_tws_vnd_request_dev_info(){
	uint8_t cmd[3] = { 0 };

	uint16_t temp_acl_handle = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_REQDEVINFO;
	cmd[2] = 0;

	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, 3);
}

void broadcast_tws_vnd_notify_dev_info(){
	uint8_t cmd[SELF_DEFAULT_SN_LEN + SELF_BTNAME_LEN + 20] = { 0 };
	uint8_t serial_num[SELF_DEFAULT_SN_LEN] = { 0 };
	uint8_t name[SELF_BTNAME_LEN + 1] = { 0 };
	uint16_t temp_acl_handle = 0;
	int index = 0,len = 0;
	uint8_t  vercode[4];    // 3Bytes is sw version, big endian, 1Byte is hw version
	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	cmd[index++] = COMMAND_IDENTIFIER;
	cmd[index++] = COMMAND_RSPDEVINFO;
	cmd[index++] = 0;

#ifdef CONFIG_PROPERTY
	len = property_get(CFG_ATS_DSN_ID, serial_num, SELF_DEFAULT_SN_LEN);
#endif
		if (len <= 0 || len > SELF_SN_LEN) {
		memcpy(serial_num, SELF_DEFAULT_SN, SELF_DEFAULT_SN_LEN);
	}
	len = strlen(serial_num);

	cmd[index++] = 0;//device index

	cmd[index++] = TokenID_BatteryStatus;
	cmd[index++] = selfapp_get_bat_power();

	cmd[index++] = TokenID_DeviceSerialNum;
	memcpy(&cmd[index],serial_num,SELF_SN_LEN);
	index += SELF_SN_LEN;
	
	extern u32_t fw_version_get_sw_code(void);
	extern u8_t fw_version_get_hw_code(void);
	u32_t hwver = fw_version_get_hw_code();
	u32_t swver = fw_version_get_sw_code();

	vercode[0] = hex2dec_digitpos((swver >> 16) & 0xFF);
	vercode[1] = hex2dec_digitpos((swver >>  8) & 0xFF);
	vercode[2] = hex2dec_digitpos( swver & 0xFF);
	vercode[3] = (u8_t)hwver;

	cmd[index++] = TokenID_DeviceFirmware;
	memcpy(&cmd[index],vercode,4);
	index += 4;

	cmd[index++] = TokenID_DeviceName;
#ifdef CONFIG_PROPERTY
	property_get(CFG_BT_NAME, name, SELF_BTNAME_LEN);
#endif
	len = strlen(name);
	cmd[index++] = len;
	memcpy(&cmd[index],name,len);
	index += len;

	cmd[2] = index - 3;

	SYS_LOG_INF("len %d\n",index);
	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, index);
}

void broadcast_tws_vnd_notify_bat_status(){
	uint8_t cmd[10] = { 0 };
	uint16_t temp_acl_handle = 0;
	int index = 0;

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
        return;
	}

	cmd[index++] = COMMAND_IDENTIFIER;
	cmd[index++] = COMMAND_RSPDEVINFO;
	cmd[index++] = 3;

	cmd[index++] = 0;//device index
	cmd[index++] = TokenID_BatteryStatus;
	cmd[index++] = selfapp_get_bat_power();

	SYS_LOG_INF("len %d\n",index);
	bt_manager_audio_le_vnd_send(temp_acl_handle,cmd, index);
}

#ifdef CONFIG_OTA_BACKEND_LETWS_STREAM
void broadcast_tws_vnd_send_ota_data(uint8_t *data, uint32_t len)
{
	uint16_t temp_acl_handle;

	uint8_t cmd[240] = { 0 };

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SENDOTADATA;
	cmd[2] = len;
	memcpy(&cmd[3], data, len);

	if(len > (sizeof(cmd) - 3)){
		printk("letws ota data too long %d\n", len);
		return;
	}

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	SYS_LOG_INF("send letws data len %d\n", len);

   	bt_manager_audio_le_vnd_send(temp_acl_handle, cmd, len + 3);
}

int broadcast_tws_vnd_send_ota_req(void)
{
	uint16_t temp_acl_handle;

	uint8_t cmd[3] = { 0 };

	temp_acl_handle = bt_manager_audio_get_letws_handle();

	if (!temp_acl_handle) {
		SYS_LOG_ERR("tws link loss:");
		return -EIO;
	}

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_REQ_OTA;
	cmd[2] = 0;

   	bt_manager_audio_le_vnd_send(temp_acl_handle, cmd, 3);

	return 0;
}
#endif
