#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>


#include <i2c.h>

#include <gpio.h>
#include <pinmux.h>


#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <volume_manager.h>
#include <msg_manager.h>
#include <thread_timer.h>
#include <media_player.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <zephyr.h>

#include <bt_manager.h>
#include <global_mem.h>
#include <stream.h>
#include <usb_audio_hal.h>

#include <soc_dvfs.h>
#include <thread_timer.h>


#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include <ui_manager.h>
#include <input_manager.h>
#include <mcu_manager_supply.h>
#include "power_supply.h"
#include "power_manager.h"

#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <pd_manager_supply.h>
#include "app/charge_app/charge_app.h"
#include "run_mode/run_mode.h"
/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @addtogroup t_pinmux_basic
 * @{
 * @defgroup t_pinmux_gpio test_pinmux_gpio
 * @brief TestPurpose: verify PINMUX works on GPIO port
 * @details
 * - Test Steps (Quark_se_c1000_devboard)
 *   -# Connect pin_53(GPIO_19) and pin_50(GPIO_16).
 *   -# Configure GPIO_19 as output pin and set init  LOW.
 *   -# Configure GPIO_16 as input pin and set LEVEL HIGH interrupt.
 *   -# Set pin_50 to PINMUX_FUNC_A and test GPIO functionality.
 *   -# Set pin_50 to PINMUX_FUNC_B and test GPIO functionality.
 * - Expected Results
 *   -# When set pin_50 to PINMUX_FUNC_A, pin_50 works as GPIO and gpio
 *	callback will be triggered.
 *   -# When set pin_50 to PINMUX_FUNC_B, pin_50 works as I2S and gpio
 *	callback will not be triggered.
 * @}
 */

typedef struct {
    /** bt_link_status */
	u8_t bt_link_status;

    /** BT send IIC to MCU led value */
	u8_t led_value;

} bt_mcu_pair_led_map_t;

static const  bt_mcu_pair_led_map_t bt_mcu_pair_ledmap[] = {

    {SYS_EVENT_BT_UNLINKED,         BT_LED_STATUS_OFF },        
    {SYS_EVENT_BT_WAIT_PAIR,        BT_LED_STATUS_SLOW_FLASH},
    {SYS_EVENT_ENTER_PAIR_MODE,     BT_LED_STATUS_QUICK_FLASH},
    {SYS_EVENT_BT_CONNECTED,        BT_LED_STATUS_ON},
    {SYS_EVENT_TWS_CONNECTED,       BT_LED_STATUS_ON},
    {SYS_EVENT_TWS_START_PAIR,      BT_LED_STATUS_FLASH},
};

#define BATTERY_CHARGING_REMAIN_CAP_LEVEL0        15
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL1        23
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL2        45
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL3        60
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL4        75
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL5        100
#define DISCHARGE_LED_DISPLAY_TIME_10s            86
#define DISCHARGE_LED_DISPLAY_TIME_2s            10
#define DISCHARGE_LED_DISPLAY_TIME_0_5s          5

//static battery_manage_info_t global_bat_namager;

static struct thread_timer mcu_reset_timer;

int mcu_ui_send_led_code(uint8_t type, int code)
{
    struct device *mcu_dev = NULL;
    const struct mcu_manager_supply_driver_api *mcu_api = NULL;	
	union mcu_manager_supply_propval val;

    
    if(!ReadODM())
    {
        mcu_dev = wlt_device_get_mcu_mspm0l();
    }
    else
    {
        mcu_dev = wlt_device_get_mcu_ls8a10049t();
    }

    if(mcu_dev == NULL){
		printk("[%s/%d],error!!!\n\n",__func__,__LINE__);
        return -1;
	}

    mcu_api = mcu_dev->driver_api;
    val.intval = code;	
	mcu_api->set_property(mcu_dev, type, &val);
	return 0;
}

int mcu_ui_get_led_code(uint8_t type)
{
    struct device *mcu_dev = NULL;
    const struct mcu_manager_supply_driver_api *mcu_api = NULL;	
	union mcu_manager_supply_propval val;
    int value;
    

    if(!ReadODM())
    {
        mcu_dev = wlt_device_get_mcu_mspm0l();
    }
    else
    {
        mcu_dev = wlt_device_get_mcu_ls8a10049t();
    }

    if(mcu_dev == NULL){
		printk("[%s/%d],error!!!\n\n",__func__,__LINE__);
        return -1;
	}

    mcu_api = mcu_dev->driver_api;
	mcu_api->get_property(mcu_dev, type, &val);
    value = val.intval;
	return value;
}

void battery_remaincap_low_poweroff(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);

    mcu_ui_send_led_code(MCU_SUPPLY_PROP_RED_LED_FLASH,BT_LED_STATUS_OFF);
    /*
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);
    */   
}

void battery_discharge_remaincap_low_5(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_15(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_FLASH);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);    
}
static void battery_discharge_remaincap_low_15_noflash(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);    
}
static void battery_discharge_remaincap_low_30(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);        
}

static void battery_discharge_remaincap_low_45(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);  	
}

static void battery_discharge_remaincap_low_60(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF); 
}

static void battery_discharge_remaincap_low_75(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF); 	
}

static void battery_discharge_remaincap_low_100(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_ON); 		
}

static void battery_charging_remaincap_low_15(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);	
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_FLASH);
   // mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);       
}

static void battery_charging_remaincap_low_23(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_FLASH);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);       		
}

static void battery_charging_remaincap_low_45(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_FLASH);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);     	
}

static void battery_charging_remaincap_low_60(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_FLASH);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);      	
}

static void battery_charging_remaincap_low_75(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_FLASH);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);      	
}

static void battery_charging_remaincap_low_100(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_FLASH);      	
}

void battery_charging_remaincap_is_full(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_OFF);      
}

void battery_charging_LED_on_all(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,BT_LED_STATUS_OFF);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,BT_LED_STATUS_ON);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,BT_LED_STATUS_ON);      
}





void mcu_ui_set_dsp_dcdc(uint8_t onoff)
{
    SYS_LOG_INF("[%d],onoff =%d\n", __LINE__, onoff);


    if(pd_srv_sync_send_msg(PA_EVENT_AW_20V5_CTL, (int)onoff) == -1)
     {
        SYS_LOG_INF("[%d],msg fail reinstall MCU_SUPPLY_PROP_DSP_DCDC \n", __LINE__);
        mcu_ui_send_led_code(MCU_SUPPLY_PROP_DSP_DCDC,onoff); 
     }

 //   SYS_LOG_INF("[%d],onoff =%d\n", __LINE__, onoff);

}
#if 0
static void battery_led_timer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	 printk("ZTH DEBUD: %s:%d\n", __func__, __LINE__);
   //  thread_timer_stop(&battery_led_timer);
     battery_discharge_remaincap_low_5();//死机
}
#endif     
extern uint8_t pd_get_app_mode_state(void); 
typedef struct {
	int batled_display_time;
	int pwrled_display_time;
	int btled_display_time;
	int acled_display_time;
}disp_time_t;
disp_time_t disp_time;

void set_batt_led_display_timer(int times)
{
   disp_time.batled_display_time = times;
   SYS_LOG_INF("batled_display_time = %d", times);
}
void set_pwr_led_display_timer(int times)
{
   disp_time.pwrled_display_time = times;
   SYS_LOG_INF("pwrled_display_time = %d", times);
}
void set_bt_led_display_timer(int times)
{
   disp_time.btled_display_time = times;
   SYS_LOG_INF("btled_display_time = %d", times);
}
void set_ac_led_display_timer(int times)
{
   disp_time.acled_display_time = times;
   SYS_LOG_INF("acled_display_time = %d", times);
}

int get_batt_led_display_timer(void)
{
   return disp_time.batled_display_time;
}
int get_pwr_led_display_timer(void)
{
   return disp_time.pwrled_display_time;
}
int get_bt_led_display_timer(void)
{
   return disp_time.btled_display_time;
}
int get_ac_led_display_timer(void)
{
   return disp_time.acled_display_time;
}

static void battery_status_discharge_handle(u16_t cap)
{
    if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){
		/* during pwroff, red led don't need flash */
		if(get_pwr_led_display_timer()){
			battery_discharge_remaincap_low_15_noflash();
		}
		else{
			battery_discharge_remaincap_low_15();
        	set_batt_led_display_timer(-1); 
		}
    }
    else{
        if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
            battery_discharge_remaincap_low_30();
        }
        else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
            battery_discharge_remaincap_low_45();
        }
        else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
            battery_discharge_remaincap_low_60();
        }
        else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
            battery_discharge_remaincap_low_75();
        }
        else{
            battery_discharge_remaincap_low_100();
        }
    }
}

/* power off and all LED sync off */
void batt_led_display_timeout(void)
{
	if(disp_time.batled_display_time > 0){     
		disp_time.batled_display_time --;
		if(disp_time.batled_display_time == 0){	  
			SYS_LOG_INF("Batt_display_time = %d", disp_time.batled_display_time);
			battery_charging_remaincap_is_full();
		}	 	
	}	
	if(disp_time.pwrled_display_time > 0){       
		disp_time.pwrled_display_time --;
		if(disp_time.pwrled_display_time == 0){	  
			SYS_LOG_INF("Pwr_display_time = %d", disp_time.pwrled_display_time);
			led_manager_set_display(128,LED_OFF,OS_FOREVER,NULL);
		}	 	
	}   
	if(disp_time.btled_display_time > 0){       
		disp_time.btled_display_time --;
		if(disp_time.btled_display_time == 0){	  
			SYS_LOG_INF("Bt_display_time = %d bt_led_enable = %d", disp_time.btled_display_time,mcu_ui_get_enable_bt_led_state());
			if(mcu_ui_get_enable_bt_led_state()){
				mcu_ui_set_enable_bt_led_state(false);
				mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_BT,0);
				mcu_ui_set_enable_bt_led_state(true);
			}
			else{
				mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_BT,0);
			}
		}	 	
	} 	
	if(disp_time.acled_display_time > 0){       
		disp_time.acled_display_time --;
		if(disp_time.acled_display_time == 0){	  
			SYS_LOG_INF("Ac_display_time = %d ac_led_enable = %d", disp_time.acled_display_time,mcu_ui_get_enable_auracast_led_state());
			if(mcu_ui_get_enable_auracast_led_state()){
				mcu_ui_set_enable_auracast_led_state(false);
				mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_PTY_BOOST,0);
				mcu_ui_set_enable_auracast_led_state(true);
			}
			else{
				mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_PTY_BOOST,0);
			}			
		}	 	
	} 		
}


extern int check_is_wait_adfu(void);

void battery_status_remaincap_display_handle(uint8_t status, u16_t cap, int led_status)
{
    printf("zth debug remaincap_display_handle,status:%d , cap: 0x%x \n", status,cap);
   static u8_t first_10s_batt_display_time_flag = 0;
    if(check_is_wait_adfu())
        return;

    switch(status)
    {
   
  #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY             
         case POWER_SUPPLY_STATUS_FORCE_DISPLAY:
		 	set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_10s);//set 10s
            battery_status_discharge_handle(cap);
         break;
  #endif                    
        case POWER_SUPPLY_STATUS_DISCHARGE:
        {
            //if(key_flag){

                 //if(pd_get_app_mode_state() == CHARGING_APP_MODE){
                 //    break;
                 //}

                /*if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                    battery_discharge_remaincap_low_5();   
                }
                else*/ 
              
              if(led_status == BATT_LED_ON_2S)
			  {
			    if(get_batt_led_display_timer() > 0)
				 {
				   set_batt_led_display_timer(2);
			     }
				else
			      {
			       set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_2s);//set 2s
				  }
              }
			  else if(led_status == BATT_PWR_LED_ON_0_5S)
			  {
			  	/* poweroff need lighton 300ms with batled,then lightoff together */
				set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_pwr_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_bt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_ac_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
			  }
			  else //if(led_status == BATT_LED_ON_10S)
			  {
			    if(first_10s_batt_display_time_flag)
			     {
			       set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_10s);//set 10s
			     }
			     else
			     {
			       first_10s_batt_display_time_flag = 1;
                   set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_10s -13);//set 10s
				 }
			  }
			  	
                battery_status_discharge_handle(cap);
           
        }
        break;

        case POWER_SUPPLY_STATUS_CHARGING:
        {
            if(cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL0){
                battery_charging_remaincap_low_15();
            }
            else if(cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL1){
                battery_charging_remaincap_low_23();
            }
            else if(cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL2){
                battery_charging_remaincap_low_45();
            }
            else if(cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL3){
                battery_charging_remaincap_low_60();
            }
            else if(cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL4){
                battery_charging_remaincap_low_75();
            }
            else{
                battery_charging_remaincap_low_100();
            }

			if(led_status == BATT_PWR_LED_ON_0_5S){
				set_batt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_pwr_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_bt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_ac_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);			
			}
			set_batt_led_display_timer(-1);
        }
        break;
        
        case POWER_SUPPLY_STATUS_FULL:

            if(run_mode_is_demo())
            {
                set_batt_led_display_timer(-1);     
                battery_charging_LED_on_all();
            }else{
				set_batt_led_display_timer(-1);  
                battery_charging_remaincap_is_full();
            }     

			if(led_status == BATT_PWR_LED_ON_0_5S){
				set_pwr_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_bt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_ac_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);				
			}

        break;  

 #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY       
        case POWER_SUPPLY_STATUS_EXT_CHARGING:
        {

            /*if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                 battery_discharge_remaincap_low_5();   
            }
            else*/ if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){

                battery_discharge_remaincap_low_15();
            }
            else{
                if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
                    battery_discharge_remaincap_low_30();
                }
                else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
                    battery_discharge_remaincap_low_45();
                }
                else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
                    battery_discharge_remaincap_low_60();
                }
                else if(cap <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
                    battery_discharge_remaincap_low_75();
                }
                else{
                    battery_discharge_remaincap_low_100();
                }
            } 
			if(led_status == BATT_PWR_LED_ON_0_5S){
				set_pwr_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_bt_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);
				set_ac_led_display_timer(DISCHARGE_LED_DISPLAY_TIME_0_5s);				
			}			
			set_batt_led_display_timer(-1);
        }
        break;   
 #endif  

        default:
            break;
    }

}

void bt_ui_charging_warning_handle(uint8_t status)
{ 
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_0,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_1,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_2,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_3,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_4,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_5,status);     
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_BT,status);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_PTY_BOOST,status);   
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_POWER,status);    
	printk("[%s/%d],%d on\n\n",__func__,__LINE__,status);	
}

void bt_manager_sys_event_led_display(int link_status)
{
    int len = 0;

    len = sizeof(bt_mcu_pair_ledmap) / sizeof(bt_mcu_pair_led_map_t); 

    printk("Totti debug:%s:%d; len= %d; %d \n", __func__,__LINE__, len, link_status);

    for(int i=0; i< len; i ++)
    {
        if(link_status == bt_mcu_pair_ledmap[i].bt_link_status)
        {
			//led_dev[MCU_SUPPLY_PROP_LED_BT].status = bt_mcu_pair_ledmap[i].led_value;
            mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_BT,bt_mcu_pair_ledmap[i].led_value);
            break;
        }
    }
}

void bt_manager_auracast_led_display(bool status)
{
	uint8_t val;
	val = BT_LED_STATUS_OFF;
	if(status)
		val = BT_LED_STATUS_ON;
	//led_dev[MCU_SUPPLY_PROP_LED_PTY_BOOST].status = val;
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_LED_PTY_BOOST,val);

}

void mcu_ui_set_enable_bt_led_state(bool enable)
{
	uint8_t val;
	val = enable;
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_ENABLE_LED_BT,val);

}

void mcu_ui_set_enable_auracast_led_state(bool enable)
{
	uint8_t val;
	val = enable;
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_ENABLE_LED_PTY_BOOST,val);

}

void mcu_ui_set_enable_bat_led_state(bool enable)
{
	uint8_t val;
	val = enable;
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_ENABLE_LED_BAT,val);

}

bool mcu_ui_get_enable_bt_led_state(void)
{
	bool val;
    val = (bool)mcu_ui_get_led_code(MCU_SUPPLY_PROP_ENABLE_LED_BT);

    return val;
}

bool mcu_ui_get_enable_auracast_led_state(void)
{
	bool val;
    val = (bool)mcu_ui_get_led_code(MCU_SUPPLY_PROP_ENABLE_LED_PTY_BOOST);

    return val;
}

bool mcu_ui_get_enable_bat_led_state(void)
{
	bool val;
    val = (bool)mcu_ui_get_led_code(MCU_SUPPLY_PROP_ENABLE_LED_BAT);

    return val;
}

/* for factory test! */
int mcu_ui_get_logic_version(void)
{
	int val = -1;
	val = mcu_ui_get_led_code(MCU_SUPPLY_PROP_READ_LOGIC_VER);
	
    return val;
}

void mcu_ui_set_led_enable_state(int state)
{
    printk("\n%s,state:0x%x \n",__func__,state);
    if(BT_LED_STATE_MASK(state) != 0xFF){
        mcu_ui_set_enable_bt_led_state(BT_LED_STATE_MASK(state));
    }
    
    if(AC_LED_STATE_MASK(state) != 0xFF){
		mcu_ui_set_enable_auracast_led_state(AC_LED_STATE_MASK(state));
    }    

    if(BAT_LED_STATE_MASK(state) != 0xFF){
	    mcu_ui_set_enable_bat_led_state(BAT_LED_STATE_MASK(state));
    }

}
#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
void mcu_ui_set_real_shutdown_dut(bool enable)
{
	uint8_t val;
	val = enable;
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_REAL_SHUTDOWN_DUT,val);

}
#endif

void mcu_ui_set_led_just_level(bool up)
{
	if(up)
    {
        mcu_ui_send_led_code(MCU_SUPPLY_PROP_JUST_UP_LED_LEVEL,1);
    }
    else
    {
        mcu_ui_send_led_code(MCU_SUPPLY_PROP_JUST_DOWN_LED_LEVEL,1);
    }

}
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
//extern void user_mspm0l_ota_sucess_startapp(void);
extern int user_mspm0l_ota_process(void);


void bt_manager_send_fw_update_code(uint8_t value)
{
	uint8_t val;
	val = value;
    printk("[%s,%d] val %d !!!\n", __FUNCTION__, __LINE__,val);
    mcu_ui_send_led_code(MCU_SUPPLY_PROP_UPDATE_FW,val);

}

uint8_t bt_manager_get_fw_update_code(void)
{
	uint8_t val;
    val = mcu_ui_get_led_code(MCU_SUPPLY_PROP_UPDATE_FW);
    //printk("[%s,%d] val %d !!!\n", __FUNCTION__, __LINE__,val);

    return val;
}

void bt_manager_user_ctl_mcu_update(void)
{
    uint8_t fw_update = bt_manager_get_fw_update_code();
    if(fw_update == MCU_FW_UPDATE_STOP){
        bt_manager_send_fw_update_code(MCU_FW_UPDATE_START);
    }
}

static void mcu_reset_timer_fn(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	 printk("ZTH DEBUD: %s:%d\n", __func__, __LINE__);
    // if(dc_power_in_status_read() == 0){
     thread_timer_stop(&mcu_reset_timer);
     //bt_manager_send_fw_update_code(MCU_FW_UPDATE_STOP);
     //user_mspm0l_ota_sucess_startapp();
     for(uint8_t i = 0; i < 10; i++){
     mcu_ui_send_led_code(MCU_SUPPLY_PROP_RESET_MCU,0);
     }
    // }
}

int mcu_ui_ota_deal(void)
{
    int ret = 0;
    struct device *dev = NULL;
    uint8_t fw_update = bt_manager_get_fw_update_code();


    if(fw_update == MCU_FW_UPDATE_START){

        if(power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0
        /*|| dc_power_in_status_read() == 1*/){
            sys_event_notify(SYS_EVENT_MCU_FW_FAIL);
            bt_manager_send_fw_update_code(MCU_FW_UPDATE_STOP);
            return 0;
        }
        /**disable key adc */
        dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME);
        if (dev)
            input_dev_disable(dev);

        bt_manager_send_fw_update_code(MCU_FW_UPDATEING);
		extern int WLT_OTA_PD(bool flag);

        if(ReadODM())
		{
		 ret = WLT_OTA_PD(1);
      	}
        else
        {
          ret = user_mspm0l_ota_process();
        }

	    if (dev)
	        input_dev_enable(dev);     
        printk("[%s,%d] ret %d !!!\n", __FUNCTION__, __LINE__,ret);
        if(ret == 1){
            sys_event_notify(SYS_EVENT_MCU_FW_SUCESS);
            pd_set_source_refrest();
           // k_sleep(3000);
           // user_mspm0l_ota_sucess_startapp();
            //mcu_mspm0l->mcu_update_fw = 0xff;

             if(!ReadODM())
		    {
              bt_manager_send_fw_update_code(MCU_FW_UPDATE_SUCESS);   
              thread_timer_start(&mcu_reset_timer,1000,1000);
		 	}else{
                bt_manager_send_fw_update_code(MCU_FW_UPDATE_STOP); 
		 	}
        }
        else{

			 if(!ReadODM())
		     {
	            /**enable key adc */
	            dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME);
	            //mcu_mspm0l->mcu_update_fw = 0;
			 }
             
             if(ret == 2){
                sys_event_notify(SYS_EVENT_MCU_FW_UPDATED);//
             }
             else{
                sys_event_notify(SYS_EVENT_MCU_FW_FAIL);
             }
             bt_manager_send_fw_update_code(MCU_FW_UPDATE_STOP);
        }        

    }
    else if(fw_update == MCU_FW_UPDATE_SUCESS)
    {
        ret = 1;
    }
    return ret;
}
/////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
bool wlt_mcu_ui_is_dc_in(void)
{
    bool ret = 0;

    if(!ReadODM()){

        ret = mcu_mspm0l_is_dc_in();
    }
    else{
        ret = mcu_ls8a10049t_is_dc_in();
    }
    printk("%s,%d, usb --> %d \n",__func__,__LINE__,ret);
    return ret;
}

#ifdef CONFIG_ACTIONS_IMG_LOAD
int mcu_mspm0l_poweron_hold(void);
void ls8a10049t_poweron_hold(void);
void mcu_ui_power_hold_fn(void)
{
    if(!ReadODM()){

        mcu_mspm0l_poweron_hold();
    }
    else{
        ls8a10049t_poweron_hold();
    }
    printk("\n%s,power has hold !! \n",__func__);
}
#endif

bool  wlt_mcu_ui_process_init(void)
{

  if(ReadODM())
   {
    extern bool  aw9523b_ui_process_init(void);
    aw9523b_ui_process_init();
  }  
     //threa95d_timer_init(&mcu_ui_timer, mcu_ui_handle_timer_fn, NULL);
	 //thread_timer_start(&mcu_ui_timer, MCU_UI_PROCESS_TIME_PERIOD,MCU_UI_PROCESS_TIME_PERIOD);
	// thread_timer_init(&battery_led_timer, battery_led_timer_fn, NULL);
     thread_timer_init(&mcu_reset_timer, mcu_reset_timer_fn, NULL);

     return true;
}  

///////////////////////////////////////MUC EVENT DEAL///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
extern void battery_discharge_remaincap_low_5(void);
extern void battery_remaincap_low_poweroff(void);
#endif
extern int bt_mcu_send_cmd_code(u8_t type, int code);
extern bool wlt_pd_manager_is_init(void);
extern bool bt_mcu_get_pw_cmd_exit_standy(void);
extern void bt_mcu_set_first_power_on_flag(bool flag);
extern bool bt_mcu_get_first_power_on_flag(void);
extern void bt_mcu_set_bt_warning_poweroff_flag(u8_t flag);
extern uint8_t bt_mcu_get_bt_warning_poweroff_flag(void);
extern void bt_mcu_set_bt_wake_up_flag(bool flag);
extern bool bt_mcu_get_bt_wake_up_flag(void);
extern void pd_manager_set_poweron_filte_battery_led(uint8_t flag);
extern int bt_mcu_send_pw_cmd_poweron(void);
extern int pd_manager_get_sink_status_flag(void);

int check_battery_low_cap_level5()
{
    if(power_manager_get_battery_capacity() > BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0)
        return 0;

    

    if((power_manager_get_battery_capacity() == BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0) && pd_get_sink_charging_state())
    {
        return 0;
    }
    return 0;
}
extern void sys_event_report_input_ats(uint32_t key_event);
extern bool ats_get_enter_key_check_record(void);
extern int pd_manager_get_power_key_debounce(void);
extern bool main_system_tts_get_play_warning_tone_flag(void);
extern bool user_get_key_test(void);

#define MCU_CHARGE_WARNNING_FILTER_TIMES           2
#define MCU_FIRST_TRIGGER_FILTER_TIME           1800
enum charge_warnning_status_t{
    BT_CHARGE_WARNNING_NONE = 0,
    BT_CHARGE_WARNNING_READY,
    BT_CHARGE_WARNNING_TRIGGERED,
};

typedef struct{
    bool bt_is_charge_warnning_flag;
    uint8_t bt_poweron_from_charge_warnning;
    uint8_t bt_water_warnning_status;
    uint8_t bt_ntc_warnning_status;
	uint8_t dc_in_triggered_cnt;    
	uint32_t water_filter_time;
	uint32_t dc_in_filter_time;
	uint32_t ntc_filter_time;
	uint32_t water_remove_filter_time;
	uint32_t dc_in_remove_filter_time;  
    uint32_t ntc_remove_filter_time;  
}charge_warnning_info_t;

static charge_warnning_info_t charge_warnning = {0};
uint8_t mcu_ui_get_poweron_from_charge_warnning_status(void)
{
    SYS_LOG_INF("get status:%d\n",charge_warnning.bt_poweron_from_charge_warnning);
    return charge_warnning.bt_poweron_from_charge_warnning;
}

void mcu_ui_set_poweron_from_charge_warnning_status(uint8_t status)
{
    SYS_LOG_INF("set status:%d\n",status);
    charge_warnning.bt_poweron_from_charge_warnning = status;
}
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
static int mcu_poweron_is_charge_warnning(void)
{
    struct app_msg msg = {0};
    int ret;

    ret = 0;
    if(pd_get_app_mode_state())
    {
        if(!charge_app_is_enter())
        {
            mcu_ui_set_poweron_from_charge_warnning_status(1);
            msg.type = MSG_POWER_KEY;
            send_async_msg(APP_ID_MAIN, &msg);   
            ret = 1;         
        }
        else
        {
            msg.type = MSG_CHARGE_WARNNING_OPEN_PA;
            send_async_msg(APP_ID_MAIN, &msg);
        }
    }
    return ret;
}


#else
//static bool bt_is_charge_warnning_flag = false;
#endif
static void mcu_power_key_deal_fn(mcu_manager_charge_event_para_t *para)
{
    uint8 result=0;
    /* for factory test */
    if(ats_get_enter_key_check_record()){
        // mcu_ui_send_led_code(MCU_SUPPLY_PROP_FACTORY_TEST_KEY,1);
        bt_mcu_send_pw_cmd_poweron();
        sys_event_report_input_ats(51);
    }
    else if(pd_manager_get_power_key_debounce())  
    {
        bt_mcu_send_pw_cmd_poweron();
    }  
    else if((bt_mcu_get_first_power_on_flag() == 0)||(bt_mcu_get_bt_wake_up_flag() == 1))
    {
        if(pd_get_app_mode_state() && (bt_mcu_get_first_power_on_flag() == 0))
        {
            if(power_manager_get_battery_capacity() > (BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0-3))
            {
                struct app_msg msg = {0};
                msg.type = MSG_POWER_KEY;
                send_async_msg(APP_ID_MAIN, &msg);
                printk("POWER KEY first MSG_POWER_KEY\n");
                pd_manager_set_poweron_filte_battery_led(WLT_FILTER_DISCHARGE_POWERON);
/* 
                k_sleep(10);
                msg.type = MSG_CHARGER_MODE;
                msg.cmd = 0;

                send_async_msg(CONFIG_SYS_APP_NAME, &msg); */
            }else{
                printk("POWER KEY first, but low battery\n");
                // pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,LOW_POWER_OFF_LED_STATUS);
                battery_remaincap_low_poweroff();
                k_sleep(250);
                pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
            }
                
        }
        else if (bt_mcu_get_bt_wake_up_flag() == 1)
        {
            printk("POWER KEY wake up \n");
            pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);
        }
        bt_mcu_send_pw_cmd_poweron();
        bt_mcu_set_first_power_on_flag(1);
        printk("POWER KEY first power on\n");

    }else if(pd_get_app_mode_state())
    {
        if(charge_app_exit_cmd() == 1)
        {
            if(power_manager_get_battery_capacity() > (BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0-3)) 
            {
                struct app_msg msg = {0};
                msg.type = MSG_POWER_KEY;
                send_async_msg(APP_ID_MAIN, &msg);
/* 
                k_sleep(10);
                msg.type = MSG_CHARGER_MODE;
                msg.cmd = 0;

                send_async_msg(CONFIG_SYS_APP_NAME, &msg); */
            }
            else
            {
                if(!(pd_get_sink_charging_state() || dc_power_in_status_read()))
                {
                    printk("POWER KEY later, but low battery\n");
                    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,LOW_POWER_OFF_LED_STATUS);
                    k_sleep(500);
                    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
                }                        
            }
        }
        bt_mcu_send_pw_cmd_poweron();
        pd_manager_set_poweron_filte_battery_led(WLT_FILTER_CHARGINE_POWERON);
        printk("POWER KEY exit charge app\n");
    }else{
        if(charge_warnning.bt_is_charge_warnning_flag)
        {
            printk("BT IS CHARGING WARNING!!!\n");
            bt_mcu_send_pw_cmd_poweron();
        }
        else
        {
            extern int ota_exit_cmd(void);
            if(ota_exit_cmd() == 1)
            {
                printk("POWER KEY exit OTA\n");
                    // struct app_msg msg = {0};
                    // msg.type = MSG_EXIT_APP;
                    //   send_async_msg(APP_ID_OTA, &msg);
                    int power_key_ota_exit(void);
                    power_key_ota_exit();
            }
            pd_manager_set_poweron_filte_battery_led(WLT_FILTER_STANDBY_POWEROFF);
            if(!ReadODM())
            {
                result = pd_get_sink_charging_state();
            }else{
                result = (sys_pm_get_power_5v_status() == 1) ? 1 : 0; 
            }
            
            if(result)
            {
                #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
                pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_CAHARGING);
                //power_manager_battery_display_handle(1, POWER_MANAGER_BATTER_10_SECOUND);
                #endif
                
                printk("POWER KEY --> sink charging!!!\n");
                bt_mcu_send_pw_cmd_poweron();
                if(run_mode_is_demo()){
                    printk("demo mode ,POWER KEY do nothing!!!\n");
                }
                else
                    charge_app_enter_cmd();
                //pd_bt_send_led_code(LED_SUPPLY_PROP_BT, SYS_EVENT_BT_UNLINKED);
                //pd_bt_send_led_code(LED_SUPPLY_PROP_AURACASE, BT_LED_STATUS_OFF);
            }
            else if(((sys_pm_get_power_5v_status() == 2) || (pd_manager_get_source_change_state()))  \
                        && (power_manager_get_battery_capacity() > BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1))    
            {
                #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
                pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_EXT_CAHARGE);
                //power_manager_battery_display_handle(1, POWER_MANAGER_BATTER_10_SECOUND);
                #endif

                printk("POWER KEY -->source charging!!!\n");
                bt_mcu_send_pw_cmd_poweron();
                if(run_mode_is_demo()){
                    printk("demo mode ,POWER KEY do nothing!!!\n");
                }
                else
                charge_app_enter_cmd();
            }
            else{
                printk("POWER KEY!!! %d \n",sys_pm_get_power_5v_status());
                #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
                if(power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
                {
                    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_NORMAL_OFF);
                }
                else
                {
                    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_2S);
                }// power_manager_battery_display_handle(1, POWER_MANAGER_BATTER_2_SECOUND);
                #endif      
                // pd_manager_send_cmd_code(PD_SUPPLY_PROP_STANDBY, 0);
                bt_mcu_send_pw_cmd_poweron();
                if(run_mode_is_demo()){
                    printk("demo mode ,poweroff, POWER KEY do nothing!!!\n");
                }
                else
                {
                //sys_event_notify(SYS_EVENT_POWER_OFF);
                    charge_app_enter_cmd();
                }
            }
        }    
            
    }          

}

static void mcu_dc_event_deal_fn(mcu_manager_charge_event_para_t *para)
{
    static bool dc_plug_status = false;

    bt_mcu_set_first_power_on_flag(1);
    if(para->mcu_event_val == MCU_INT_CMD_DC_IN)
    {
        bt_mcu_send_cmd_code(MCU_INT_TYPE_DC, MCU_INT_CMD_DC_IN);
        /***********exuce only one********************************* */
        if(!dc_plug_status)
        {
            SYS_LOG_INF("------> dc_in_OK\n");
            /* keep led on during pd recognition */
            bool run_mode_is_demo(void);
            if(!run_mode_is_demo()){
                if(get_batt_led_display_timer()>0){
                    pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,BATT_LED_ON_10S);
                }
            }
            dc_plug_status = true;
        }
        
        if(charge_warnning.dc_in_triggered_cnt == 0)
        {
            charge_warnning.dc_in_triggered_cnt++;
            charge_warnning.dc_in_filter_time = os_uptime_get_32();
        }
        else if(charge_warnning.dc_in_triggered_cnt == 1)
        {
            //filter 1.8*2s
            if((os_uptime_get_32() - charge_warnning.dc_in_filter_time) > 2*MCU_FIRST_TRIGGER_FILTER_TIME)
            {
                charge_warnning.dc_in_triggered_cnt++;
                charge_warnning.dc_in_filter_time = os_uptime_get_32();
            }
        }
    }else{
        dc_plug_status = false;
        if((charge_warnning.dc_in_triggered_cnt > 0))
        {
            if(charge_warnning.dc_in_remove_filter_time == 0)
            {
                charge_warnning.dc_in_remove_filter_time = os_uptime_get_32();
            }

            if((os_uptime_get_32() - charge_warnning.dc_in_remove_filter_time) > MCU_FIRST_TRIGGER_FILTER_TIME)
            {
                printk("\n%s,%d,dc_in_triggered_cnt:%d\n",__func__,__LINE__,charge_warnning.dc_in_triggered_cnt);
                charge_warnning.dc_in_triggered_cnt = 0;
                charge_warnning.dc_in_remove_filter_time = 0;
                //USB OUT REMOVE WATER WARNNING
                if(charge_warnning.bt_water_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
                {
                    charge_warnning.water_remove_filter_time = 0;
                    charge_warnning.water_filter_time = os_uptime_get_32();
                    charge_warnning.bt_water_warnning_status = BT_CHARGE_WARNNING_READY;
                }
                bt_mcu_send_cmd_code(MCU_INT_TYPE_DC, MCU_INT_CMD_DC_OUT);
            }
        }

#if 0        
        if(pd_get_app_mode_state() == CHARGING_APP_MODE){

            if((!pd_manager_get_source_change_state()) && (!dc_power_in_status_read()))
            {

                SYS_LOG_INF("DC OUT!!! \n");
                struct app_msg msg = {0};

                msg.type = MSG_HOTPLUG_EVENT;
                msg.cmd = 6;
                msg.value = 2;
                send_async_msg(APP_ID_MAIN, &msg); 
            }                 
        }
#endif        
    }
}

static void mcu_ntc_event_deal_fn(mcu_manager_charge_event_para_t *para)
{
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
    if(para->mcu_event_val >= MCU_INT_CMD_TEMPERATURE_HIGH)
    {
        bt_mcu_send_pw_cmd_poweron();
        if(mcu_poweron_is_charge_warnning())
        {

            if(sys_pm_get_power_5v_status() || dc_power_in_status_read())//2024.8.21  zth
            {
                if(pd_manager_get_sink_status_flag())
                {
                    pd_manager_disable_charging(true);
                }else if(pd_manager_get_source_status())
                {
                    pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 1);
                }
                if(!main_system_tts_get_play_warning_tone_flag())
                {
                    sys_event_notify(SYS_EVENT_CHARGING_WARNING);    
                }
                bt_mcu_set_bt_warning_poweroff_flag(1);
                charge_warnning.bt_is_charge_warnning_flag = true;
            }
            else
            {
                SYS_LOG_INF("[%d] \n", __LINE__);
                if(!main_system_tts_get_play_warning_tone_flag())
                {
                    sys_event_notify(SYS_EVENT_CHARGING_WARNING);    //---2024.8.15 zth
                }
                //offcharge mode dont need to poweroff ---2024.8.23-----zth
                if(!pd_get_app_mode_state()){
                    bt_mcu_set_bt_warning_poweroff_flag(2);
                }
                charge_warnning.bt_is_charge_warnning_flag = false;
            }
            //sys_event_notify(SYS_EVENT_CHARGING_WARNING);
            pd_manager_set_poweron_filte_battery_led(WLT_FILTER_CHARGINE_WARNING);
        }
    }
    else
    {
        pd_set_source_refrest();
        sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
        pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
        if(!pd_get_app_mode_state())
        {
            bt_mcu_set_bt_warning_poweroff_flag(2);   //----2024.8.15 zth  
        }
        charge_warnning.bt_is_charge_warnning_flag = false;
    }  
#else          
    if(para->mcu_event_val >= MCU_INT_CMD_TEMPERATURE_HIGH)
    {
        charge_warnning.ntc_remove_filter_time = 0;
        if(charge_warnning.bt_ntc_warnning_status == BT_CHARGE_WARNNING_NONE)
        {
            charge_warnning.bt_ntc_warnning_status = BT_CHARGE_WARNNING_READY;
            charge_warnning.ntc_filter_time = os_uptime_get_32();
        }
        else if(charge_warnning.bt_ntc_warnning_status == BT_CHARGE_WARNNING_READY)
        {
            //filter 1.8s
            if(((os_uptime_get_32() - charge_warnning.ntc_filter_time) > MCU_FIRST_TRIGGER_FILTER_TIME))
            {
                charge_warnning.bt_ntc_warnning_status = BT_CHARGE_WARNNING_TRIGGERED;
                charge_warnning.ntc_filter_time = os_uptime_get_32();
            }
        }        
    }
    else
    {
        if(charge_warnning.bt_ntc_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
        {
            if(charge_warnning.ntc_remove_filter_time == 0)
            {
                charge_warnning.ntc_remove_filter_time = os_uptime_get_32();
            }

            if((os_uptime_get_32() - charge_warnning.ntc_remove_filter_time) > MCU_FIRST_TRIGGER_FILTER_TIME)
            {
                charge_warnning.ntc_remove_filter_time = 0;
                charge_warnning.bt_ntc_warnning_status = BT_CHARGE_WARNNING_NONE;
                bt_mcu_send_cmd_code(MCU_INT_TYPE_NTC, MCU_INT_CMD_TEMPERATURE_NORMAL);
            }
        }
        else
        {
            charge_warnning.bt_ntc_warnning_status = BT_CHARGE_WARNNING_NONE;
            bt_mcu_send_cmd_code(MCU_INT_TYPE_NTC, MCU_INT_CMD_TEMPERATURE_NORMAL);
        }
    }  
#endif

}

static void mcu_water_event_deal_fn(mcu_manager_charge_event_para_t *para)
{
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
    if(para->mcu_event_val == MCU_INT_CMD_WATER_IN)
    {
        int charge_flag;
        bt_mcu_send_pw_cmd_poweron();
        if(mcu_poweron_is_charge_warnning())
        {
            charge_flag = sys_pm_get_power_5v_status();
            printk("\n%s/%d,charge_flag:%d,dc:%d\n",__func__,__LINE__,charge_flag,dc_power_in_status_read());
            if(charge_flag || dc_power_in_status_read())
            {
                if(pd_manager_get_sink_status_flag())
                {
                    pd_manager_disable_charging(true);
                }else if(pd_manager_get_source_status())
                {
                    pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 1);
                }
                if(!main_system_tts_get_play_warning_tone_flag())
                {
                    sys_event_notify(SYS_EVENT_CHARGING_WARNING);    
                }
                
                pd_manager_set_poweron_filte_battery_led(WLT_FILTER_CHARGINE_WARNING);
                charge_warnning.bt_is_charge_warnning_flag = true;
            }
            else
            {
                if(charge_warnning.bt_is_charge_warnning_flag)//2024.8.16 zth
                {
                    //pd_manager_disable_charging(false);
                    pd_set_source_refrest();

                    sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
                    pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
                    charge_warnning.bt_is_charge_warnning_flag = false;
                }    

            }
        }
    }
    else
    {
        pd_set_source_refrest();

        sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
        pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
        charge_warnning.bt_is_charge_warnning_flag = false;
    }
#else             
    if(para->mcu_event_val == MCU_INT_CMD_WATER_IN)
    {
        charge_warnning.water_remove_filter_time = 0;
        if(charge_warnning.bt_water_warnning_status == BT_CHARGE_WARNNING_NONE)
        {
            charge_warnning.bt_water_warnning_status = BT_CHARGE_WARNNING_READY;
            charge_warnning.water_filter_time = os_uptime_get_32();
        }
        else if(charge_warnning.bt_water_warnning_status == BT_CHARGE_WARNNING_READY)
        {
            //filter 1.8*2 = 3.6s
            if(((os_uptime_get_32() - charge_warnning.water_filter_time) > 2*MCU_FIRST_TRIGGER_FILTER_TIME)
            &&(charge_warnning.dc_in_triggered_cnt == MCU_CHARGE_WARNNING_FILTER_TIMES))
            {
                charge_warnning.bt_water_warnning_status = BT_CHARGE_WARNNING_TRIGGERED;
                charge_warnning.water_filter_time = os_uptime_get_32();
                charge_warnning.dc_in_remove_filter_time = 0;
            }
        }

    }
    else
    {
        if(charge_warnning.bt_water_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
        {
            if(charge_warnning.water_remove_filter_time == 0)
            {
                charge_warnning.water_remove_filter_time = os_uptime_get_32();
            }

            if((os_uptime_get_32() - charge_warnning.water_remove_filter_time) > MCU_FIRST_TRIGGER_FILTER_TIME)
            {
                charge_warnning.water_remove_filter_time = 0;
                charge_warnning.bt_water_warnning_status = BT_CHARGE_WARNNING_NONE;
                bt_mcu_send_cmd_code(MCU_INT_TYPE_WATER, MCU_INT_CMD_WATER_OUT);
            }
        }
        else
        {
            charge_warnning.bt_water_warnning_status = BT_CHARGE_WARNNING_NONE;
            bt_mcu_send_cmd_code(MCU_INT_TYPE_WATER, MCU_INT_CMD_WATER_OUT);
        }
    }
#endif    

}

static void bt_ntc_charge_warnnig_trigger_fn(void)
{
    if(pd_get_app_mode_state() == CHARGING_APP_MODE){
        if(charge_app_exit_cmd() == 1){
            struct app_msg msg = {0};

            mcu_ui_set_poweron_from_charge_warnning_status(1);
            msg.type = MSG_POWER_KEY;
            send_async_msg(APP_ID_MAIN, &msg);

/*                     k_sleep(10);
            msg.type = MSG_CHARGER_MODE;
            msg.cmd = 0; */

            send_async_msg(CONFIG_SYS_APP_NAME, &msg);

            bt_mcu_send_pw_cmd_poweron();               

        }
    }
    else
    {
        if(sys_pm_get_power_5v_status() || dc_power_in_status_read())//charging or dc in 2024.8.21  zth
        {
            if(pd_manager_get_sink_status_flag())
            {
                pd_manager_disable_charging(true);
            }else if(pd_manager_get_source_status())
            {
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 1);
            }
            if(!main_system_tts_get_play_warning_tone_flag())
            {
                sys_event_notify(SYS_EVENT_CHARGING_WARNING);    
            }
            bt_mcu_set_bt_warning_poweroff_flag(1);
            charge_warnning.bt_is_charge_warnning_flag = true;
        }
        else
        {
            SYS_LOG_INF("[%d] \n", __LINE__);
            if(!main_system_tts_get_play_warning_tone_flag())
            {
                sys_event_notify(SYS_EVENT_CHARGING_WARNING);    // ntc warn ----2024.8.15 zth
            }
            bt_mcu_set_bt_warning_poweroff_flag(2);
            charge_warnning.bt_is_charge_warnning_flag = false;
        }
        //sys_event_notify(SYS_EVENT_CHARGING_WARNING);
        pd_manager_set_poweron_filte_battery_led(WLT_FILTER_CHARGINE_WARNING);
    }    

}

static void bt_ntc_charge_warnnig_remove_fn(void)
{
    /*if(wlt_pd_manager->pd_sink_status_flag){
        pd_manager_disable_charging(false);
    }else{
        if((power_manager_get_battery_capacity() > BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1))
        {
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 0);
        }
    }*/
    pd_set_source_refrest();
    sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
    pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
    //  if(sys_pm_get_power_5v_status())
    {
        bt_mcu_set_bt_warning_poweroff_flag(2);   //remove ntc warn and power off----2024.8.15 zth  
    }
    charge_warnning.bt_is_charge_warnning_flag = false;
}

static void bt_water_charge_warnnig_trigger_fn(void)
{
    int charge_flag;
    if(pd_get_app_mode_state() == CHARGING_APP_MODE){
        if(charge_app_exit_cmd() == 1){
            struct app_msg msg = {0};

            mcu_ui_set_poweron_from_charge_warnning_status(1);
            msg.type = MSG_POWER_KEY;
            send_async_msg(APP_ID_MAIN, &msg);

/*                     k_sleep(10);
            msg.type = MSG_CHARGER_MODE;
            msg.cmd = 0; */

            send_async_msg(CONFIG_SYS_APP_NAME, &msg);

            bt_mcu_send_pw_cmd_poweron();               

        }
    }
    else
    {
        charge_flag = sys_pm_get_power_5v_status();
        printk("\n%s/%d,charge_flag:%d,dc:%d\n",__func__,__LINE__,charge_flag,dc_power_in_status_read());
        //if(/*charge_flag || */dc_power_in_status_read())
        {

            pd_manager_disable_charging(true);
            // otg off must be early， so process in  pd_manager_battery_low_check_otg() funcion
            
            if(!main_system_tts_get_play_warning_tone_flag())
            {
                sys_event_notify(SYS_EVENT_CHARGING_WARNING);    
            }
            
            pd_manager_set_poweron_filte_battery_led(WLT_FILTER_CHARGINE_WARNING);
            charge_warnning.bt_is_charge_warnning_flag = true;
        }
        #if 0
        else
        {
            if(charge_warnning.bt_is_charge_warnning_flag)//2024.8.16 zth
            {
                //pd_manager_disable_charging(false);
                pd_set_source_refrest();

                sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
                pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
                charge_warnning.bt_is_charge_warnning_flag = false;
            }    

        }
        #endif
    }

}

static void bt_water_charge_warnnig_remove_fn(void)
{
    // if(wlt_pd_manager->pd_sink_status_flag){
    //     pd_manager_disable_charging(false);
    // }else{
    //     if((power_manager_get_battery_capacity() > BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1))
    //     {
    //         pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 0);
    //     }
    // }
    //pd_manager_disable_charging(false);
    pd_set_source_refrest();

    sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
    pd_manager_set_poweron_filte_battery_led(WLT_FILTER_NOTHING);
    charge_warnning.bt_is_charge_warnning_flag = false;
}

void mcu_charge_warnning_event_deal(void)
{
    static uint8_t last_water_warnning_status = 0;
    static uint8_t last_ntc_warnning_status = 0;
	static uint8_t warnning_3_second_cnt = 0;
	if(warnning_3_second_cnt++ < 3)
	{
		return;
	}
	warnning_3_second_cnt = 0;   
    SYS_LOG_INF("ntc:%d,last_ntc:%d,water:%d,last_water:%d\n",charge_warnning.bt_ntc_warnning_status,last_ntc_warnning_status,
    charge_warnning.bt_water_warnning_status,last_water_warnning_status); 
    if(charge_warnning.bt_ntc_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
    {
        last_ntc_warnning_status = BT_CHARGE_WARNNING_TRIGGERED;
        bt_ntc_charge_warnnig_trigger_fn();
    }
    else
    {
        if(last_ntc_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
        {
            last_ntc_warnning_status = 0;
            bt_ntc_charge_warnnig_remove_fn();
        }
    }

    if(charge_warnning.bt_water_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
    {
        last_water_warnning_status = BT_CHARGE_WARNNING_TRIGGERED;
        bt_water_charge_warnnig_trigger_fn();
    }
    else
    {
        if(last_water_warnning_status == BT_CHARGE_WARNNING_TRIGGERED)
        {
            last_water_warnning_status = 0;
            bt_water_charge_warnnig_remove_fn();
        }
    }
}

uint8_t mcu_get_water_charge_warnning_status(void)
{
    return charge_warnning.bt_water_warnning_status;
}

void mcu_supply_report(mcu_charge_event_t event, mcu_manager_charge_event_para_t *para)
{

    //uint8 result=0;
    
    if (!wlt_pd_manager_is_init()) {
		return;
	}

    SYS_LOG_INF("[%d] event=%d, para = %d app_mode =%d \n", __LINE__, event, para->pd_event_val,pd_get_app_mode_state());

    switch(event)
    {

        case MCU_INT_TYPE_POWER_KEY:
            mcu_power_key_deal_fn(para);
            break;

        case MCU_INT_TYPE_DC:
            mcu_dc_event_deal_fn(para);
            break;

        case MCU_INT_TYPE_NTC: 
            mcu_ntc_event_deal_fn(para);
            break;

        case MCU_INT_TYPE_WATER: 
            mcu_water_event_deal_fn(para);
            break;
        case MCU_INT_TYPE_HW_RESET:
            if(run_mode_is_demo()){
                bt_mcu_set_first_power_on_flag(1);
                printk("demo mode ,mcu hw reset!!!\n");
                bt_mcu_send_pw_cmd_poweron();
            }
            else
            {
                printk("mcu hw reset!!! \n");
               // pd_manager_deinit(0);
                //sys_pm_poweroff();    
            }
            break;
        default:
        {
            printk("UNKOWN EVENT ,DO NOTHING , NO POWEROFF!!!\n");
            bt_mcu_send_pw_cmd_poweron();
        }
            break;

        //if(bt_mcu_get_bt_wake_up_flag())
        //{
        //    bt_mcu_set_bt_wake_up_flag(0);
        //}
    }

}


///////////////////////////////////////hw version/////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <adc.h>
#include <misc/byteorder.h>


struct hw_ver_detect_info {
	struct device *adc_dev;
	struct adc_seq_table seq_tbl;
	struct adc_seq_entry seq_entrys;
	u8_t adcval[4];
};

uint8_t Read_hw_ver(void)
{
    uint8_t hw_info;

#define HW_VER_PIN		22 // TONLI:0 PRE-EV3,3.3 DV1; ggec 0 PRE-EV1,3.3 PRE-EV2; 
    struct hw_ver_detect_info hw_ver;  
    int adc_val; 
	/* LRADC2: GPIO22  */

	hw_ver.adc_dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_NAME);
	if (!hw_ver.adc_dev) {
		SYS_LOG_ERR("cannot found ADC device %s\n", CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_NAME);
		return 0;
	}
    sys_write32((sys_read32(GPIO_CTL(HW_VER_PIN)) & ~(GPIO_CTL_MFP_MASK))  | (0x1 << 3), GPIO_CTL(HW_VER_PIN));
	hw_ver.seq_entrys.sampling_delay = 0;
	hw_ver.seq_entrys.buffer = (u8_t *)&hw_ver.adcval;
	hw_ver.seq_entrys.buffer_length = sizeof(hw_ver.adcval);
	hw_ver.seq_entrys.channel_id = ADC_ID_CH2;

	hw_ver.seq_tbl.entries = &hw_ver.seq_entrys;
	hw_ver.seq_tbl.num_entries = 1;

	adc_enable(hw_ver.adc_dev);  
	/* wait 10ms */
	//k_busy_wait(20000);

	adc_read(hw_ver.adc_dev, &hw_ver.seq_tbl);
	adc_val = sys_get_le16(hw_ver.seq_entrys.buffer);

    printk("\n %s,%d , hw_ver : %x !!!\n",__func__,__LINE__,adc_val);
	
    if(adc_val < 0xdd)
    {
        hw_info = GGC_EV1_TONLI_EV3;
    }
    else{
        hw_info = GGC_EV2_TONLI_DV1;
    }

    return hw_info;    

}
