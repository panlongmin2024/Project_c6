/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <ui_manager.h>
#include <power_manager.h>
#include <tts_manager.h>
#include <input_manager.h>
#include <property_manager.h>
#include "app_defines.h"
#include "app_ui.h"
#include "main_app.h"
#include <sys_manager.h>
#include "power_manager.h"

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
const ui_key_map_t common_keymap[] = {
	{KEY_BT, KEY_TYPE_SHORT_UP | KEY_TYPE_SHORT_LONG_UP, 1, MSG_ENTER_PAIRING_MODE}, //phone pair
#ifdef CONFIG_SPI_SWITCH
	{KEY_BROADCAST, KEY_TYPE_SHORT_UP | KEY_TYPE_SHORT_LONG_UP, 1, MSG_SWITCH_SPI},
#else
	//{KEY_BROADCAST, KEY_TYPE_SHORT_UP, 1, MSG_ENTER_BMR},
#endif
	{KEY_FACTORY, KEY_TYPE_LONG, 1, MSG_FACTORY_RESET},
	{KEY_MEDIA_EFFECT_BYPASS, KEY_TYPE_LONG10S, 1, MSG_MEDIA_EFFECT_BYPASS},
	{KEY_SOFT_VERSION, KEY_TYPE_LONG, 1, MSG_SOFT_VERSION},
	{KEY_DEMO, KEY_TYPE_LONG10S, 1, MSG_DEMO_SWITCH},
	{KEY_ATS, KEY_TYPE_LONG10S, 1, MSG_ENTER_ATS},
	{KEY_DSP_EFFECT_SWITCH, KEY_TYPE_LONG10S, 1, MSG_DSP_EFFECT_SWITCH},
	{KEY_BT, KEY_TYPE_LONG5S, 1, MSG_CLEAR_PAIRIED_LIST}, //phone pair
	{KEY_TWS_DEMO_MODE, KEY_TYPE_LONG5S, 1, MSG_ENTERN_TWS_MODE}, //phone pair
	{KEY_FW_UPDATE, KEY_TYPE_LONG, 1, MSG_MCU_FW_UPDATE},//KEY_FW_UPDATE
	{KEY_ENTER_BQB, KEY_TYPE_LONG, 1, MSG_ENTER_BQB},//KEY_FW_UPDATE
	{KEY_RESERVED, 0, 0, 0},
};
#else
const ui_key_map_t common_keymap[] = {
	{KEY_MENU, KEY_TYPE_SHORT_UP, 1, MSG_KEY_SWITCH_APP},
	{KEY_MENU, KEY_TYPE_LONG6S, 1, MSG_FACTORY_DEFAULT},
	{KEY_POWER, KEY_TYPE_LONG_DOWN, 1, MSG_KEY_POWER_OFF},
	{KEY_BT, KEY_TYPE_SHORT_UP, 1, MSG_ENTER_PAIRING_MODE}, //phone pair
	{KEY_BT, KEY_TYPE_LONG_DOWN, 1, MSG_BT_PLAY_TWS_PAIR}, //tws pair/unpair
	{KEY_COMBO_VOL, KEY_TYPE_SHORT_UP, 1, MSG_ENTER_DEMO},

	//{KEY_TBD, KEY_TYPE_LONG_DOWN, 1, MSG_BT_PLAY_DISCONNECT_TWS_PAIR},
	//{KEY_TBD, KEY_TYPE_DOUBLE_CLICK, 1, MSG_BT_SIRI_START},
	{KEY_RESERVED, 0, 0, 0},
};
#endif

int main_app_ui_event(int event)
{
	SYS_LOG_INF(" %d\n", event);

	ui_message_send_async(MAIN_VIEW, MSG_VIEW_PAINT, event);

	return 0;
}

void main_app_volume_show(struct app_msg *msg)
{
#ifdef CONFIG_SEG_LED_MANAGER
	int volume_value = msg->value;
	u8_t volume[5];

	snprintf(volume, sizeof(volume), "U %02u", volume_value);
	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_P1, false);
	seg_led_display_icon(SLED_COL, false);
	seg_led_display_string(SLED_NUMBER1, volume, true);
	seg_led_manager_set_timeout_event(2000, NULL);
#endif
#ifdef CONFIG_LED_MANAGER
	if (msg->cmd) {
		led_manager_set_timeout_event_start();
		led_manager_set_blink(0, 100, 50, OS_FOREVER,
				      LED_START_STATE_ON, NULL);
		led_manager_set_blink(1, 100, 50, OS_FOREVER,
				      LED_START_STATE_OFF, NULL);
		/*blink 3 times */
		led_manager_set_timeout_event(100 * 3, NULL);
	}
#endif
}

static bool is_need_play_tts(u32_t ui_event)
{
	bool ret = false;
	switch (ui_event) {
		case UI_EVENT_POWER_ON:
		case UI_EVENT_POWER_OFF:
		case UI_EVENT_CONNECT_SUCCESS:
		case UI_EVENT_SECOND_DEVICE_CONNECT_SUCCESS:
		case UI_EVENT_ENTER_PAIRING:
		case UI_EVENT_MAX_VOLUME:
		case UI_EVENT_PLAY_ENTER_AURACAST_TTS:
		case UI_EVENT_PLAY_EXIT_AURACAST_TTS:
		case UI_EVENT_MCU_FW_SUCESS:
	        case UI_EVENT_MCU_FW_FAIL:
	        case UI_EVENT_MCU_FW_UPDATED:
		case UI_EVENT_CHARGING_WARNING:
		case UI_EVENT_REMOVE_CHARGING_WARNING:	
		case UI_EVENT_STEREO_GROUP_INDICATION:
			ret = true;
			break;
		default:
			break;
	}
	return ret;
}

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
#include <volume_manager.h>
#include <audio_system.h>
extern int power_manager_get_just_volume_step(void);
#define SMART_JUST_MIN_VOLUME		13
static int smart_handle_volume(void)
{
	int ret;
	u8_t vol;
	int step;

	ret = system_volume_get(AUDIO_STREAM_MUSIC);
	if (ret < 0 || ret <= SMART_JUST_MIN_VOLUME) {
		SYS_LOG_ERR("%d\n", ret);
		return -EINVAL;
	}else{
		vol = ret;
	}
	step = power_manager_get_just_volume_step();
	vol -= step;
	if (vol < SMART_JUST_MIN_VOLUME) {
		vol = SMART_JUST_MIN_VOLUME;
	} 

	SYS_LOG_INF("vol=%d\n", vol);
	system_volume_set(AUDIO_STREAM_MUSIC, vol, true);

	return 0;
}

#endif
extern void bt_manager_update_led_display(void);

static void main_app_view_deal(u32_t ui_event)
{
	struct device *dev = NULL;

	switch (ui_event) {
	case UI_EVENT_PLAY_START:
#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_icon(SLED_PAUSE, false);
		seg_led_display_icon(SLED_PLAY, true);
#endif
		break;
	case UI_EVENT_PLAY_PAUSE:
#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_icon(SLED_PAUSE, true);
		seg_led_display_icon(SLED_PLAY, false);
#endif
		break;
	case UI_EVENT_POWER_OFF:
	case UI_EVENT_OTA_FINISHED_REBOOT:
		/**make sure powerdown tts */
#ifdef CONFIG_PLAYTTS
		tts_manager_wait_finished(true);
#endif
		if(ui_event == UI_EVENT_POWER_OFF){
			sys_event_send_message(MSG_POWER_OFF);
		}else{
			ui_event = UI_EVENT_POWER_OFF;
		}
		break;
	case UI_EVENT_AUTO_POWER_OFF:
		sys_event_send_message(MSG_POWER_OFF);
		break;
	case UI_EVENT_NO_POWER:
		sys_event_send_message(MSG_NO_POWER);
		break;
	case UI_EVENT_WAIT_CONNECTION:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_blink(0, 500, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#endif
		break;
	case UI_EVENT_CONNECT_SUCCESS:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
#endif
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;
	case UI_EVENT_BT_DISCONNECT:
	case UI_EVENT_BT_UNLINKED:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_blink(0, 2000, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#endif
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;
	case UI_EVENT_TWS_TEAM_SUCCESS:
	case UI_EVENT_TWS_DISCONNECTED:
	case UI_EVENT_CLEAR_PAIRED_LIST:
	case UI_EVENT_SECOND_DEVICE_CONNECT_SUCCESS:
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
	case UI_EVENT_SMART_CONTROL_VOLUME:
		smart_handle_volume();
		break;	
#endif
	case UI_EVENT_CHARGING_WARNING:
#ifndef CONFIG_C_TEST_BATT_MACRO	
		/**disable key adc */
		dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME);
		if (dev)
			input_dev_disable(dev);	
#endif			
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,CHARGING_WARNING);
		break;			

	case UI_EVENT_REMOVE_CHARGING_WARNING:
		/**enable key adc */
		dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME);
		if (dev)
			input_dev_enable(dev);	
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,REMOVE_CHARGING_WARNING);
		bt_manager_update_led_display();
		#ifdef CONFIG_LED_MANAGER
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
		#endif
		break;	

	case UI_EVENT_MCU_FW_SUCESS:
		break;

	case UI_EVENT_MCU_FW_FAIL:
		break;
	case UI_EVENT_MCU_FW_UPDATED:
		break;		
	}
	if (is_need_play_tts(ui_event)) {
#ifdef CONFIG_PLAYTTS
#ifdef CONFIG_TWS_UI_EVENT_SYNC
		system_do_event_notify(ui_event);
#else
/* 		static uint32_t last_time = 0;
		if(ui_event == UI_EVENT_MAX_VOLUME){
			if(last_time && k_uptime_get_32() - last_time < 1100){
				return;
			}
			last_time = k_uptime_get_32();
		} */
		tts_manager_process_ui_event(ui_event);
#endif
#endif
	}
}

static int main_app_view_proc(u8_t view_id, u8_t msg_id, u32_t ui_event)
{
	SYS_LOG_INF(" msg_id %d ui_event %d\n", msg_id, ui_event);
	switch (msg_id) {
	case MSG_VIEW_CREATE:
		main_app_view_deal(UI_EVENT_POWER_ON);
		break;
	case MSG_VIEW_PAINT:
		main_app_view_deal(ui_event);
		break;
	case MSG_VIEW_DELETE:
#ifdef CONFIG_PLAYTTS
		tts_manager_wait_finished(true);
#endif
		break;
	default:
		break;
	}
	return 0;
}

void main_app_view_init(void)
{
	ui_view_info_t view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = main_app_view_proc;
	view_info.view_key_map = common_keymap;
	view_info.view_get_state = NULL;
	view_info.order = 0;
	view_info.app_id = APP_ID_MAIN;

#ifdef CONFIG_UI_MANAGER
	ui_view_create(MAIN_VIEW, &view_info);
#endif

#ifdef CONFIG_LED_MANAGER
	led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
	led_manager_set_blink(0, 1000, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
#endif	
#endif

	SYS_LOG_INF(" ok\n");
}

void main_app_view_exit(void)
{
#ifdef CONFIG_UI_MANAGER
	ui_view_delete(MAIN_VIEW);
#endif
}
