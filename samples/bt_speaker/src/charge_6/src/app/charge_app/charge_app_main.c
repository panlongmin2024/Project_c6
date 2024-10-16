#include "app/charge_app/charge_app.h"

#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#include <app_defines.h>
#include "tts_manager.h"
#include <thread_timer.h>
#include <global_mem.h>
#include <bt_manager.h>
#include <input_manager.h>
#include <soc_pm.h>
#include <sys_wakelock.h>
#include <soc_dvfs.h>
#include "desktop_manager.h"
#include <hotplug_manager.h>
#include <ui_manager.h>
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#ifdef CONFIG_SYS_LOG
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "charge_app"
#include <logging/sys_log.h>
#endif

#include "pd_manager.h"
#include "wltmcu_manager_supply.h"
#include "power_supply.h"
#include "power_manager.h"
#include "self_media_effect/self_media_effect.h"
#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

extern void hm_ext_pa_deinit(void);
extern void hm_ext_pa_init(void);

static int charge_app_state = CHARGING_APP_NORMAL;
static struct charge_app_t *p_charge_app_app;
extern bool run_mode_is_normal(void);
static struct thread_timer reset_timer;
static struct thread_timer check_bt_timer;
static u8_t power_off_no_tts;
extern int logic_mcu_ls8a10023t_otg_mobile_det(void);

#ifdef CONFIG_BT_SELF_APP
extern int selfapp_get_feedback_tone_ext(void);
#endif
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
extern void main_system_tts_set_play_warning_tone_flag(bool flag);
static bool charge_app_pa_status = false;
#endif

int charge_app_is_enter(void)
{
	int app_id = desktop_manager_get_plugin_id();
	printk("[%s/%d],app_id %d \n\n",__func__,__LINE__,app_id);
	if ((app_id == DESKTOP_PLUGIN_ID_CHARGER) ||(charge_app_state != CHARGING_APP_NORMAL)) {
		return 1;
	}
	return 0;
}

int charge_app_get_state(void)
{
	return charge_app_state;
}

void charge_app_set_wait_power_dowm(void)
{
	charge_app_state = CHARGING_APP_POWER_DOWM;
}

int charge_app_enter_cmd(void)
{
	if((run_mode_is_normal() == true) && (charge_app_state == CHARGING_APP_NORMAL)){
		//hotplug_charger_deinit();
		//charge_app_state = CHARGING_APP_INIT;
		system_app_launch_add(DESKTOP_PLUGIN_ID_CHARGER,1);
	}
	return 0;
}

int charge_app_exit_cmd(void)
{
	
	int app_id = desktop_manager_get_plugin_id();
	printk("[%s/%d],app_id %d \n\n",__func__,__LINE__,app_id);
	if ((app_id == DESKTOP_PLUGIN_ID_CHARGER) ||(charge_app_state != CHARGING_APP_NORMAL)) {
		if(charge_app_state != CHARGING_APP_OK){
			return 2;
		}
		desktop_manager_unlock();   
		system_app_launch_switch(DESKTOP_PLUGIN_ID_CHARGER,DESKTOP_PLUGIN_ID_BR_MUSIC);
		return 0;
	}
	return 1;
}

int ota_exit_cmd(void)
{
	
	int app_id = desktop_manager_get_plugin_id();
	printk("[%s/%d],app_id %d \n\n",__func__,__LINE__,app_id);
	if (app_id == DESKTOP_PLUGIN_ID_OTA) {
	      return 1;
		}
	
	return 0;
}

static void charge_system_tts_event_nodify(u8_t * tts_id, u32_t event)
{
	switch (event) {
	case TTS_EVENT_START_PLAY:
		SYS_LOG_INF("%s start", tts_id);
		#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
		if(!memcmp(tts_id,"c_err.mp3",9)){
			main_system_tts_set_play_warning_tone_flag(true);
		}
		#endif
		input_manager_lock();
		break;
	case TTS_EVENT_STOP_PLAY:
		SYS_LOG_INF("%s end", tts_id);
		if(memcmp(tts_id,"poweroff.mp3",sizeof("poweroff.mp3")) == 0){
			input_manager_lock();
			tts_manager_lock();
			tts_manager_clear_all();
			os_sleep(100);
			printk("wati 100ms output\n");
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
			charge_app_pa_status = false;
#endif			
			hm_ext_pa_deinit();		
			external_dsp_ats3615_deinit();
			if(charge_app_state == CHARGING_APP_INIT)
				charge_app_state = CHARGING_APP_TTS_DONE;
		}
		#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
		if(memcmp(tts_id,"c_err.mp3",sizeof("c_err.mp3")) == 0){
			if(charge_app_state == CHARGING_APP_INIT)
				charge_app_state = CHARGING_APP_TTS_DONE;
			main_system_tts_set_play_warning_tone_flag(false);
		}
		#endif
		break;
	default:
		break;
	}
}

extern bool sys_check_standby_state(void);

int charge_app_real_exit_deal(void)
{
	int app_id = desktop_manager_get_plugin_id();
	if (((app_id == DESKTOP_PLUGIN_ID_CHARGER) && (charge_app_state == CHARGING_APP_OK)) || sys_check_standby_state()){
		SYS_LOG_INF("real power off\n");
		struct app_msg msg = {0};
        msg.type = MSG_PD_EVENT;
		msg.value = PD_EVENT_POWER_OFF;
        send_async_msg(APP_ID_MAIN, &msg);	
	}
	return 0;
}
static uint8_t power_first_factory_reset_flag = 0;
uint8_t get_power_first_factory_reset_flag(void)
{
  return power_first_factory_reset_flag;
}

void set_power_first_factory_reset_flag(uint8_t value)
{
    power_first_factory_reset_flag = value ;
}

static void charge_app_timer(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	thread_timer_stop(&reset_timer);
	if(get_property_factory_reset_flag()){
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
		pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
		led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(0));
		set_power_first_factory_reset_flag(1);
		set_property_factory_reset_flag(0);
	    pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
	}
	#ifdef CONFIG_BT_SELF_APP
	if((power_off_no_tts == 1)&&(charge_app_state == CHARGING_APP_INIT)){

		charge_system_tts_event_nodify("poweroff.mp3",TTS_EVENT_STOP_PLAY);
	}
	#endif
}
static void charge_app_check_bt_timer(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if(((btif_br_get_connected_device_num() == 0) && (charge_app_state == CHARGING_APP_TTS_DONE)) || \
	((os_uptime_get_32() - p_charge_app_app->tts_start_time) > 25000)){
		charge_app_state = CHARGING_APP_OK;
		thread_timer_stop(&check_bt_timer);
		hotplug_charger_init();	
		bt_manager_halt_ble();
		printk("charge_app_check_bt_timer %d %d \n",charge_app_state,(os_uptime_get_32() - p_charge_app_app->tts_start_time));
	}
}
static int _charge_app_init(void *p1, void *p2, void *p3)
{
	int ret = 0;

	p_charge_app_app = app_mem_malloc(sizeof(struct charge_app_t));
	if (!p_charge_app_app) {
		SYS_LOG_ERR("malloc fail!\n");
		return -ENOMEM;
	}
	hotplug_charger_deinit();
	charge_app_state = CHARGING_APP_INIT;
	desktop_manager_lock();
	input_manager_lock();
	charge_app_view_init();

	sys_event_send_message_new(MSG_BLE_ADV_ENABLE, 0, NULL, 0);

	#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
	if(mcu_ui_get_poweron_from_charge_warnning_status())
	{
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
		charge_app_pa_status = true;
#endif	
		pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));	
		pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,CHARGING_WARNING);
	}
	else
	#endif
	{
		if(!get_property_factory_reset_flag()){
			//pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
			//pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_UNLINKED);
			led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
			pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_PWR_LED_ON_0_5S);//PWROFF pwr&bat off 500ms
			pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(0));
		}else{
			pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));
			pd_srv_event_notify(PD_EVENT_BT_LED_DISPLAY,SYS_EVENT_BT_CONNECTED);
			pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,1);
			pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_ON); // all led turn on
			pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(1)|AC_LED_STATE(1)|BAT_LED_STATE(1));
		}
		thread_timer_init(&reset_timer, charge_app_timer, NULL);
		thread_timer_start(&reset_timer,2000,2000);
	}
	tts_manager_add_event_lisener(charge_system_tts_event_nodify);
    pd_set_app_mode_state(CHARGING_APP_MODE);
	pd_manager_send_disc();
	system_set_power_run_mode(1);
	bt_manager_set_autoconn_info_need_update(0);
	//bt_manager_halt_ble();
	bt_manager_auto_reconnect_stop();
	bt_manager_end_pair_mode();
	bt_manager_set_user_visual(1,0,0,0);
	bt_manager_disconnect_all_device_power_off();
	system_app_set_auracast_mode(0);
	//self_music_effect_ctrl_set_enable(1);

	//lear all tts
	tts_manager_lock();
	tts_manager_clear_all();
	tts_manager_unlock();

	power_off_no_tts = 0;
	#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
	if(mcu_ui_get_poweron_from_charge_warnning_status())
	{
		tts_manager_play("c_err.mp3", PLAY_IMMEDIATELY);
		hotplug_charger_init();	

	}
	else
	#endif
	{
		ret = tts_manager_play("poweroff.mp3", PLAY_IMMEDIATELY);
		if(ret != 0 )
			power_off_no_tts = 1;
	}
	p_charge_app_app->tts_start_time = os_uptime_get_32();
	thread_timer_init(&check_bt_timer, charge_app_check_bt_timer, NULL);
	thread_timer_start(&check_bt_timer,200,200);
	//tts_manager_lock();
	//sys_standby_time_set(5,5);
	sys_wake_lock(WAKELOCK_CHARGE_MODE);
	SYS_LOG_INF("init finished\n");

	return ret;
}

static int _charge_app_exit(void)
{
	if (!p_charge_app_app)
			return 0;

	app_mem_free(p_charge_app_app);
	p_charge_app_app = NULL;

	if(thread_timer_is_running(&reset_timer)){
		thread_timer_stop(&reset_timer);
	}

	if(thread_timer_is_running(&check_bt_timer)){
		thread_timer_stop(&check_bt_timer);
	}

	if(charge_app_state != CHARGING_APP_POWER_DOWM){
		soc_dvfs_set_level(SOC_DVFS_LEVEL_HIGH_PERFORMANCE, "power_on");
		pd_set_app_mode_state(BTMODE_APP_MODE);
		pd_manager_pd_wakeup();
		pd_manager_send_disc();
		charge_app_view_deinit();
		system_set_power_run_mode(0);

		sys_event_send_message_new(MSG_BLE_ADV_ENABLE, 1, NULL, 0);

	#ifdef CONFIG_BT_SELF_APP
		if(selfapp_get_feedback_tone_ext()){
			int i = 0;
			while(tts_manager_is_locked() && (i < 10)){
				tts_manager_unlock();
				i++;
			}
		}
	#endif

	#ifdef CONFIG_PROPERTY
		property_flush_req(NULL);
	#endif

		#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
		mcu_ui_set_poweron_from_charge_warnning_status(0);
		#endif
		external_dsp_ats3615_load(0);
		hm_ext_pa_init();

		bt_manager_init_dev_volume();
		bt_manager_set_user_visual(0,0,0,0);
		bt_manager_set_autoconn_info_need_update(1);
		bt_manager_powon_auto_reconnect(0); 
		bt_manager_resume_ble();
		tts_manager_remove_event_lisener(charge_system_tts_event_nodify);
		tts_manager_unlock();
		tts_manager_disable_filter();
		tts_manager_play("poweron.mp3", PLAY_IMMEDIATELY);

		app_manager_thread_exit(APP_ID_CHARGE_APP_NAME);
		input_manager_unlock();
		sys_wake_unlock(WAKELOCK_CHARGE_MODE);
		charge_app_state = CHARGING_APP_NORMAL; 
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_HIGH_PERFORMANCE, "power_on");
		//sys_standby_time_set(CONFIG_AUTO_STANDBY_TIME_SEC,CONFIG_AUTO_POWEDOWN_TIME_SEC);
		SYS_LOG_INF("exit finished\n");
	}else{
		int cnt = 0;
		ui_view_delete(CHARGE_APP_VIEW);
		tts_manager_remove_event_lisener(charge_system_tts_event_nodify);
		app_manager_thread_exit(APP_ID_CHARGE_APP_NAME);
		while(tts_manager_get_same_tts_in_list("poweroff.mp3")){
			k_sleep(10);
			if(cnt++ > 1000){
				break;
			}
			#ifdef CONFIG_TASK_WDT
			task_wdt_feed(TASK_WDT_CHANNEL_MAIN_APP);
			#endif
		}
		tts_manager_lock();//have play power off tone;
		SYS_LOG_INF("wait power dowm\n");
	}
	return 0;
}

struct charge_app_t *charge_app_get_app(void)
{
	return p_charge_app_app;
}

static int charge_app_msg_pro(struct app_msg *msg)
{
	SYS_LOG_INF("msg type %d\n",msg->type);
	switch (msg->type) {
	case MSG_EXIT_APP:
	case MSG_LOW_POWER:
		
		break;
	case MSG_INPUT_EVENT:
		charge_app_input_event_proc(msg);
		break;
	case MSG_TTS_EVENT:
		charge_app_tts_event_proc(msg);
		break;

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	case MSG_PD_BAT_SINK_FULL:
	{
		struct app_msg msg = {0};
        msg.type = MSG_PD_EVENT;
		msg.value = PD_EVENT_POWER_OFF;
        send_async_msg(APP_ID_MAIN, &msg);	
		logic_mcu_ls8a10023t_otg_mobile_det();							// charge trigrering mode of logic ic to rising edge 
		break;
	}
#endif
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
	case MSG_CHARGE_WARNNING_OPEN_PA:
		if(charge_app_pa_status == false)
		{
			pd_srv_event_notify(PD_EVENT_LED_LOCK,BT_LED_STATE(0)|AC_LED_STATE(0)|BAT_LED_STATE(0));
			external_dsp_ats3615_load(0);
			hm_ext_pa_init();
			//bt_manager_init_dev_volume();
			tts_manager_unlock();
			charge_app_pa_status = true;
		}
		break;
#endif
	default:
		SYS_LOG_ERR("error: 0x%x!\n", msg->type);
	}

	return 0;
	
}

/*APP_DEFINE(charge_app, share_stack_area, sizeof(share_stack_area),
	   CONFIG_APP_PRIORITY, FOREGROUND_APP, NULL, NULL, NULL,
	   charge_app_main_loop, NULL);*/

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_CHARGER, _charge_app_init, _charge_app_exit, charge_app_msg_pro, \
	NULL, NULL, NULL, NULL);
