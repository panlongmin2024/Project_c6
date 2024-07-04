/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demo_app.h"

#ifdef CONFIG_USB_HOTPLUG
#include <hotplug_manager.h>
#endif

#include <ui_manager.h>
#include <audio_policy.h>
#include "app_common.h"
#include <sys_wakelock.h>

static struct demo_app_t *p_usound;

static void _usb_audio_event_callback_handle(u8_t info_type, int pstore_info)
{
	struct app_msg msg = { 0 };

	SYS_LOG_INF("t=%d, v=%d\n", info_type, pstore_info);
	switch (info_type) {
	case USOUND_SYNC_HOST_MUTE:
		SYS_LOG_INF("Host Set Mute\n");
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 1);
	break;

	case USOUND_SYNC_HOST_UNMUTE:
		SYS_LOG_INF("Host Set UnMute\n");
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 0);
	break;

	case USOUND_SYNC_HOST_VOL_TYPE:
	case UMIC_SYNC_HOST_VOL_TYPE:
	case UMIC_SYNC_HOST_MUTE:
	case UMIC_SYNC_HOST_UNMUTE:
		break;
	case USOUND_STREAM_STOP:
		SYS_LOG_INF("stream stop\n");
		msg.type = MSG_DEMO_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_STOP;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	break;
	case USOUND_STREAM_START:
		SYS_LOG_INF("stream start\n");
		msg.type = MSG_DEMO_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_START;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_USB_UART_CONSOLE
void trace_set_usb_console_active(u8_t active);
#endif

static int _demo_init(void *p1, void *p2, void *p3)
{
	if (p_usound)
		return 0;

	p_usound = app_mem_malloc(sizeof(struct demo_app_t));
	if (!p_usound) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

	memset(p_usound, 0, sizeof(struct demo_app_t));

	demo_view_init();

#ifdef CONFIG_USB_UART_CONSOLE
#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
		trace_set_usb_console_active(0);
	}
#endif
#endif

#ifdef CONFIG_BT_ADV_MANAGER
	send_message_to_system(MSG_BT_MGR_EVENT, MSG_BLE_ADV_CONTROL, 0);
#endif
	bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
	btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
	bt_manager_halt_ble();

#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
#endif
		usb_audio_init(_usb_audio_event_callback_handle);
		p_usound->device_init = true;
#ifdef CONFIG_USB_HOTPLUG
	}
#endif

	p_usound->current_volume_level = audio_system_get_stream_volume(AUDIO_STREAM_USOUND);

	sys_wake_lock(WAKELOCK_DEMO_MODE);
	SYS_LOG_INF("Demo init ok\n");
	return 0;
}

static int _demo_exit(void)
{
	if (!p_usound)
		goto exit;

	if (p_usound->playing) {
		usb_hid_control_pause_play();
	}

	demo_stop_play();

	if (p_usound->device_init){
		usb_audio_deinit();
		p_usound->device_init = false;
	}

	demo_view_deinit();

	bt_manager_set_user_visual(0,0,0,0);
	bt_manager_resume_ble();
#ifdef CONFIG_BT_ADV_MANAGER
	send_message_to_system(MSG_BT_MGR_EVENT, MSG_BLE_ADV_CONTROL, 1);
#endif

	app_mem_free(p_usound);

	p_usound = NULL;

#ifdef CONFIG_USB_UART_CONSOLE
#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
		trace_set_usb_console_active(1);
	}
#endif
#endif

exit:
	sys_wake_unlock(WAKELOCK_DEMO_MODE);
	SYS_LOG_INF("exit ok\n");
	return 0;
}

struct demo_app_t *demo_get_app(void)
{
	return p_usound;
}

static int _demo_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		demo_input_event_proc(msg);
		break;

	case MSG_DEMO_APP_EVENT:
		demo_event_proc(msg);
		break;

	case MSG_TTS_EVENT:
		demo_tts_event_proc(msg);
		break;

	case MSG_EXIT_APP:
		_demo_exit();
		break;

#ifdef CONFIG_USB_HOTPLUG
	case MSG_HOTPLUG_EVENT:
		if (msg->value == HOTPLUG_IN){
			if(!p_usound->device_init){
				usb_audio_init(_usb_audio_event_callback_handle);
				p_usound->device_init = true;
			}
		}else{
			if(p_usound->device_init){
				usb_audio_deinit();
				p_usound->device_init = false;
			}
		}
		break;
#endif

	default:
		break;
	}
	return 0;
}

static int _demo_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_DEMO, (void *)p_usound, sizeof(struct demo_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_DEMO, _demo_init, _demo_exit, _demo_proc_msg, \
	_demo_dump_app_state, NULL, NULL, NULL);

