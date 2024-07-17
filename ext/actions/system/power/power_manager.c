/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file power manager interface
 */
#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "power"
#endif
#include <logging/sys_log.h>
#include <os_common_api.h>
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <sys_monitor.h>
#include <property_manager.h>
#include <power_supply.h>
#include <string.h>
#ifdef CONFIG_POWER_SMART_CONTROL
#include <volume_manager.h>
#endif

#include "power_manager.h"
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#include "wltmcu_manager_supply.h"
#include <board.h>

#define DEFAULT_BOOTPOWER_LEVEL		3100000
#define DEFAULT_NOPOWER_LEVEL	    3400000
#define DEFAULT_LOWPOWER_LEVEL		3600000


#define DEFAULT_REPORT_PERIODS	(60*1000)

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
//0.1 C°
#define DEFUALT_HIAHERTEMP_POWER_LEVEL					700
#define DEFUALT_HIAHTEMP_POWER_LEVEL					600
#define DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL0				520
#define DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL1				540
#define DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL2				560
#define DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL3				580
#define DEFUALT_LOWTEMP_POWER_LEVEL						-180

#define DEFAULT_NOPOWER_CAP_LEVEL	    				5
#define DEFAULT_JUST_VOLUME_PERIODS						(300*1000)
#define DEFAULT_POWER_OFF_PERIODS						(8*1000)
#endif	

struct power_manager_info {
	struct device *dev;
	bat_charge_callback_t cb;
	bat_charge_event_t event;
	bat_charge_event_para_t *para;
	int nopower_level;
	int lowpower_level;
	int current_vol;
	int current_cap;
	int slave_vol;
	int slave_cap;
	int battary_changed;
#ifdef CONFIG_POWER_SMART_CONTROL
	int smart_control;
#endif
	uint32_t charge_full_flag;
	uint32_t report_timestamp;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
	int battary_led_need_change;
	int current_temperature;
	uint32_t report_justvol_timestamp;
#endif	
	int last_cap;
	uint32_t report_last_cap_timestamp;
};

struct power_manager_info *power_manager = NULL;
uint8_t power_first_get_cap_flag = 0;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
extern bool sys_check_standby_state(void);
extern void battery_status_remaincap_display_handle(uint8_t status, u16_t cap, int led_status);
extern void battery_charging_remaincap_is_full(void);
int power_manager_get_charge_status(void);
int power_manager_get_battery_capacity(void);

int power_manager_get_just_volume_step(void)
{
	int step = 0;
	if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL3){
		step = 4;
	}
	else if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL2){
		step = 3;
	}
	else if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL1){
		step = 2;
	}
	else if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL0){
		step = 1;
	}
	else{
		step = 0;
	}
	return step;
}

extern bool run_mode_is_demo(void);
void power_manager_battery_display_handle(int led_status ,bool key_flag)
{
	int temp_status = power_manager_get_charge_status();
	int current_cap = power_manager_get_battery_capacity();
	
	if(power_first_get_cap_flag == 0){
		printk("[%s/%d],power_first_get_cap_flag = %d \n\n",__func__,__LINE__,power_first_get_cap_flag);
		return;
	}

	if(key_flag){
		if(temp_status == POWER_SUPPLY_STATUS_DISCHARGE
		||temp_status == POWER_SUPPLY_STATUS_FULL
		){
			key_flag = 1;

				if((temp_status == POWER_SUPPLY_STATUS_FULL)
					&& (pd_get_app_mode_state() != CHARGING_APP_MODE))
				{
					temp_status = POWER_SUPPLY_STATUS_DISCHARGE;
				}
			
		}
		else{	
			key_flag = 0;
		}	
	}
	battery_status_remaincap_display_handle(temp_status,current_cap,led_status);
}

void power_manager_battery_display_reset(void)
{
	int temp_status = power_manager_get_charge_status();
	int current_cap = power_manager_get_battery_capacity();
	if(temp_status == POWER_SUPPLY_STATUS_DISCHARGE
	&& current_cap > BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
	{
		battery_charging_remaincap_is_full();
	}
	else
	{
		battery_status_remaincap_display_handle(temp_status,current_cap,0);
	}	
}
#endif

void power_manager_status_notify(void)
{		
	//notify system charge status
	struct app_msg msg = {0};
	msg.type = MSG_BAT_CHARGE_EVENT;
	msg.cmd = 0;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

void power_supply_report(bat_charge_event_t event, bat_charge_event_para_t *para)
{
	if (!power_manager) {
		return;
	}
	SYS_LOG_INF("event %d\n", event);
	switch (event) {
	case BAT_CHG_EVENT_DC5V_IN:
	#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
		//power_manager->battary_led_need_change = 1;
	#endif	
		break;
	case BAT_CHG_EVENT_DC5V_OUT:
		break;
	case BAT_CHG_EVENT_CHARGE_START:	
		break;
	case BAT_CHG_EVENT_CHARGE_FULL:
		power_manager->charge_full_flag = 1;
	#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
		power_manager->battary_led_need_change = 1;
	#endif				
		break;

	case BAT_CHG_EVENT_VOLTAGE_CHANGE:
		SYS_LOG_INF("voltage change %u uV\n", para->voltage_val);
		power_manager->current_vol = para->voltage_val;		
		break;

	case BAT_CHG_EVENT_CAP_CHANGE:
		SYS_LOG_INF("cap change %u\n", para->cap);
		power_manager->current_cap = para->cap;
		power_manager->battary_changed = 1;	
	#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
		power_manager->battary_led_need_change = 1;
	#endif				
		break;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
	case BAT_CHG_EVENT_TEMP_CHANGE:
		SYS_LOG_INF("temp change %u\n", para->temperature);
		power_manager->current_temperature = para->temperature;
		break;
#endif
	default:
		break;
	}
}

bool power_manager_get_battery_is_full(void)
{
	if (!power_manager) {
		return false;
	}
	return !!power_manager->charge_full_flag;
}

static int get_system_bat_info(int property)
{
	union power_supply_propval val;

	int ret;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(power_manager->dev, property, &val);
	if (ret < 0) {
		SYS_LOG_ERR("get property err %d\n", ret);
		return -ENODEV;
	}

	return val.intval;
}

int power_manager_get_battery_capacity(void)
{
#ifdef CONFIG_TWS
	int report_cap = get_system_bat_info(POWER_SUPPLY_PROP_CAPACITY);
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
		if (report_cap > power_manager->slave_cap)
			report_cap = power_manager->slave_cap;
	}
	return report_cap;
#else
	return get_system_bat_info(POWER_SUPPLY_PROP_CAPACITY);
#endif
}

int power_manager_get_charge_status(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_STATUS);
}

int power_manager_get_battery_vol(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_VOLTAGE_NOW);
}


int power_manager_get_dc5v_status(void)
{

	return get_system_bat_info(POWER_SUPPLY_PROP_DC5V);
}
	

int power_manager_set_slave_battery_state(int capacity, int vol)
{
	power_manager->slave_vol = vol;
	power_manager->slave_cap = capacity;
	power_manager->battary_changed = 1;
	SYS_LOG_INF("vol %dmv cap %d\n", vol, capacity);
	return 0;
}



int power_manager_set_battery_vol(u16_t vol)
{

	union power_supply_propval val;

	val.intval = vol;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	power_supply_set_property(power_manager->dev, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	return true;
}

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
int power_manager_set_battery_cap(u16_t vol)
{

	union power_supply_propval val;

	val.intval = vol;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	power_supply_set_property(power_manager->dev, POWER_SUPPLY_PROP_CAPACITY, &val);
	if(power_first_get_cap_flag == 0){
		power_first_get_cap_flag = 1;
		SYS_LOG_INF("power_first_get_cap_flag = %d\n",power_first_get_cap_flag);
	}
	return true;
}

int power_manager_set_battery_remain_charge_time(u16_t vol)
{

	union power_supply_propval val;

	val.intval = vol;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	power_supply_set_property(power_manager->dev, POWER_SUPPLY_PROP_REMAIN_CHARGE_TIME, &val);
	return true;
}

int power_manager_set_battery_temperature(int vol)
{

	union power_supply_propval val;

	val.intval = vol;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	power_supply_set_property(power_manager->dev, POWER_SUPPLY_PROP_BAT_TEMP, &val);
	return true;
}

int power_manager_get_battery_temperature(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_BAT_TEMP);
}
#endif

int power_manager_sync_slave_battery_state(void)
{
	uint32_t send_value;

#ifdef CONFIG_BT_TWS_US281B
	send_value = power_manager->current_cap / 10;
#else
	send_value = (power_manager->current_cap << 24) | power_manager->current_vol;
#endif

#ifdef CONFIG_TWS
	bt_manager_tws_send_event(TWS_BATTERY_EVENT, send_value);
#endif

	return 0;
}

 void power_manager_battery_led_fn(void)
{
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
	static int charge_status = 0;
	int temp_status = power_manager_get_charge_status();
	if(charge_status != temp_status){
		power_manager->battary_changed = 1;	
		if(temp_status != POWER_SUPPLY_STATUS_UNKNOWN){
			if(pd_manager_get_poweron_filte_battery_led() == WLT_FILTER_CHARGINE_POWERON){

                 pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S); //display 10s
			}
			else if(pd_manager_get_poweron_filte_battery_led() == WLT_FILTER_DISCHARGE_POWERON){
                 printk("[%s/%d], WLT_FILTER_DISCHARGE_POWERON !!!\n\n",__func__,__LINE__);
                 pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S); //display 10s
			}
			else if(pd_manager_get_poweron_filte_battery_led() == WLT_FILTER_CHARGINE_WARNING){
				printk("[%s/%d], charging WARNING!!!\n\n",__func__,__LINE__);
			}
			else
			{
				{
					//add buglist
					printk("charge_status is %d / %d /%d !!!\n\n",charge_status,temp_status,pd_get_app_mode_state());
					if(temp_status == POWER_SUPPLY_STATUS_CHARGING && !dc_power_in_status_read()){
						printk("[%s/%d], charging , but usb no insert!!!\n\n",__func__,__LINE__);
					}
					else if(charge_status > POWER_SUPPLY_STATUS_DISCHARGE
					&& temp_status == POWER_SUPPLY_STATUS_DISCHARGE ){

					   if(pd_get_app_mode_state() == CHARGING_APP_MODE || pd_manager_get_poweron_filte_battery_led() == WLT_FILTER_STANDBY_POWEROFF)
					  {
					    // no use
						#ifdef CONFIG_BATTERYLED_POWEROFF_DELAY_10S
						printk("[%s/%d], poweroff , display 10s !!!\n\n",__func__,__LINE__);
						#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY   
						pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
						#endif	
						#else
						pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
						#endif
					   }
					   else
					   {
					      printk("[%s/%d], bt mode, display 10s !!!\n\n",__func__,__LINE__);
                          pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);
					   }
					}
					else if(temp_status == POWER_SUPPLY_STATUS_CHARGING){
						printk("[%s/%d], POWER_SUPPLY_STATUS_CHARGING !!!\n\n",__func__,__LINE__);
					   pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_CAHARGING);	
					}
					else
					{
						 //pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_CAHARGING);
					}
					

				}
				
			}
		}
		charge_status = temp_status;
	}	
	else if(power_manager->battary_led_need_change == 1){
		power_manager->battary_led_need_change = 0;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
		if(temp_status != POWER_SUPPLY_STATUS_DISCHARGE
		|| power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
		{
			pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);

		}
#endif		
	}
	
#endif
}
#ifdef CONFIG_POWER_SMART_CONTROL
/*
The unit shall support smart post Gain control for reducing power 
consumption. 

If remain battery capacity is lower than 15%, the post Gain is 
reduced by 0.5 db every 30s. And the total reduction is 2 db.

When above is triggered and the remain battery capacity is higher 
than 15%, the post Gain is increased by 0.5db every 30s till the 
reduction is increased.
*/

static void power_manager_smart_control(void)
{
	static u32_t count = 0;
	static int increase = 0;
	bool last = false;

	if (power_manager->battary_changed) {
		if (power_manager->current_cap < 15 && power_manager->smart_control >= 0) {
			increase = -1;
			count = 0xFFFFFFFF;
		}

		if (power_manager->current_cap >= 15 && power_manager->smart_control < 0) {
			increase = 1;
			count = 0xFFFFFFFF;
		}
	}

	if (increase != 0) {
		if(count >= (30*1000) / CONFIG_MONITOR_PERIOD) {
			if (increase == 1) {
				if(power_manager->smart_control < 0) {
					power_manager->smart_control +=5;
					if(power_manager->smart_control >= 0) {
						power_manager->smart_control = 0;
						last = true;
					}
				}
			}

			if (increase == -1) {
				if(power_manager->smart_control > -20) {
					power_manager->smart_control -=5;
					if(power_manager->smart_control <= -20) {
						power_manager->smart_control = -20;
						last = true;
					}
				}
			}

			SYS_LOG_INF("Gain %d, increase %d, last %d\n", power_manager->smart_control, increase, last);
			system_player_smart_control_volume_set(power_manager->smart_control);
			count = 0;

			if(last) {
				increase = 0;
			}
		}
		count++;
	}
}
#endif

static int _power_manager_work_handle(void)
{

	if (!power_manager)
		return -ESRCH;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY		
	//power_manager_battery_led_fn();
	/*if (power_manager->current_temperature >= DEFUALT_HIAHERTEMP_POWER_LEVEL) {
		if (((os_uptime_get_32() - power_manager->report_timestamp) >=  2000
				|| !power_manager->report_timestamp)) {		
			SYS_LOG_INF("%d temp is higher", power_manager->current_temperature);
			sys_event_notify(SYS_EVENT_CHARGING_WARNING);
			power_manager->report_timestamp = os_uptime_get_32();
		}
	}
	else */if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_POWER_LEVEL){
		SYS_LOG_INF("%d temp too high", power_manager->current_temperature);
		sys_event_notify(SYS_EVENT_POWER_OFF);
		return 0;
	}//DEFUALT_HIAHTEMP_POWER_LEVEL
	else if(power_manager->current_temperature >= DEFUALT_HIAHTEMP_JUST_VOLUME_LEVEL0){
		if (((os_uptime_get_32() - power_manager->report_justvol_timestamp) >=  DEFAULT_JUST_VOLUME_PERIODS
				|| !power_manager->report_justvol_timestamp)) {		
			SYS_LOG_INF("%d just volume", power_manager->current_temperature);
			sys_event_notify(SYS_EVENT_SMART_CONTROL_VOLUME);
			power_manager->report_justvol_timestamp = os_uptime_get_32();
		}		
		// return 0;																		// Totti debug on 2024/0712
	}
	else if(power_manager->current_temperature < DEFUALT_LOWTEMP_POWER_LEVEL){
		SYS_LOG_INF("%d temp too low", power_manager->current_temperature);
		sys_event_notify(SYS_EVENT_POWER_OFF);
		return 0;
	}		
#endif			

#ifdef CONFIG_POWER_SMART_CONTROL
	power_manager_smart_control();
#endif

	if (power_manager->battary_changed) {
		power_manager->battary_changed = 0;
		power_manager_status_notify();
#ifdef CONFIG_BT_HFP_HF
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
			power_manager_sync_slave_battery_state();
		} else if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
			int report_cap = power_manager->current_cap;

			if (report_cap > power_manager->slave_cap)
				report_cap = power_manager->slave_cap;

			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, report_cap);
		} else {
			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, power_manager->current_cap);
		}
	#else
		bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, power_manager->current_cap);
	#endif
#endif
	}


	if (power_manager->charge_full_flag) {
		power_manager->charge_full_flag = 0;
		sys_event_notify(SYS_EVENT_CHARGE_FULL);
	}


#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY

	int dut_is_charging;
	if(ReadODM())
	{
		dut_is_charging = power_manager_get_dc5v_status();
		
	}else{
		dut_is_charging = pd_get_sink_charging_state();					//3 nods
	}

	if(dut_is_charging)
	{
		if(power_first_get_cap_flag == 0){
			printk("\n %s,%d,last_cap:%d,current_cap:%d\n",__func__,__LINE__,power_manager->last_cap,power_manager->current_cap);
			return 0;
		}
		power_manager->current_cap = power_manager_get_battery_capacity();
		if(power_manager->last_cap != power_manager->current_cap)
		{
			SYS_LOG_INF("[%d] current_cap%d, last_cap:%d \n", __LINE__, power_manager->current_cap, power_manager->last_cap);
			if((power_manager->current_cap < power_manager->last_cap)
					&& (power_manager->current_cap <= DEFAULT_NOPOWER_CAP_LEVEL) 
					&& (power_manager->last_cap<15))
			{
				if(!power_manager->report_last_cap_timestamp)
				{
					power_manager->report_last_cap_timestamp = os_uptime_get_32();
				}
				//5分钟后last_cap一直小于current_cap，关机
				if((os_uptime_get_32() - power_manager->report_last_cap_timestamp) >= DEFAULT_POWER_OFF_PERIODS){
					printk("\n %s,%d,cap level is down\n",__func__,__LINE__);
					power_manager->last_cap = power_manager->current_cap;
					power_manager->report_last_cap_timestamp = os_uptime_get_32();
					goto _POWER_OFF_;
				}
				return 0;
			}
			power_manager->last_cap = power_manager->current_cap;
			
		}
		power_manager->report_last_cap_timestamp = 0;
		return 0;
	}


	if (power_manager->current_cap <= DEFAULT_NOPOWER_CAP_LEVEL) {
		if(!power_manager->report_timestamp)
		{
			power_manager->report_timestamp = os_uptime_get_32();
		}
		if((os_uptime_get_32() - power_manager->report_timestamp) >= (DEFAULT_POWER_OFF_PERIODS/2))
		{
_POWER_OFF_:			
			SYS_LOG_INF("%d %d too low", power_manager->current_cap, power_manager->current_vol);
			power_manager->report_timestamp = 0;

			// if((pd_get_app_mode_state() == CHARGING_APP_MODE && power_manager_get_charge_status() == POWER_SUPPLY_STATUS_DISCHARGE)
			// 	|| (pd_get_app_mode_state() != CHARGING_APP_MODE && bt_manager_a2dp_get_status() == BT_STATUS_PLAYING)){

			if((pd_get_app_mode_state() == CHARGING_APP_MODE) && (power_manager_get_charge_status() == POWER_SUPPLY_STATUS_DISCHARGE))
			{

				printk("\n %s,charge mode do not charge  \n",__func__);
				pd_manager_deinit(0);
				sys_pm_poweroff(); 
			} 
			else
			{		
				sys_event_notify(SYS_EVENT_BATTERY_TOO_LOW);
			}
		}
		return 0;
	}
	power_manager->report_timestamp = 0;

#else
_POWER_OFF_:
	if (power_manager->current_vol <= power_manager->nopower_level) {
		SYS_LOG_INF("%d %d too low\n", power_manager->current_vol, power_manager->slave_vol);
		sys_event_notify(SYS_EVENT_BATTERY_TOO_LOW);
		return 0;
	}

	if ((power_manager->current_vol <= power_manager->lowpower_level)
		&& ((os_uptime_get_32() - power_manager->report_timestamp) >=  DEFAULT_REPORT_PERIODS
			|| !power_manager->report_timestamp)) {
		SYS_LOG_INF("%d %d low\n", power_manager->current_vol, power_manager->slave_vol);
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
			sys_event_notify(SYS_EVENT_BATTERY_LOW);
		}
	#else
		sys_event_notify(SYS_EVENT_BATTERY_LOW);
	#endif
		power_manager->report_timestamp = os_uptime_get_32();
	}
#endif
	return 0;
}

static struct power_manager_info global_power_manager;

int power_manager_init(void)
{
//#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE	
	power_manager = &global_power_manager;

	memset(power_manager, 0, sizeof(struct power_manager_info));

	power_manager->dev = (struct device *)device_get_binding("battery");
	if (!power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	if ((power_manager_get_battery_vol() <= DEFAULT_BOOTPOWER_LEVEL)
			&& (!power_manager_get_dc5v_status())) {
		SYS_LOG_INF("no power ,shundown: %d\n", power_manager_get_battery_vol());
		// sys_pm_poweroff();
		// return 0;
	}


	power_supply_register_notify(power_manager->dev, power_supply_report);
#ifdef CONFIG_PROPERTY
	power_manager->lowpower_level = property_get_int(CFG_LOW_POWER_WARNING_LEVEL, DEFAULT_LOWPOWER_LEVEL);
	power_manager->nopower_level = property_get_int(CFG_SHUTDOWN_POWER_LEVEL, DEFAULT_NOPOWER_LEVEL);
#else
	power_manager->lowpower_level = DEFAULT_LOWPOWER_LEVEL;
	power_manager->nopower_level = DEFAULT_NOPOWER_LEVEL;
#endif
	power_manager->current_vol = power_manager_get_battery_vol();
	power_manager->current_cap = power_manager_get_battery_capacity();
	power_manager->slave_vol = 4200000;
	power_manager->slave_cap = 100;
	power_manager->report_timestamp = 0;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY	
	power_manager->current_temperature = power_manager_get_battery_temperature();
	power_manager->report_justvol_timestamp = 0;
	power_first_get_cap_flag = 0x00;
#endif	
	power_manager->last_cap = 0;
	power_manager->report_last_cap_timestamp = 0;
//#endif
	sys_monitor_add_work(_power_manager_work_handle);

	return 0;
}

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
extern bool pd_manager_check_mobile(void);
extern int pd_manager_deinit(int value);	
extern void battery_remaincap_low_poweroff(void);

int power_manager_early_init(void)
{
	if (!power_manager)
		return -ESRCH;

	SYS_LOG_INF("battery capacity: %d\n", power_manager_get_battery_capacity());

	if ((power_manager_get_battery_capacity() <= DEFAULT_NOPOWER_CAP_LEVEL))
	{
		if(power_manager_get_dc5v_status())
		{
			k_sleep(1000);
			if(!pd_manager_check_mobile())
				return 0;
		}

		SYS_LOG_INF("no power ,shundown: %d\n", power_manager_get_battery_capacity());
		//pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,LOW_POWER_OFF_LED_STATUS);
       battery_remaincap_low_poweroff();
		k_sleep(50);
        
		pd_manager_deinit(0);		
		sys_pm_poweroff();

	}
		
	return 0;
}

#endif
