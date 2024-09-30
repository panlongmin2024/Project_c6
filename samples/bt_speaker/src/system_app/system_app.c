/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 #include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include <app_launch.h>
#include <soc_dvfs.h>
#include "app_ui.h"
#include <stream.h>
#include <thread_timer.h>
#include <property_manager.h>
#include <sys_manager.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <logic.h>
#include <global_mem.h>
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#include <broadcast.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif
#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif
#include "system_app.h"
#include "app_defines.h"
#include "app_common.h"

#ifdef CONFIG_INPUT_MANAGER
#include <input_manager.h>
#endif

#ifdef CONFIG_BT_LEATWS
#include "system_le_audio.h"
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#include "power_manager.h"

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <run_mode/run_mode.h>
#include <wltmcu_manager_supply.h>
#endif

#include <board.h>

system_app_context_t system_app_context;

extern int bt_event_callback(uint8_t event, uint8_t* extra, uint32_t extra_len);
int sys_ble_advertise_init(void);
void sys_ble_advertise_deinit(void);
void gfp_ble_mgr_update_sleep_time(void);
int system_tws_event_handle(struct app_msg *msg);
extern int sys_standby_start(void);

/* for ats test GGEC */
static bool auracast_sink_connected = false;
void user_set_auracast_sink_connected(bool connect)
{
	auracast_sink_connected = connect;
}
bool user_get_auracast_sink_connected(void)
{
	return auracast_sink_connected;
}

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
static void logic_mcu_ls8a10023t_thread_timer_handler(struct thread_timer *timer, void* pdata)
{
	int ret = 0;

	//printk("[%s,%d]\n", __FUNCTION__, __LINE__);

	struct device *dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
	if (dev == NULL)
	{
		return;
	}

	ret = logic_mcu_ls8a10023t_power_key(dev);

	//printk("[%s,%d] ret : %d\n", __FUNCTION__, __LINE__, ret);
}
#endif

#ifdef CONFIG_OTA_BACKEND_LETWS_STREAM
int ota_app_init_letws(void);
#endif
static void _system_app_init(void)
{
#ifdef CONFIG_LOGIC_MCU_LS8A10023T
	struct thread_timer logic_mcu_ls8a10023t_thread_timer;

	thread_timer_init(&logic_mcu_ls8a10023t_thread_timer, logic_mcu_ls8a10023t_thread_timer_handler, NULL);
    thread_timer_start(&logic_mcu_ls8a10023t_thread_timer, 0, 1000);
#endif
	system_app_init();
	sys_standby_start();
}

static void _system_app_deinit(void)
{

}


static void system_app_do_poweroff(int result)
{
	system_app_context_t*  manager = system_app_get_context();

	SYS_LOG_INF("result %d", result);
	if (result != BTMGR_LREQ_POFF_RESULT_OK && result != BTMGR_RREQ_SYNC_POFF_RESULT_OK) {
		SYS_LOG_ERR("wrong result %d", result);
		return;
	}

	system_power_off();


	manager->sys_status.in_power_off_stage = false;
}


void system_app_enter_poweroff(bool tws_trigger)
{
	static bool g_tws_trigger = false;
	system_app_context_t*  manager = system_app_get_context();

	SYS_LOG_INF("tws=%d, in_power_off=%d", tws_trigger, manager->sys_status.in_power_off_stage);
	SYS_EVENT_INF(EVENT_SYSTEM_POWER_OFF, tws_trigger,manager->sys_status.in_power_off_stage);
	if (manager->sys_status.in_power_off_stage) {
		return;
	}

	// ui_key_filter(true);

	if (tws_trigger) {
		g_tws_trigger = true;
		sys_event_notify(SYS_EVENT_POWER_OFF);
	} else {
		if (!g_tws_trigger) {
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_POWER_OFF, 0, 0);
		}
		manager->sys_status.in_power_off_stage = true;
		g_tws_trigger = false;
		bt_manager_proc_poweroff(false);
	}
}

void system_app_do_poweroff_msg(struct app_msg* msg)
{
	system_app_context_t*  manager = system_app_get_context();

	if (manager->sys_status.in_power_off_stage) {
		system_app_do_poweroff(msg->reserve);
	}
}

static int sys_bt_restart_leaudio(void)
{
#ifdef CONFIG_LE_AUDIO_APP
	btmgr_ble_cfg_t* ble_config = bt_manager_ble_config();
	if (ble_config->leaudio_enable){
		leaudio_base_deinit();
		leaudio_base_init();
	}
#endif

	return 0;
}

static void system_exit_front_app(void)
{
	struct app_msg msg = {0};
	msg.type = MSG_EXIT_APP;
	send_async_msg(APP_ID_MAIN, &msg);
}

static void system_btmgr_event_proc(struct app_msg* msg)
{
	switch (msg->cmd)
	{
	case MSG_BT_POWEROFF_RESULT:
		system_app_do_poweroff_msg(msg);
		break;

	case MSG_BT_LEAUDIO_RESTART:
		sys_bt_restart_leaudio();
		break;

#ifdef CONFIG_GFP_PROFILE
	case MSG_BT_GFP_CONNECTED:
	    gfp_routine_start();
		break;

	case MSG_BT_GFP_DISCONNECTED:
	    gfp_routine_stop();
		break;
	case MSG_BT_GFP_INITIAL_PAIRING:
		gfp_ble_mgr_update_sleep_time();
		break;
#endif

#ifdef CONFIG_BT_ADV_MANAGER
	case MSG_BLE_ADV_CONTROL:
		if (msg->reserve == 0) {
			sys_ble_advertise_deinit();
		} else if (msg->reserve == 1) {
			sys_ble_advertise_init();
		}
		break;
#endif

	default:
		break;
	}
}
extern bool run_mode_is_demo(void);
/* 
* plm add 
* when bat<=5%, poweron and poweroff quickly, can receice SYS_EVENT_POWER_OFF 2 times
* add this code is to pass the second event of SYS_EVENT_POWER_OFF
*/
static bool isPoweroff = false;
void system_set_poweroff(bool is_poweroff)
{
	SYS_LOG_INF("------> %d\n",is_poweroff);
	isPoweroff = is_poweroff;
}
bool system_get_poweroff(void)
{
	SYS_LOG_INF("------> %d\n",isPoweroff);
	return isPoweroff;
}

static void system_sys_event_proc(struct app_msg *msg)
{
	
	system_app_context_t*  manager = system_app_get_context();
	
	if (manager->sys_status.in_power_off_stage == true)
	{
		SYS_LOG_INF("drop event when power off\n");
		return;
	} 

	sys_event_process(msg->cmd);

	if (SYS_EVENT_POWER_OFF == msg->cmd) {

#ifdef CONFIG_INPUT_MANAGER
		input_manager_lock();
#endif
#ifdef CONFIG_BT_SELFAPP_ADV
		selfapp_set_system_status(SELF_SYS_POWEROFF);
		selfapp_set_allowed_adv_crc(false);
		selfapp_set_allowed_join_party(false);
		sys_ble_advertise_deinit();
		sys_ble_advertise_init();
#endif
		/* panlm add,poweroff all led off together */
		extern bool run_mode_is_demo(void);
		if(!system_get_poweroff()){
			system_set_poweroff(true);
			if(run_mode_is_demo()){
			    led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
				pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
				pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
				pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
				pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(0xFF));
			}
			else{
				//led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
				//pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_PWR_LED_ON_0_5S);//PWROFF pwr&bat off 500ms
				pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(0xFF));		
			}
		}
		system_exit_front_app();
	}
#ifdef CONFIG_OTA_BACKEND_LETWS_STREAM
	if (SYS_EVENT_OTA_REQ_START == msg->cmd){
		ota_app_init_letws();
	}
#endif
}

static void _system_app_loop(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = { 0 };
	bool terminated = false;
    int ingore_callback = 0;
	
	_system_app_init();

#ifdef CONFIG_TASK_WDT
	task_wdt_add(TASK_WDT_CHANNEL_SYSTEM_APP, 5000, NULL, NULL);
#endif

	while (!terminated) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			MSG_RECV_TIME_STAT_START();
			SYS_LOG_INF("type %d, value %d\n", msg.type, msg.value);
			switch (msg.type) {
			case MSG_SYS_EVENT:
				system_sys_event_proc(&msg);
				break;

				case MSG_POWER_OFF:
					//Do read system power off after tws sync finish.
					//system_power_off();
					system_app_enter_poweroff(false);
					break;

				case MSG_REBOOT:
					SYS_EVENT_INF(EVENT_SYSTEM_REBOOT, msg.cmd);			
					system_power_reboot(msg.cmd);
					break;

				case MSG_NO_POWER:
					SYS_EVENT_INF(EVENT_SYSTEM_NO_POWER);
					sys_event_notify(SYS_EVENT_POWER_OFF);
					break;

				case MSG_BT_ENGINE_READY:
				{
					system_app_init_bte_ready();
				}
					break;

				case MSG_AUTOTEST_START_BT:
					{
#ifdef CONFIG_BT_MANAGER
						printk("autotest_start_BT_manager\n");
						bt_manager_init(system_bt_event_callback);
#endif
						break;
					}

				case MSG_BT_MGR_EVENT:
					system_btmgr_event_proc(&msg);
					break;

#ifdef CONFIG_PLAYTTS
				case MSG_TTS_EVENT:
				if (msg.cmd == TTS_EVENT_START_PLAY) {
					tts_manager_play_process();
				}
				break;
#endif


				case MSG_BAT_CHARGE_EVENT:
#ifdef CONFIG_BT_SELF_APP
					selfapp_report_bat();
#endif
#ifdef 	CONFIG_BT_LETWS
					if(bt_manager_letws_get_dev_role() == BTSRV_TWS_SLAVE)
						broadcast_tws_vnd_notify_bat_status();
#endif
				break;
#ifdef CONFIG_BT_SELF_APP
				case MSG_SELFAPP_APP_EVENT:
					if(SELFAPP_CMD_CALLBACK == msg.cmd) {
						selfapp_on_connect_event((msg.reserve >> 4)&0xF, msg.reserve&0xF, msg.value);
					} else if(SELFAPP_CMD_ROLE_UPDATE == msg.cmd) {
						if(msg.value == 1 || msg.value == 2 || !msg.value){
							selfapp_eq_cmd_switch_auracast(msg.value);
						}
						selfapp_group_status_update(msg.value);
					}else if(SELFAPP_CMD_LASTING_STEREO_MODE_UPDATE == msg.cmd) {
						selfapp_notify_lasting_stereo_status();
					}else if(SELFAPP_CMD_LEAUDIO_STATUS_UPDATE == msg.cmd) {
						selfapp_report_leaudio_status();
					}else if(SELFAPP_CMD_THREAD_TIMER_START == msg.cmd){
						selfapp_thread_timer_start(msg.value);
					}
					else if (SELFAPP_CMD_DISCONNECT == msg.cmd) {
						selfapp_disconnect_all_App();
					}
				break;
#endif

#if (defined(CONFIG_TWS) || defined(CONFIG_BT_LETWS))
			case MSG_TWS_EVENT:
			{
				SYS_LOG_INF("cmd %d, value %d\n", msg.cmd, msg.value);
				if (system_tws_event_handle(&msg) != 0){
					   ;
					}
					ingore_callback = 1;
			}
			break;
#endif
				case MSG_BATTERY_DISPLAY:
				{
					#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
					pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);
				//	power_manager_battery_display_handle(1, POWER_MANAGER_BATTER_10_SECOUND);
					#endif
				}
				break;

			/* for ats test GGEC */
			case MSG_ATS_LE_AUDIO:
				if(msg.cmd == 1){
					/* sink is connected to source... */
					user_set_auracast_sink_connected(true);
				}
				else if(msg.cmd == 2){
					/* sink disconnected from source */
					user_set_auracast_sink_connected(false);
				}
				break;
			case MSG_BLE_ADV_ENABLE:
				if(msg.cmd == 1){
					selfapp_set_system_status(SELF_SYS_RUNNING);
					selfapp_set_allowed_adv_crc(true);
					selfapp_set_allowed_join_party(true);
					sys_ble_advertise_deinit();
					sys_ble_advertise_init();
				}
				else if(msg.cmd == 0){
					selfapp_set_system_status(SELF_SYS_STANDBY);
					selfapp_set_allowed_adv_crc(false);
					selfapp_set_allowed_join_party(false);
					sys_ble_advertise_deinit();
					sys_ble_advertise_init();
					selfapp_enter_standby();
				}

				break;

				default:
					break;
			}
			if (msg.callback && !ingore_callback) {
				msg.callback(&msg, 0, NULL);
			}
			MSG_RECV_TIME_STAT_STOP(msg.cmd, msg.type, msg.value);
		}
		if (!terminated) {
			thread_timer_handle_expired();
		}

#ifdef CONFIG_TASK_WDT
		task_wdt_feed(TASK_WDT_CHANNEL_SYSTEM_APP);
#endif

	}

	_system_app_deinit();
}


APP_DEFINE(system, share_stack_area, sizeof(share_stack_area),
	CONFIG_APP_PRIORITY, BACKGROUND_APP, NULL, NULL, NULL,
	_system_app_loop, NULL);

