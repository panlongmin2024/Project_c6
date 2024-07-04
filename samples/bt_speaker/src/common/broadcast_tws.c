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
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

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
	TokenID_GroupName = 0x3a,        // RW dynamic
	TokenID_GroupId = 0x3b,		     // RW 4bytes
	TokenID_AuracastMode = 0x3c,	 // RW 1byte  0 normal, 1 auracast, 2 auracast on and unlinked, 3 auracast on and linked
	TokenID_SyncOnOff = 0x3d,	     // RW 1byte  0 sync on off disable, 1 sync on off enable
	TokenID_DeviceSerialNum = 0x40,	 // R  16bytes
	TokenID_DeviceFirmware = 0x41,	 // R  3bytes
	TokenID_UsbStatus = 0x42,	     // R  1byte
	TokenID_MicStatus = 0x43,	     // R  1byte
	TokenID_DeviceNameCRC16 = 0x44,	 // R  2bytes, A CRC16 value of source device name
	TokenID_SoundDetection = 0x45,	 // RW 1byte
	TokenID_AbsoluteVolume = 0x46,	 // RW 1byte, Range:0-127
	TokenID_DeviceName = 0xc1,	     // RW dynamic
};

int broad_tws_send_message_to_front(uint8_t msg_type, uint8_t cmd, uint8_t reserved)
{
	struct app_msg  msg = {0};

	msg.type = msg_type;
	msg.cmd = cmd;
	msg.reserve = reserved;

	return send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
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
			SYS_LOG_INF("ActiveChan=0x%x\n", data[index + 2]);
#ifdef CONFIG_BT_SELF_APP
			group.ch = data[index + 2];
#endif
			break;

		case TokenID_MACAddress:
#ifdef CONFIG_BT_SELF_APP
			memcpy(group.addr,&data[index + 2],6);
#endif
			break;

		case TokenID_GroupName:
			SYS_LOG_INF("group name: %s\n",&data[index + 2]);
#ifdef CONFIG_BT_SELF_APP
			memcpy(group.name,&data[index + 2],data[index + 1]);
#endif
			break;

		case TokenID_GroupId:
#ifdef CONFIG_BT_SELF_APP
			memcpy(&group.id,&data[index + 2],data[index + 1]);
#endif
			SYS_LOG_INF("group id 0x%x.\n", group.id);
			break;

		default:
			SYS_LOG_INF("unknow token 0x%x\n", id);
			break;
		}

		index += 2 + data[index + 1];
	}

#ifdef CONFIG_BT_SELF_APP
	selfapp_set_lasting_stereo_group_info(&group);
#endif

}

int broadcast_tws_vnd_rx_cb(const uint8_t *buf, uint16_t len)
{
	int i;
	u8_t cmd_length = 0;
	u8_t cmd_id = 0;
	u8_t send_ack = 0;
	//tws_message_t rep = {0};
	
	SYS_LOG_INF("buf: %p, len: %d\n", buf, len);

	for (i = 0; i < len; i++) {
		printk("0x%x", buf[i]);
	}
	printk("\n");

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
		case COMMAND_SETLIGHTINFO:
			SYS_LOG_INF("set light info\n");
			send_ack = 1;
			break;
		case COMMAND_SETUSEREQ:
			SYS_LOG_INF("set eq info\n");
			send_ack = 1;
			break;
		case COMMAND_DEVACK:
			SYS_LOG_INF("recv ack for cmd 0x%x \n",buf[3]);
			break;
		default:
			SYS_LOG_INF("Unkown type");
			break;
	}

	if(send_ack){
		broadcast_tws_vnd_send_ack(cmd_id,0);
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

	cmd[0] = COMMAND_IDENTIFIER;
	cmd[1] = COMMAND_SENDSYSEVENT;
	cmd[2] = 0x01;
	cmd[3] = event;
	SYS_LOG_INF("");
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
	cmd[index++] = 1;
	cmd[index++] = info->ch;

	cmd[index++] = TokenID_MACAddress;
	cmd[index++] = 6;
	memcpy(&cmd[index],info->addr,6);
	index += 6;

	cmd[index++] = TokenID_GroupName;
	cmd[index++] = strlen(info->name) + 1;
	memcpy(&cmd[index],info->name,strlen(info->name));
	index += strlen(info->name) + 1;

	cmd[index++] = TokenID_GroupId;
	cmd[index++] = 4;
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

