/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include <app_ui.h>
#include <app_launch.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <power_supply.h>
#include <srv_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include <power_manager.h>
#include <input_manager.h>
#include <property_manager.h>

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#include "desktop_manager.h"
#include "system_app.h"

#ifdef CONFIG_BT_MANAGER
static void _event_notify_cb(struct app_msg *msg, int result, void *not_used)
{
	if (msg && msg->ptr)
		app_mem_free(msg->ptr);
}

int send_message_to_foregroup(uint8_t msg_type, int event_id, void *event_data, int event_data_size)
{
	struct app_msg  msg = {0};

	if (event_data && event_data_size) {
		msg.ptr = app_mem_malloc(event_data_size + 1);
		if (!msg.ptr)
			return -ENOMEM;
		memset(msg.ptr, 0, event_data_size + 1);
		memcpy(msg.ptr, event_data, event_data_size);
		msg.callback = _event_notify_cb;
	}

	msg.type = msg_type;
	msg.cmd = event_id;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		if (msg.callback){
			app_mem_free(msg.ptr);
		}
		return -1;
	}

	return 0;
}

int send_message_to_system_ext(uint8_t msg_type, int event_id, void *event_data, int event_data_size)
{
	struct app_msg  msg = {0};

	if (event_data && event_data_size) {
		msg.ptr = app_mem_malloc(event_data_size + 1);
		if (!msg.ptr)
			return -ENOMEM;
		memset(msg.ptr, 0, event_data_size + 1);
		memcpy(msg.ptr, event_data, event_data_size);
		msg.callback = _event_notify_cb;
	}

	msg.type = msg_type;
	msg.cmd = event_id;

	if(send_async_msg(CONFIG_SYS_APP_NAME, &msg) == false){
		if (msg.callback){
			app_mem_free(msg.ptr);
		}
		return -1;
	}

	return 0;
}

int send_message_to_system(uint8_t msg_type, uint8_t cmd, uint8_t reserved)
{
	struct app_msg  msg = {0};

	msg.type = msg_type;
	msg.cmd = cmd;
	msg.reserve = reserved;

	return send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}


static const char * const stream_names[] = {
	"unknown", "btcall", "btmusic", "le_call", "le_music", "le_tws"
};

static __unused uint8_t translate_app_id(uint8_t id)
{
	switch (id) {
	case BT_AUDIO_STREAM_BR_CALL:
		return DESKTOP_PLUGIN_ID_BR_CALL;
	case BT_AUDIO_STREAM_BR_MUSIC:
		return DESKTOP_PLUGIN_ID_BR_MUSIC;
	case BT_AUDIO_STREAM_LE_CALL:
		return DESKTOP_PLUGIN_ID_LE_CALL;
	case BT_AUDIO_STREAM_LE_MUSIC:
		return DESKTOP_PLUGIN_ID_LE_MUSIC;
	case BT_AUDIO_STREAM_LE_TWS:
		return DESKTOP_PLUGIN_ID_LE_TWS_MUSIC;
	}

	return 0;
}


int bt_stream_switch(struct bt_manager_stream_report* rep)
{
	static bt_audio_stream_e old_stream = BT_AUDIO_STREAM_NONE;
	static uint16_t old_handle = 0;
	int ret = 0;
	bt_audio_stream_e new_stream = rep->stream;

	SYS_LOG_INF("[%d]%s -> [%d]%s", old_stream, stream_names[old_stream], new_stream, stream_names[new_stream]);

	if ((new_stream != BT_AUDIO_STREAM_NONE) && (old_stream != BT_AUDIO_STREAM_NONE)) {
		system_app_launch_switch(translate_app_id(old_stream), translate_app_id(new_stream));
	} else {
		if (old_stream != BT_AUDIO_STREAM_NONE) {
			system_app_launch_del(translate_app_id(old_stream));
		}
		if (new_stream != BT_AUDIO_STREAM_NONE) {
			system_app_launch_add(translate_app_id(new_stream), true);
		}
	}

	if ((new_stream == old_stream) && (old_handle != rep->handle)){
		ret = 1;
	}

	old_stream = new_stream;
	old_handle = rep->handle;

    return ret;
}


int system_bt_event_callback(uint8_t event, uint8_t* extra, uint32_t extra_len)
{
	SYS_LOG_INF("Event:%d", event);
	print_buffer_lazy(NULL, extra, extra_len);
	switch(event)
	{
		case BT_READY:
			send_message_to_system(MSG_BT_ENGINE_READY, 0, 0);
		break;

		case BT_POWEROFF_RESULT:
			send_message_to_system(MSG_BT_MGR_EVENT,MSG_BT_POWEROFF_RESULT, *extra);
		break;
		case BT_REQUEST_LEAUDIO_RESTART:
			send_message_to_system(MSG_BT_MGR_EVENT,MSG_BT_LEAUDIO_RESTART, 0);
		break;

        case BT_GFP_BLE_CONNECTED:
			send_message_to_system(MSG_BT_MGR_EVENT,MSG_BT_GFP_CONNECTED, 0);
		break;
		case BT_GFP_BLE_DISCONNECTED:
			send_message_to_system(MSG_BT_MGR_EVENT,MSG_BT_GFP_DISCONNECTED,0);
		break;
		case BT_GFP_BLE_INITIAL_PAIRING:
			send_message_to_system(MSG_BT_MGR_EVENT,MSG_BT_GFP_INITIAL_PAIRING,0);
		break;

		case BT_AUDIO_SWITCH:
		{
			if (bt_stream_switch((struct bt_manager_stream_report*)extra) != 0){
				send_message_to_foregroup(MSG_BT_EVENT, event, extra, extra_len);
			}
		}
		break;
		case BT_TWS_EVENT:
		{
			tws_message_t* rep = (tws_message_t*)extra;
			send_message_to_system_ext(MSG_TWS_EVENT, rep->cmd_id, rep, sizeof(tws_message_t));
		}
		break;
		/* for ats test ggec */
		case BT_BIS_CONNECTED:
		{
			send_message_to_system(MSG_ATS_LE_AUDIO,1,0);
		}
		break;		
		case BT_BIS_DISCONNECTED:
		{
			send_message_to_system(MSG_ATS_LE_AUDIO,2,0);
		}
		break;	
		
		default:
			send_message_to_foregroup(MSG_BT_EVENT, event, extra, extra_len);
		break;
	}

	return 0;
}
#endif