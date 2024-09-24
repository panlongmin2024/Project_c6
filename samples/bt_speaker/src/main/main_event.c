/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#include <hotplug_manager.h>
#include <sys_manager.h>
#include <sys_event.h>
#include <soc_pm.h>
#include <app_launch.h>
#include <app_manager.h>
#ifdef CONFIG_USB_MASS_STORAGE
#ifdef CONFIG_SYS_WAKELOCK
#include <sys_wakelock.h>
#endif
#ifdef CONFIG_FS_MANAGER
#include <fs_manager.h>
#endif
#include <usb/class/usb_msc.h>
#endif
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#include <input_manager.h>
#include <input_dev.h>
#include "app_ui.h"
#include "app_defines.h"
#include "main_app.h"
#include "audio_system.h"
#include <ui_manager.h>
#include <board.h>
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include "property_manager.h"
#include "self_media_effect/self_media_effect.h"
#include "soft_version/soft_version.h"
#include "run_mode/run_mode.h"
#include "common/sys_reboot.h"
#include <tool_app.h>
#include <thread_timer.h>
#include <media_player.h>
#ifdef CONFIG_C_EXTERNAL_DSP_ATS3615
#include <external_dsp/ats3615/ats3615_head.h>
#endif
#ifdef CONFIG_C_AMP_TAS5828M
#include <driver/amp/tas5828m/tas5828m_head.h>
#endif
#include "tts_manager.h"
#include "app/charge_app/charge_app.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#ifdef CONFIG_USB_UART_CONSOLE
void trace_set_usb_console_active(u8_t active);
u8_t trace_get_usb_console_status(void);
#endif

#ifdef CONFIG_GFP_PROFILE
void account_key_clear(void);
void personalized_name_clear(void);
void gfp_ble_mgr_clear_sleep_time(void);
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <wltmcu_manager_supply.h>
extern int ext_dsp_set_bypass(int bypass);
extern int dsp_bypass_enable;
extern bool pd_manager_check_mobile(void);
extern void enter_ats(void);

#endif			
static void main_input_enter_pairing_mode(void)
{
#ifdef CONFIG_AURACAST
	//struct app_msg app_msg = {0};
#endif

	uint8_t bt_link_num = bt_manager_audio_get_cur_dev_num();
	uint8_t max_dev_num = bt_manager_config_connect_phone_num();

	if (bt_link_num <= max_dev_num) {
		sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);
		pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_ENTER_PAIR_MODE);
#ifdef CONFIG_AURACAST
/* 		if(1 == system_app_get_auracast_mode()
			|| 2 == system_app_get_auracast_mode()){
			app_msg.type = MSG_INPUT_EVENT;
			app_msg.cmd = MSG_AURACAST_EXIT;
			desktop_manager_proc_app_msg(&app_msg);
		} */
		if(3 == system_app_get_auracast_mode()
			|| 4 == system_app_get_auracast_mode()){
			bt_manager_audio_disconnect_all_phone_dev();
		}
		bt_manager_event_notify(BT_MSG_AURACAST_EXIT, NULL, 0);
#endif

#ifdef CONFIG_GFP_PROFILE
        gfp_ble_mgr_clear_sleep_time();
#endif

		bt_manager_enter_pair_mode();
	} else {
		SYS_LOG_ERR("max connected dev num,bt:%d\n", bt_link_num);
	}
}

void main_input_event_handle(struct app_msg *msg)
{
	SYS_LOG_INF("cmd %d\n", msg->cmd);
	switch (msg->cmd) {
	case MSG_KEY_POWER_OFF:
		sys_event_notify(SYS_EVENT_POWER_OFF);
		break;
	case MSG_FACTORY_DEFAULT:
		SYS_LOG_INF("factory default\n");
#ifdef CONFIG_BT_MANAGER
		bt_manager_end_pair_mode();
		bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
		bt_manager_auto_reconnect_stop();
		bt_manager_clear_bt_info();
#ifdef CONFIG_BT_LETWS
		bt_manager_clear_letws_info();
#endif
#endif
		audio_system_clear_volume();
#ifdef CONFIG_GFP_PROFILE
		account_key_clear();
		personalized_name_clear();
#endif
#ifdef CONFIG_BT_SELF_APP
		selfapp_config_reset();
#endif

		if(msg->value == 0x0606){
			/* for ats test mode, GGEC factory reset and reboot */
			k_sleep(500);
			sys_event_notify(SYS_EVENT_FACTORYRESET_AND_REBOOT);
		}
		else{
			sys_event_notify(SYS_EVENT_POWER_OFF);	
		}
		system_restore_factory_config();
		break;
	case MSG_KEY_SWITCH_APP:
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
			system_app_switch();
		}
		break;
	case MSG_ENTER_PAIRING_MODE:
		//bt_manager_event_notify(BT_MSG_AURACAST_EXIT, NULL, 0);
		//sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);
		//bt_manager_enter_pair_mode();
		//mcu_ui_set_enable_bt_led_state(1);
		//bt_manager_sys_event_led_display(SYS_EVENT_BT_UNLINKED);
		main_input_enter_pairing_mode();
		break;
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	case MSG_CLEAR_PAIRIED_LIST:
		bt_manager_disconnect_all_device();
		//bt_manager_clear_paired_list();
		//bt_manager_clear_list(BTSRV_DEVICE_ALL);
		//bt_manager_clear_dev_volume();
		//bt_manager_init_dev_volume();
		//tts_manager_filter_music_unlock("MSG_CLEAR_PAIRIED_LIST");
		bt_manager_event_notify(BT_MSG_AURACAST_EXIT, NULL, 0);
		sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);
		bt_manager_end_pair_mode();
		bt_manager_enter_pair_mode();
		break;	
#endif
#ifdef CONFIG_TWS
	case MSG_BT_PLAY_TWS_PAIR:
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_NONE) {
			bt_manager_tws_pair_search();
		} else {
			bt_manager_tws_end_pair_search();
			//bt_manager_tws_clear_paired_list();
			bt_manager_tws_disconnect();
		}
		break;
	case MSG_BT_PLAY_DISCONNECT_TWS_PAIR:
		break;
	case MSG_BT_TWS_SWITCH_MODE:
		bt_manager_tws_channel_mode_switch();
		break;
#endif
#ifdef CONFIG_BT_MANAGER
	case MSG_BT_PLAY_CLEAR_LIST:
		bt_manager_clear_list(BTSRV_DEVICE_ALL);
		break;
#ifdef CONFIG_BT_HFP_HF
	case MSG_BT_SIRI_STOP:
		bt_manager_hfp_stop_siri();
		break;
	case MSG_BT_SIRI_START:
		bt_manager_hfp_start_siri();
		break;
	case MSG_BT_CALL_LAST_NO:
		bt_manager_hfp_dial_last_number();
		break;
#endif
#endif
#ifdef CONFIG_BT_HID
	case MSG_BT_HID_START:
		bt_manager_hid_take_photo();
		/* bt_manager_hid_key_func(KEY_FUNC_HID_CUSTOM_KEY); */
		break;
#endif
#ifdef CONFIG_AURACAST
	case MSG_AURACAST_ENTER:
	case MSG_AURACAST_EXIT:
		SYS_LOG_INF("plugin=%d", desktop_manager_get_plugin_id());
		desktop_manager_proc_app_msg(msg);
		break;
#endif
	case MSG_ENTER_DEMO:
		//app_switch(APP_ID_DEMO, APP_SWITCH_CURR, true);
		system_app_launch_add(DESKTOP_PLUGIN_ID_DEMO,true);
		break;

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	case MSG_FACTORY_RESET:
		SYS_LOG_INF("factory reset\n");
#ifdef CONFIG_BT_SELF_APP
		//exit ota first
		extern void otadfu_SetForce_exit_ota(void);
		otadfu_SetForce_exit_ota();
		int cnt = 0;
		while(cnt++ < 100){
			if(desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_OTA)
				break;
			k_sleep(10);	
		}
#endif
		//lock the led first
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(0));

#ifdef CONFIG_BT_MANAGER
		bt_manager_set_autoconn_info_need_update(0);
		bt_manager_end_pair_mode();
		bt_manager_auto_reconnect_stop();
		bt_manager_clear_paired_list();
		bt_manager_clear_bt_info();
#endif
		audio_system_clear_volume();

#ifdef CONFIG_GFP_PROFILE
		account_key_clear();
		personalized_name_clear();
#endif

#ifdef CONFIG_BT_SELF_APP
		selfapp_config_reset();
#endif

#ifdef CONFIG_TWS
		bt_manager_tws_clear_paired_list();
#endif
		system_restore_factory_config();

/* 		pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_CONNECTED);
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,1);
		battery_charging_LED_on_all();
		mcu_ui_set_enable_bt_led_state(1);
		mcu_ui_set_enable_auracast_led_state(1);
		mcu_ui_set_enable_bat_led_state(1); */
		set_property_factory_reset_flag(1);
		/*
		uint8_t hw_info = ReadODM();
		bool charge_state;

		if(!hw_info){

			charge_state = pd_get_sink_charging_state();
		}else{
			charge_state = dc_power_in_status_read();
		}	*/	
/*  		if (dc_power_in_status_read() 
		|| (sys_pm_get_power_5v_status() == 2)){  */
		//从充电模式关�?		
		if(1){
			u8_t name[33];
			int ret;
			ret = property_get(CFG_BT_NAME, name, 33);
			bt_manager_bt_name_update_fatcory_reset(name,ret);
			//k_sleep(200);
			charge_app_enter_cmd();
		}else
			sys_event_notify(SYS_EVENT_POWER_OFF);
		break;
	case MSG_MEDIA_EFFECT_BYPASS:
		SYS_LOG_INF("media_effect_bypass\n");
        self_music_effect_ctrl_set_enable(!self_music_effect_ctrl_get_enable());
#if 1 // need tts dump music effect info ?
		self_music_effect_ctrl_info_dump_by_tts();
#endif
		break;
	case MSG_DEMO_SWITCH:
	{
		SYS_LOG_INF("demo switch\n");
		
		int run_mode = run_mode_get();

		if (run_mode == RUN_MODE_DEMO)
		{
			run_mode_set(RUN_MODE_NORMAL);
			//tts_manager_play("poweroff.mp3", PLAY_IMMEDIATELY);
			//k_sleep(50);
			//sys_reboot_by_user(REBOOT_REASON_USER_GOTO_NORMAL_FROM_DEMO);
			sys_event_notify(SYS_EVENT_POWER_OFF);
		}
		else if (run_mode == RUN_MODE_NORMAL)
		{
			run_mode_set(RUN_MODE_DEMO);
			pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
			pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
			led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
			//if(sys_pm_get_power_5v_status() != 2){
				sys_event_notify(SYS_EVENT_POWER_OFF);
	/* 		}else{
				tts_manager_play("poweroff.mp3", PLAY_IMMEDIATELY);
				k_sleep(50);
				sys_reboot_by_user(REBOOT_REASON_USER_GOTO_DEMO_FROM_NORMAL);
			} */
		}
		break;
	}
	case MSG_ATS_ENTER_DEMO://for ats ggec
	{
		SYS_LOG_INF("enter demo\n");
		
		int run_mode = run_mode_get();
		if (run_mode == RUN_MODE_DEMO)
		{
			break;
		}
		else if (run_mode == RUN_MODE_NORMAL)
		{
			run_mode_set(RUN_MODE_DEMO);
			pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
			pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
			led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
			sys_event_notify(SYS_EVENT_POWER_OFF);
		}
		break;
	}	
	case MSG_ATS_EXIT_DEMO://for ats ggec
	{
		SYS_LOG_INF("exit demo\n");
		
		int run_mode = run_mode_get();
		if (run_mode != RUN_MODE_DEMO)
		{
			break;
		}
		else if (run_mode == RUN_MODE_NORMAL)
		{
			run_mode_set(RUN_MODE_NORMAL);
			sys_event_notify(SYS_EVENT_POWER_OFF);
		}
		break;
	}		
	case MSG_SOFT_VERSION:
		SYS_LOG_INF("soft version\n");
		fw_version_play_by_tts();
		break;
	case MSG_SWITCH_SPI:
	{
		extern void board_spi_switch(void);
		board_spi_switch();
	}
		break;
	case MSG_ENTER_ATS:
		enter_ats();
		SYS_LOG_INF("enter ATS\n");
		break;
	case MSG_DSP_EFFECT_SWITCH:
		
		self_music_effect_ctrl_set_enable(!self_music_effect_ctrl_get_enable());
#if 1 // need tts dump music effect info ?
		self_music_effect_ctrl_info_dump_by_tts();
#endif
		ext_dsp_set_bypass(!self_music_effect_ctrl_get_enable());

		SYS_LOG_INF("dsp effect switch\n");
		break;
	case MSG_EXTERN_CHARGE_MODE:
		app_switch_add_app(APP_ID_CHARGE_APP_NAME);
		app_switch(APP_ID_CHARGE_APP_NAME, APP_SWITCH_NEXT, false);
		SYS_LOG_INF("entern charge mode\n");
		break;
	case MSG_EXIT_CHARGE_MODE:
		app_switch(APP_ID_BTMUSIC, APP_SWITCH_CURR, true);
		app_switch_remove_app(APP_ID_CHARGE_APP_NAME);
		SYS_LOG_INF("exit charge mode\n");
		break;
	case MSG_ENTERN_TWS_MODE:
	{
		extern void enable_disable_pawr(uint8_t enable);
		static uint8_t pawr_enable_status = 0;
		pawr_enable_status = !pawr_enable_status;
		enable_disable_pawr(pawr_enable_status);
		sys_event_notify(SYS_EVENT_MAX_VOLUME);
		SYS_LOG_INF("tws mode status %d\n",pawr_enable_status);
		break;
	}
	case MSG_MCU_FW_UPDATE:
	{	
		if(!run_mode_is_demo()){
			tts_manager_play("0.mp3", PLAY_IMMEDIATELY);
			pd_srv_event_notify(PD_EVENT_MUC_UPDATA,0);
		}
		SYS_LOG_INF("MSG_MCU_FW_UPDATE !!!\n");
		break;
	}
	case MSG_ENTER_BQB:
	{
		extern int btdrv_set_bqb_mode_ext(void);
		btdrv_set_bqb_mode_ext();
		tts_manager_play("poweroff.mp3",PLAY_IMMEDIATELY);
		break;
	}	
#endif
#ifdef CONFIG_USB_UART_CONSOLE
	case MSG_TRACE_MODE_SWITCH: {
		int cur_plugin = desktop_manager_get_plugin_id();
		if (cur_plugin == DESKTOP_PLUGIN_ID_UAC || cur_plugin == DESKTOP_PLUGIN_ID_DEMO) {
			printk("trance abort: plugin %d unsupport\n", cur_plugin);
			break;
		}

		if (trace_get_usb_console_status()) {
			trace_set_usb_console_active(false);
		} else {
			trace_set_usb_console_active(true);
		}
		break;
	}
#endif
	default:
		desktop_manager_proc_app_msg(msg);
		break;
	}
}

extern int logic_mcu_ls8a10023t_otg_mobile_det(void);

void main_hotplug_event_handle(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, %d\n", msg->cmd, msg->value);
	SYS_EVENT_INF(EVENT_MAIN_HOTPLUG, msg->cmd,msg->value);
	switch (msg->cmd) {
	case HOTPLUG_LINEIN:
#ifdef CONFIG_LINE_IN_APP
		// APP_ID_LINEIN, APP_ID_AUXTWS
		if (msg->value == HOTPLUG_IN) {
			system_app_launch_add(DESKTOP_PLUGIN_ID_LINE_IN, true);
		} else if (msg->value == HOTPLUG_OUT) {
			system_app_launch_del(DESKTOP_PLUGIN_ID_LINE_IN);
		}
#endif
		break;
	case HOTPLUG_USB_DEVICE:
		if (msg->value == HOTPLUG_IN) {
#ifdef CONFIG_DEMO_APP
			if(desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_DEMO){
				desktop_manager_proc_app_msg(msg);
				break;
			}
#endif
#ifdef CONFIG_USOUND_APP
#ifdef CONFIG_USB_UART_CONSOLE
			trace_set_usb_console_active(false);  // exit: confict with UAC
#endif
			system_app_launch_add(DESKTOP_PLUGIN_ID_UAC, (system_boot_time() > 5000) ? 1 : 0);
#endif
		}
		else if (msg->value == HOTPLUG_OUT) {
#ifdef CONFIG_DEMO_APP
			if(desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_DEMO){
				desktop_manager_proc_app_msg(msg);
				break;
			}
#endif
#ifdef CONFIG_USOUND_APP
			system_app_launch_del(DESKTOP_PLUGIN_ID_UAC);
#endif
		}
		break;

	case HOTPLUG_CHARGER:
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
		if (msg->value == HOTPLUG_IN)
		{
			if(run_mode_is_demo() && pd_manager_check_mobile())
			{
				k_sleep(1000);
				logic_mcu_ls8a10023t_otg_mobile_det();
				sys_event_notify(SYS_EVENT_POWER_OFF);
			}
		}
		else if (msg->value == HOTPLUG_OUT)
		{
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
			if (run_mode_is_demo())
			{
				if(!pd_manager_get_source_change_state())
				{
#ifdef CONFIG_LED_MANAGER
					//led_manager_set_display(128, LED_OFF, OS_FOREVER, NULL);
#endif	
					sys_event_notify(SYS_EVENT_POWER_OFF);
				}
			}
			else
			{
		
				if(!pd_manager_get_source_change_state())
				{
	
					charge_app_real_exit_deal();          //Totti modify by 2023/12/23; pd check usb in/out status
				}
				tool_deinit();
			}
#endif
		}
#endif
		break;

#ifdef CONFIG_LCMUSIC_APP
	case HOTPLUG_SDCARD: {
		if (msg->value == HOTPLUG_IN) {
			system_app_launch_add(DESKTOP_PLUGIN_ID_SDCARD_PLAYER, 1);
		}
		else if (msg->value == HOTPLUG_OUT) {
			system_app_launch_del(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
		}
		break;
	}

	case HOTPLUG_USB_HOST: {
		if (msg->value == HOTPLUG_IN) {
			system_app_launch_add(DESKTOP_PLUGIN_ID_USB_PLAYER, 1);
		}
		else if (msg->value == HOTPLUG_OUT) {
			system_app_launch_del(DESKTOP_PLUGIN_ID_USB_PLAYER);
		}
		break;
	}
#endif

	default:
		SYS_LOG_WRN("skip type: %d!\n", msg->cmd);
		break;
	}
}

