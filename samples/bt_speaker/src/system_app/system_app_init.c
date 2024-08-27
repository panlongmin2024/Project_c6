/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
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
#include "app_defines.h"
#include <sys_manager.h>
#include <ui_manager.h>
#include "app_ui.h"
#include "system_util.h"
#include <thread_timer.h>
#ifdef CONFIG_TWS_UI_EVENT_SYNC
#include "../include/list.h"
#endif
#include <soc_dvfs.h>
#include <stream.h>

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif

#ifdef CONFIG_OTA_APP
#include "../ota/ota_app.h"
#endif

#ifdef CONFIG_TOOL
#include "../tool/tool_app.h"
#endif

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#ifdef CONFIG_BT_SELF_APP
#include <selfapp_api.h>
#endif


#include <logic.h>
#include "system_le_audio.h"
#include "system_app.h"
#include <stack_backtrace.h>

LOG_MODULE_REGISTER(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <run_mode/run_mode.h>
#include <wltmcu_manager_supply.h>
#include <power_manager.h>
#ifdef CONFIG_C_EXTERNAL_DSP_ATS3615
#include <external_dsp/ats3615/ats3615_head.h>
#endif
#ifdef CONFIG_C_AMP_TAS5828M
#include <driver/amp/tas5828m/tas5828m_head.h>
#endif
#endif

#ifdef CONFIG_BT_CONTROLER_BQB
extern int btdrv_get_bqb_mode(void);
#endif

enum {
	SYS_INIT_NORMAL_MODE,
	SYS_INIT_ATT_TEST_MODE,
	SYS_INIT_ALARM_MODE,
};

int sys_ble_advertise_init(void);
void sys_ble_advertise_deinit(void);
void system_audio_policy_init(void);
void system_tts_policy_init(void);

extern void system_library_version_dump(void);
extern void trace_init(void);

extern int card_reader_mode_check(void);
extern int check_is_wait_adfu(void);
extern bool dc_power_in_status_read(void);
#ifdef CONFIG_TEST_BLE_NOTIFY
extern void ble_test_init(void);
#endif

#ifdef CONFIG_GFP_PROFILE
extern int gfp_ble_stream_init(void);
#endif

#ifdef CONFIG_ACTIONS_ATT
void autotest_start(void);
int get_autotest_connect_status(void);
#endif

extern int system_btmgr_configs_update(void);
extern void system_input_handle_init(void);
extern int system_event_map_init(void);

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
int acts_xspi_nor_enable_suspend(uint8_t enable);
#endif

static bool att_enter_bqb_flag = false;
bool bt_manager_config_inited =  false;
bool bt_manager_power_on_setup =  true;

#ifdef CONFIG_BT_CONTROLER_BQB
bool main_system_is_enter_bqb(void)
{
	printk("[%s/%d] , bqb:%d \n\n",__func__,__LINE__,att_enter_bqb_flag);
	return att_enter_bqb_flag == true;
}
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static bool tts_is_play_warning_tone = false;
void main_system_tts_set_play_warning_tone_flag(bool flag)
{
	tts_is_play_warning_tone = flag;
	SYS_LOG_INF("tts_is_play_warning_tone:%d \n",tts_is_play_warning_tone);
}

bool main_system_tts_get_play_warning_tone_flag(void)
{
	SYS_LOG_INF("tts_is_play_warning_tone:%d \n",tts_is_play_warning_tone);

	return tts_is_play_warning_tone;
}

extern int btdrv_get_bqb_mode(void);
extern void hm_ext_pa_deinit(void);

static void main_system_tts_event_nodify(u8_t * tts_id, u32_t event)
{
	switch (event) {
	case TTS_EVENT_START_PLAY:
		SYS_LOG_INF("%s start \n",tts_id);
		if(!memcmp(tts_id,"c_err.mp3",9)){
			main_system_tts_set_play_warning_tone_flag(true);
		}
		break;
	case TTS_EVENT_STOP_PLAY:
		SYS_LOG_INF("%s end \n", tts_id);
		tts_manager_disable_keycode_check();
		if(!memcmp(tts_id,"poweroff.mp3",12)){
			if(btdrv_get_bqb_mode()){
				sys_pm_reboot(7);
			}
			if(run_mode_is_demo())
			{
				// input_manager_lock();
				// tts_manager_lock();
				// tts_manager_wait_finished(1);
				os_sleep(50);
				printk("wati 50ms output\n");
				hm_ext_pa_deinit();		
				external_dsp_ats3615_deinit();
			}

		}
		if(!memcmp(tts_id,"c_err.mp3",9)){
			main_system_tts_set_play_warning_tone_flag(false);
		}
		break;
	default:
		break;
	}
}

static struct thread_timer check_adfu_timer;
static u32_t  adfu_wait_time;
static void main_system_check_adfu_timer(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	static u8_t power_led_cnt = 0 ,fairst = 0;
	power_led_cnt++;
	if(power_led_cnt < 2){
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
	}else if(power_led_cnt < 6){
		led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
	}else{
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
		power_led_cnt = 0;
	}

	if((os_uptime_get_32() - adfu_wait_time)>1200000){
		pd_srv_sync_exit();
	}

	if(fairst == 0){
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
		pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF); // all led turn on
		led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(1));
		fairst = 1;
		power_led_cnt = 3;
	}else{
		if(dc_power_in_status_read())
			sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	}

}
#endif


#ifdef CONFIG_TWS
static int app_get_expect_tws_role(void)
{
	int role = BTSRV_TWS_NONE;

#ifdef CONFIG_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_LINEIN) == HOTPLUG_IN) {
		role = BTSRV_TWS_MASTER;
	}
#endif

	SYS_LOG_INF("%d\n", role);
	return role;
}
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
extern u16_t g_reboot_type;
extern u8_t g_reboot_reason;
extern void user_app_later_init(void);
extern bool main_get_enter_att_state(void);
#endif
#ifdef CONFIG_WLT_ATS_ENABLE
extern bool ats_wlt_get_enter_state(void);
extern void ats_wlt_start(void);
#endif

#ifdef CONFIG_CHARGER_APP
static void system_notify_enter_charger_mode(void)
{

	struct app_msg msg = {0};

	SYS_LOG_INF("start charger");
	msg.type = MSG_CHARGER_MODE;
	msg.cmd = 1;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static void system_wait_exit_charger_mode(void)
{
	int terminaltion = 0;
	int wait_adfu = 0;
	struct app_msg msg = {0};

	system_ready();
	if(check_is_wait_adfu() && (main_system_is_enter_bqb() == 0)){
		thread_timer_init(&check_adfu_timer, main_system_check_adfu_timer, NULL);
		thread_timer_start(&check_adfu_timer,2000,500);
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
		pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_CONNECTED);
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,1);
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_ON); // all led turn on
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(1));
		wait_adfu = 1;
		adfu_wait_time = os_uptime_get_32();
	}

	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);
			switch (msg.type) {
			case MSG_CHARGER_MODE:
				if((!msg.cmd) && (wait_adfu == 0))
					terminaltion = true;
				break;
			default:
				SYS_LOG_ERR("error type: 0x%x! \n", msg.type);
				continue;
			}
			if (msg.callback != NULL)
				msg.callback(&msg, 0, NULL);
		}
		thread_timer_handle_expired();
	}
	SYS_LOG_INF("exit charger");
}

#endif

#ifdef CONFIG_ACT_EVENT
static void act_event_callback_process(uint8_t event)
{
	if(event == ACT_EVENT_FLASH_FULL){
		act_event_set_level_filter("all", 0);
	}
}
#endif

static void system_app_ota_init(void)
{
#ifdef CONFIG_OTA_APP
		ota_app_init();
#ifdef CONFIG_OTA_BACKEND_SDCARD
		ota_app_init_sdcard();
#endif
#endif
}

void system_app_init(void)
{
	bool play_welcome = true;
#ifdef CONFIG_BT_MANAGER
	bool init_bt_manager = true;
#endif
	u16_t reboot_type = 0;
	u8_t reason = 0;

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	uint8_t flag_print_dma_init = 0;
#endif

//#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#if 1
	system_power_get_reboot_reason(&reboot_type, &reason);
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	g_reboot_type = reboot_type;
	g_reboot_reason = reason;
	if(REBOOT_REASON_OTA_FINISHED & g_reboot_reason){
		extern void bt_mcu_set_first_power_on_flag(bool flag);
		bt_mcu_set_first_power_on_flag(1);
	     extern void set_batt_led_display_timer(int deley100ms);
		 extern void pd_manager_set_poweron_filte_battery_led(uint8_t flag);
		  set_batt_led_display_timer(-1);
		  SYS_LOG_INF("[%d];  REBOOT_REASON_OTA_FINISHED", __LINE__);
		  pd_manager_set_poweron_filte_battery_led(WLT_FILTER_DISCHARGE_POWERON);
		}
#endif	
#else
	reboot_type = g_reboot_type;
	reason = g_reboot_reason;
#endif

#ifdef CONFIG_FIRMWARE_VERSION
	fw_version_dump_current();
#endif

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
	acts_xspi_nor_enable_suspend(true);
#endif

	system_library_version_dump();

	system_init();

	system_audio_policy_init();

	system_tts_policy_init();

	system_event_map_init();

	system_input_handle_init();

	void user_uuid_init(void);
	user_uuid_init();
#ifdef CONFIG_BT_MANAGER
	system_btmgr_configs_update();
#endif

	//trace_init();
#ifdef CONFIG_BT_SELF_APP
	selfapp_config_init();
#endif

#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
#ifdef CONFIG_WDT_MODE_RESET
		if (reboot_type == REBOOT_TYPE_WATCHDOG) {
			play_welcome = false;
		} else {
			esd_manager_reset_finished();
		}
#else
		if (reboot_type == REBOOT_TYPE_WATCHDOG
		    || reboot_type == REBOOT_TYPE_HW_RESET) {
			play_welcome = false;
		} else {
			esd_manager_reset_finished();
		}
#endif
	} else {
		esd_manager_reset_finished();
	}
#endif

    bt_manager_power_on_setup = true;

	if ((reboot_type == REBOOT_TYPE_SF_RESET)
	    && ((reason == REBOOT_REASON_GOTO_BQB_ATT) || (reason == REBOOT_REASON_GOTO_BQB ))) {
		play_welcome = false;
#ifdef CONFIG_BT_CONTROLER_BQB
		att_enter_bqb_flag = true;
#endif
#ifdef CONFIG_BT_PTS_TEST
		bt_manager_power_on_setup = false;
#endif
	}

	if (!play_welcome) {
#ifdef CONFIG_PLAYTTS
		tts_manager_lock();
#endif
	}

#ifdef CONFIG_BT_CONTROLER_BQB
	if (btdrv_get_bqb_mode() != 0) {
		att_enter_bqb_flag = true;
	}
#endif

	if( 1
#ifdef CONFIG_BT_CONTROLER_BQB		
		&&(!att_enter_bqb_flag)
#endif	
#ifdef CONFIG_WLT_ATS_ENABLE
	&& (!ats_wlt_get_enter_state())
#endif
#if (defined CONFIG_TOOL && defined CONFIG_ACTIONS_ATT)
	&& (!main_get_enter_att_state())
#endif
	)	
	{
		pd_srv_sync_init();
	}

	if (!att_enter_bqb_flag && reason != REBOOT_REASON_OTA_FINISHED ) {
		bool enter_stub_tool = false;

#if (defined CONFIG_TOOL && defined CONFIG_ACTIONS_ATT)
#if 0
		if (tool_att_connect_try() == 0) {
			enter_stub_tool = true;
			init_bt_manager = false;
		}
#else
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
		if(main_get_enter_att_state()){
#else
		if (get_autotest_connect_status() == 0) {
#endif			
			enter_stub_tool = true;
			init_bt_manager = false;
#ifdef CONFIG_PLAYTTS
			tts_manager_lock();
#endif
			autotest_start();
		}
#endif
#endif

#ifdef CONFIG_WLT_ATS_ENABLE
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
		/* wlt factory test start!!! */
		bool enter_wlt_fac_test = false;
		if(ats_wlt_get_enter_state() && (!main_get_enter_att_state())){
			enter_wlt_fac_test = true;
			init_bt_manager = false;
#ifdef CONFIG_PLAYTTS
			tts_manager_lock();
#endif			
			//trace_init();
			//ats_wlt_start();
		}
#endif
#endif
		system_app_ota_init();

#ifdef CONFIG_WLT_ATS_ENABLE
		if (enter_stub_tool == false && enter_wlt_fac_test == false) {
#else
		if (enter_stub_tool == false) {
#endif
#ifdef CONFIG_CARD_READER_APP
			if (usb_hotplug_device_mode()
			    && !(reason == REBOOT_REASON_NORMAL)) {

				//system_ready();
				if (card_reader_mode_check() == NO_NEED_INIT_BT) {
					init_bt_manager = false;
				}
			} else
#endif
			{
#ifdef CONFIG_CHARGER_APP

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
				// for charger_mode can use uart dma.
#ifdef CONFIG_ACTIONS_TRACE
				if (flag_print_dma_init == 0)
				{
					flag_print_dma_init = 1;
#ifdef CONFIG_TOOL
					if (tool_get_dev_type() == TOOL_DEV_TYPE_UART0) {
						/* stub uart and trace both use uart0, forbidden trace dma mode */
						//trace_dma_print_set(false);
					}
#endif
					trace_init();
				}
#endif
#endif

				SYS_LOG_INF("[%d], demo_mode:%d, battery_capacity:%d \n\n", __LINE__, run_mode_is_demo(), power_manager_get_battery_capacity());

				if((!run_mode_is_demo())||(check_is_wait_adfu())){
					system_notify_enter_charger_mode();
					system_ready();
					system_wait_exit_charger_mode();
				}
				else if(run_mode_is_demo() && (power_manager_get_battery_capacity() < DEFAULT_NOPOWER_CAP_LEVEL))
				{
					system_notify_enter_charger_mode();
					system_ready();
					system_wait_exit_charger_mode();
				}
				else{
					system_ready();
				}
#else
				system_ready();
#endif
			}
		}
	}else{
		//system_app_ota_init();

		system_ready();
	}

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	printf("%s:%d led_manager_set_display\n", __func__, __LINE__);
	led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);						// power led
	pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);	
#endif


#ifdef CONFIG_ACTIONS_TRACE
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	if (flag_print_dma_init == 0)
	{
		flag_print_dma_init = 1;
#endif
#ifdef CONFIG_TOOL
		if (tool_get_dev_type() == TOOL_DEV_TYPE_UART0) {
			/* stub uart and trace both use uart0, forbidden trace dma mode */
			//trace_dma_print_set(false);
		}
#endif
		trace_init();
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	}
#endif
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//user_app_later_init();
	tts_manager_add_event_lisener(main_system_tts_event_nodify);
	if(run_mode_is_demo()&&(!dc_power_in_status_read())){
		SYS_LOG_INF("[%d] run_mode_is_demo, but No dc_power_in \n", __LINE__);
		//system_power_off();
	}else{
#ifdef CONFIG_BT_CONTROLER_BQB
		if(!main_system_is_enter_bqb())
#endif	
		{	
			#ifdef CONFIG_ACT_EVENT
			act_event_init(act_event_callback_process);
			#endif
			//tts_manager_play_process();
			pd_srv_event_notify(PD_EVENT_SOURCE_REFREST,0);
		}
	}
#endif	

#ifdef CONFIG_BT_MANAGER
	if (init_bt_manager && bt_manager_config_inited) {
#if 0

		if (run_mode_is_demo() == false)
		{
#endif
			bt_manager_init(system_bt_event_callback);
#ifdef CONFIG_TWS
			extern void audio_tws_pre_init(void);
			audio_tws_pre_init();
			bt_manager_tws_register_config_expect_role_cb(app_get_expect_tws_role);
#endif
#if 0
		}
		else 
		{
			// run a null foreground application, and wait usb insert, and enter demo application.
			// demo mode is not need bt.
			//system_app_launch_for_demo_mode(SYS_INIT_NORMAL_MODE);

			bt_manager_power_on_setup = false;
		}
#endif
	}
#endif




#ifdef CONFIG_TEST_BLE_NOTIFY
	ble_test_init();
#endif

#ifdef CONFIG_BT_ADV_MANAGER
#ifdef CONFIG_BT_LEA_PTS_TEST
	if (!bt_pts_is_enabled())
#endif
	{
		sys_ble_advertise_init();
	}
#endif

#ifdef CONFIG_LOGIC_ANALYZER
	logic_init();
#endif

#ifdef CONFIG_TOOL
extern int tool_init(void);
	//tool_init();
#endif

/* #ifdef CONFIG_ACT_EVENT
	act_event_init(act_event_callback_process);
#endif */
// extern void wlt_hm_ext_pa_start(void);
// 	wlt_hm_ext_pa_start();

}

static void main_freq_init(void)
{
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE, "init");

#if (defined(CONFIG_DSP_IP_VENDOR_MUSIC_EFFECT_LIB) || defined(CONFIG_DSP_IP_VENDOR_VOICE_EFFECT_LIB))
	soc_dvfs_set_level(SOC_DVFS_LEVEL_CSB_PERFORMANCE, "init");
#else
	//Enter S2 and set the system clock to 60M
	soc_dvfs_set_level(SOC_DVFS_LEVEL_IDLE, "init");
#endif

#endif
}

#ifdef CONFIG_BT_LEATWS
extern int sys_ble_leatws_adv_register(void);
extern int sys_ble_leatws_adv_unregister(void);
#endif

void system_app_init_bte_ready(void)
{
	main_freq_init();

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE	
	if(REBOOT_REASON_OTA_FINISHED & g_reboot_reason){
		extern u8_t bt_manager_set_ota_reboot(u8_t reboot_type);
		bt_manager_set_ota_reboot(1);
	}
#endif

	if(run_mode_is_demo() == false){
		bt_manager_start_wait_connect();
		if(bt_manager_power_on_setup){
			bt_manager_powon_auto_reconnect(0);
		}
		else{
			bt_manager_powon_auto_reconnect(BT_MANAGER_REBOOT_ENTER_PAIR_MODE);
		}
	}
#ifdef CONFIG_OTA_BACKEND_BLUETOOTH
	ota_app_init_bluetooth();
#endif
#ifdef CONFIG_BT_SELF_APP
#ifdef CONFIG_LOGSRV_SELF_APP
	selfapp_init(system_app_log_transfer);
#else
	selfapp_init(NULL);
#endif
#ifdef CONFIG_OTA_SELF_APP
	ota_app_init_selfapp();
#endif
#endif
#ifdef CONFIG_ACTIONS_ATT
	extern int act_att_notify_bt_engine_ready(void);
	act_att_notify_bt_engine_ready();
#endif

#ifdef CONFIG_BT_LEA_PTS_TEST
	if (bt_pts_is_enabled()) {
		pts_le_audio_init();
	} else
#endif /*CONFIG_BT_LEA_PTS_TEST*/
	{
		bt_le_audio_init();
#ifdef CONFIG_BT_LEATWS
#if defined(CONFIG_BT_LEATWS_FL)
		bt_le_audio_exit();
		sys_ble_leatws_adv_unregister();
		sys_ble_leatws_adv_register();
		leatws_set_audio_channel(BT_AUDIO_LOCATIONS_FL);
		leatws_notify_leaudio_reinit();
#elif defined(CONFIG_BT_LEATWS_FR)
		bt_le_audio_exit();
		sys_ble_leatws_adv_unregister();
		leatws_set_audio_channel(BT_AUDIO_LOCATIONS_FR);
		leatws_scan_start();
#endif
#endif
	}

	struct app_msg msg = {0};

	msg.type = MSG_BT_ENGINE_READY;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}


void ctrl_dump_all_info(void);
void ctrl_mem_dump_info_all(void);

static void crash_dump_version_info(void)
{
	system_library_version_dump();
	fw_version_dump_current();
	ctrl_dump_all_info();
	ctrl_mem_dump_info_all();
}

CRASH_DUMP_REGISTER(dump_version_info, 1) =
{
    .dump = crash_dump_version_info,
};

#ifdef CONFIG_DEBUG_RAMDUMP

#include <debug/ramdump.h>
#include <ota_upgrade.h>

static void crash_dump_ramdump(void)
{
	if(!ota_upgrade_is_ota_running()){
		if(ramdump_check() != 0){
			printk("start ramdump...\n");
			ramdump_save();
		}else{
			printk("ramdump data existed, override\n");
			ramdump_save();
		}
	}else{
		printk("ota is running, skip ramdump!\n");
	}
}

static void crash_dump_ramdump_print(void)
{
	int i;

	printk("first ramdump...\n");

	ramdump_print();

	for(i = 0; i < 500; i++){
		k_busy_wait(1000);
	}

	printk("second ramdump...\n");

	ramdump_print();
}

CRASH_DUMP_REGISTER(ramdump_register, 10) =
{
    .dump = crash_dump_ramdump,
};

CRASH_DUMP_REGISTER(ramdump_print_register, 99) =
{
    .dump = crash_dump_ramdump_print,
};


#endif



#if defined(CONFIG_IRQ_STAT)
void dump_irqstat(void);

CRASH_DUMP_REGISTER(dump_irqstat_info, 11) =
{
    .dump = dump_irqstat,
};
#endif

#if defined(CONFIG_THREAD_STACK_INFO)
void kernel_dump_all_thread_stack_usage(void);

CRASH_DUMP_REGISTER(dump_stacks_analyze, 12) =
{
    .dump = kernel_dump_all_thread_stack_usage,
};
#endif
