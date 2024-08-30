#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>


#include <i2c.h>
#include <adc.h>
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
#include <power_supply.h>
#include <pd_manager_supply.h>

#include "app/charge_app/charge_app.h"
#include "power_manager.h"
#include <mcu_manager_supply.h>
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

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

#define I2C_BAT_DEV_ADDR                            0x55
#define KELVIN_TEMPERATURE_BASE_VAL                 2731

#define MAX_G1_G2_DECOUNCE_TIME                     3

#define PD_BUS_POWER_9V_VOLT                        8500

int bt_mcu_send_pw_cmd_powerdown(void);
int pd_manager_send_cmd_code(uint8_t type, int code);
int pd_manager_deinit(int value);
uint8_t ReadODM(void);
bool pd_get_sink_charging_state(void);
bool pd_set_source_refrest(void);
static os_mutex pd_iic_data_lock;
static PD_CIRCLES_QUEUE_TYPE pd_circles_queue; 

uint8_t pd_get_app_mode_state(void);
// static int battery_temperature=200;
extern void mcu_supply_report(mcu_charge_event_t event, mcu_manager_charge_event_para_t *para);

#ifdef CONFIG_C_TEST_BATT_MACRO

static uint8_t test_id = 1;
static int battery_cap= 3;
static int battery_temperature=250;

void set_battery_cap(bool flag)
{
    if(flag)
    {
        battery_cap += 1;
    }else{
        battery_cap -= 1;
    }
    
}

void set_battery_temprature_test(bool flag)
{
    if(flag)
    {
        battery_temperature += 10;
    }else{
        battery_temperature -= 10;
    }
    
}

static void set_typec_temperatur_test(bool flag)
{
    mcu_manager_charge_event_para_t para;
    if(flag)
    {
        para.mcu_event_val = 3;
    }else{
        para.mcu_event_val = 0;
    }
    mcu_supply_report(MCU_INT_TYPE_NTC,&para);    
}

static void set_typec_water_test(bool flag)
{
    mcu_manager_charge_event_para_t para;
    if(flag)
    {
        para.mcu_event_val = 1;
    }else{
        para.mcu_event_val = 0;
    }
    mcu_supply_report(MCU_INT_TYPE_WATER,&para);    
}

uint8_t wlt_simulation_get_test_id(void)
{
    return test_id;
}

uint8_t wlt_simulation_test_switch(void)
{
    if(test_id < WLT_TEST_ID_NUM){
        test_id++;
    }
    else{
        test_id = 0;
    }
    printk("[%s/%d],id = %d !!!\n\n",__func__,__LINE__,test_id);
    return test_id;
}

void wlt_simulation_test_handle(bool flag)
{
    uint8_t id = wlt_simulation_get_test_id();

    switch(id)
    {
        case WLT_BATTERY_CAP_TEST:
        printk("[%s/%d],WLT_BATTERY_CAP_TEST !!!\n\n",__func__,__LINE__);
        set_battery_cap(flag);
        break;

        case WLT_BATTERY_TEMPERATURE_TEST:
        printk("[%s/%d],WLT_BATTERY_TEMPERATURE_TEST !!!\n\n",__func__,__LINE__);
        set_battery_temprature_test(flag);
        break;

        case WLT_TYPC_TEMPERATURE_TEST:
        printk("[%s/%d],WLT_TYPC_TEMPERATURE_TEST !!!\n\n",__func__,__LINE__);
        set_typec_temperatur_test(flag);
        break;

        case WLT_TYPC_WATER_TEST:
        printk("[%s/%d],WLT_TYPC_WATER_TEST !!!\n\n",__func__,__LINE__);
        set_typec_water_test(flag);
        break;

        default:
        printk("[%s/%d],UNKOWN TEST !!!\n\n",__func__,__LINE__);
        break;

    }
}
#endif

//////////////////////////////////battery///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
#define BATTERY_CAP_IS_VALID_BASE       5
#define BATTERY_TEMP_IS_VALID_BASE      100
#define BATTERY_VALUE_CHECK_TIMERS      5
////*************************************************************************** */////
static bool _battery_compare_value_is_valid(int last,int current,int base)
{
    int temp;
    bool ret;
    if(last > current)
    {
        temp = last - current;
    }
    else
    {
        temp = current - last;
    }

    if(temp > base)
    {
        ret = 0;
    }
    else
    {
        ret = 1;
    }

    return ret;
}


void battery_read_iic_value(void)
{
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
    uint8_t check_cnt = 0;
    int last_cap = 0;
    int current_cap = 0;
    int last_temp = 0;
    int current_temp = 0;
    uint8_t cap_is_valid_cnt = 0;
    uint8_t temp_is_valid_cnt = 0;
    int current_volt = 0;
    int16_t current_cur = 0;
    int current_status = 0;
    
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    k_sleep(1);
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_CAP_IIC_ADDR, buf, 2);
    //printf("Totti debug BAT value: 0x%x 0x%x\n", buf[1], buf[0]);   
    power_manager_set_battery_vol((buf[1]<<8 | buf[0])/2);                                  // two battery/2;   
    current_volt = (u16_t)(buf[1]<<8 | buf[0]);

    for(check_cnt = 0;check_cnt < BATTERY_VALUE_CHECK_TIMERS;check_cnt++)
    {
        if(i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_REMAIN_CAP_IIC_ADDR, buf, 2) == 0)
        {
            //printf("zth debug BAT cap: 0x%x 0x%x\n", buf[1], buf[0]);   
            //pd_manager->global_bat_namager.battery_remain_cap = (u16_t)(buf[1]<<8 | buf[0]);
            current_cap = (u16_t)(buf[1]<<8 | buf[0]);
            if(last_cap == 0)
            {
                last_cap = current_cap;
            }
            if(_battery_compare_value_is_valid(last_cap,current_cap,BATTERY_CAP_IS_VALID_BASE))
            {
                cap_is_valid_cnt++;
            }
            last_cap = current_cap;
        }
        if(i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_TEMPERATURE_IIC_ADDR, buf, 2) == 0)
        {
            current_temp = (int)(buf[1]<<8 | buf[0]);
            if(last_temp == 0)
            {
                last_temp = current_temp;
            }
            if(_battery_compare_value_is_valid(last_temp,current_temp,BATTERY_TEMP_IS_VALID_BASE))
            {
                temp_is_valid_cnt++;
            }
            last_temp = current_temp;            
  
        }
        k_sleep(1);
    }

    if(cap_is_valid_cnt >= BATTERY_VALUE_CHECK_TIMERS)
    {
    #ifdef CONFIG_C_TEST_BATT_MACRO
        printf("Totti debug BAT real cap: %d \n", battery_cap); 
        power_manager_set_battery_cap(battery_cap);
    #else
        power_manager_set_battery_cap((u16_t)current_cap);
	    printf("power_manager_set_battery_cap: %d\n", current_cap);
    #endif
        //printf("zth debug battery_remain_cap: %d%% \n", current_cap);
    }

    if(temp_is_valid_cnt >= BATTERY_VALUE_CHECK_TIMERS)
    {
        #ifdef CONFIG_C_TEST_BATT_MACRO   
        power_manager_set_battery_temperature(battery_temperature);
        #else   
        power_manager_set_battery_temperature( ((int)current_temp) - KELVIN_TEMPERATURE_BASE_VAL);
        #endif 
    }
    //printf("zth debug BAT temp: 0x%x 0x%x\n", buf[1], buf[0]);  
    //pd_manager->global_bat_namager.battery_temperature = ((int)(buf[1]<<8 | buf[0])) - KELVIN_TEMPERATURE_BASE_VAL;
    // int temp_value = ((int)(buf[1]<<8 | buf[0])) - KELVIN_TEMPERATURE_BASE_VAL;

    //i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_REMAIN_CHARGE_TIME_IIC_ADDR, buf, 2);
    //power_manager_set_battery_remain_charge_time((u16_t)(buf[1]<<8 | buf[0]));
  
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_CURRENT_IIC_ADDR, buf, 2);
    current_cur = (int16_t)(buf[1]<<8 | buf[0]);
    
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_STATUS_IIC_ADDR, buf, 2);
    current_status = (u16_t)(buf[1]<<8 | buf[0]);
    
    //i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_AVERAGE_CURRENT_IIC_ADDR, buf, 2);
    //current_cur = (int16_t)(buf[1]<<8 | buf[0]);
    printk("\nbattery status,temp:%d,volt:%dmv,current:%dmA,cap:%d%%,status:0x%x\n",((int)current_temp) - KELVIN_TEMPERATURE_BASE_VAL,current_volt,
    current_cur,current_cap,current_status);
}

static void battery_iic_init(void)
{
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_CAP_IIC_ADDR, buf, 2);
    //printf("Totti debug BAT value: 0x%x 0x%x\n", buf[1], buf[0]);

    power_manager_set_battery_vol((buf[1]<<8 | buf[0])/2);                                  // two battery/2;
    
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_REMAIN_CAP_IIC_ADDR, buf, 2);
    //printf("zth debug BAT cap: 0x%x 0x%x\n", buf[1], buf[0]);   
    //pd_manager->global_bat_namager.battery_remain_cap = (u16_t)(buf[1]<<8 | buf[0]);

#ifdef CONFIG_C_TEST_BATT_MACRO
    SYS_LOG_INF("Totti debug BAT real cap: %d \n", battery_cap); 
    power_manager_set_battery_cap(battery_cap);
#else
    power_manager_set_battery_cap((u16_t)(buf[1]<<8 | buf[0]));
#endif
}


#define PD_RUN_TIME_PERIOD              100
#define PD_CHARGE_TEN_MINITE_COUNT      6000                // 10 minites
#define MAX_SOURCE_CHANGE_TIME          23                  // 2.3 seconds


#define MAX_POWER_KEY_DEBOUNCE_TIME     25

/**
 * @brief Auto-negotiate and configure link parameters.
 * @author Totti 
 * @data	2023/12/21
 * @param 
 * @param 
 * @return 
 */



struct wlt_pd_manager_info {
	struct device *dev;
    struct device *adc_wio_dev;
    struct thread_timer timer;

    /** _PD status */
	u16_t   dc_sink_charge_count;

    u32_t   pd_sink_source_chg_flag:1;
    u32_t   pd_sink_status_flag:1;
    u32_t   pd_source_status_flag:1;
    u32_t   app_music_flag:1;
    u32_t   pd_sink_only_mode:1;
    u32_t   pd_sink_full_state:1;
    u32_t   otg_off_flag:1;
	u32_t   battery_security_flag:1;
	u32_t   pd_manager_finish_flag:1;
    u32_t   pd_manager_low_cur_flag:1;
    u32_t   pd_sink_dischg_flag:1;

     /** _app music current status */
    u8_t    bt_app_mode;
    u8_t    source_change_debunce_count;
    
    

};

static uint8_t bt_warning_poweroff_flag = 0;
struct wlt_pd_manager_info *wlt_pd_manager = NULL;
static struct wlt_pd_manager_info global_pd_manager;
static bool bt_first_power_on_flag = 0;
static bool bt_standy_mode_flag = 0;
static bool bt_wake_up_flag = 0;
static uint8_t poweron_filte_battery_led_flag = WLT_FILTER_NOTHING;
static u16_t power_key_debounce_count = 0x00;


extern bool media_player_is_working(void);
extern bool  wlt_led_timer_init(void);
extern bool sys_check_standby_state(void);

void pd_manager_set_poweron_filte_battery_led(uint8_t flag)
{
    poweron_filte_battery_led_flag = flag;
    SYS_LOG_INF("[%d],flag =%d\n", __LINE__, flag);
}

uint8_t pd_manager_get_poweron_filte_battery_led(void)
{
    SYS_LOG_INF("[%d],flag =%d\n", __LINE__, poweron_filte_battery_led_flag);
    return poweron_filte_battery_led_flag;
}
#if 0
static void pd_manger_poweron_filte_battery_led_handle(void)
{
    static uint8_t filter_cnt = 0;

    if(poweron_filte_battery_led_flag == WLT_FILTER_CHARGINE_POWERON)
    {
        filter_cnt++;
        if(filter_cnt > 3){
            poweron_filte_battery_led_flag = WLT_FILTER_NOTHING;
            filter_cnt = 0;
        }
    }
    else{
        filter_cnt = 0;
    }
}
#endif
static void bt_warning_poweroff_handle_fn(void)
{
    //bt_mcu_send_pw_cmd_powerdown();
    // pd_bq25798_adc_off();
    // pd_manager_send_cmd_code(PD_SUPPLY_PROP_STANDBY, 0);
    sys_event_notify(SYS_EVENT_POWER_OFF);    
}

static  void battery_power_supply_enable(void)
{
	struct device *bat_dev;
    bat_dev = (struct device *)device_get_binding("battery");
	const struct power_supply_driver_api *api = bat_dev->driver_api;
	api->enable(bat_dev);
}

extern bool ODM_FLAG;

int bt_mcu_send_cmd_code(u8_t type, int code)
{/*
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    val.intval = code;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->mcu_response(wlt_pd_manager->dev, type, &val);
    
    return 0;*/
    struct device *mcu_dev = NULL;


  if(!ReadODM())

    {
        mcu_dev = wlt_device_get_mcu_mspm0l();
    }
    else
    {
        mcu_dev = wlt_device_get_mcu_ls8a10049t();
    }
 
    union mcu_manager_supply_propval val;
    if(mcu_dev == NULL)
        return -1;
        
    val.intval = code;

    const struct mcu_manager_supply_driver_api *api = mcu_dev->driver_api;
    api->mcu_response(mcu_dev, type, &val);  
   
    return 0;      
}

int bt_mcu_send_pw_cmd_standby(void)
{
    bt_standy_mode_flag = 1;
    return bt_mcu_send_cmd_code(MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_STANDBY);
}

int bt_mcu_send_pw_cmd_powerdown(void)
{
    return bt_mcu_send_cmd_code(MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_POWEROFF);
}


int logic_mcu_ls8a10023t_otg_mobile_det()
{
    return bt_mcu_send_cmd_code(MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_OTG_MOBILE_ON);
}

int bt_mcu_send_pw_cmd_charging(void)
{
    return bt_mcu_send_cmd_code(MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_CHARGING);
}

int bt_mcu_send_pw_cmd_poweron(void)
{
    return bt_mcu_send_cmd_code(MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_POWERON);
}

void bt_mcu_send_pw_cmd_exit_standy(void)
{
    bt_wake_up_flag = 1;
    bt_standy_mode_flag = 0;    
}

bool bt_mcu_get_pw_cmd_exit_standy(void)
{
    return bt_standy_mode_flag;
}

void bt_mcu_set_first_power_on_flag(bool flag)
{
    bt_first_power_on_flag = flag;
}

bool bt_mcu_get_first_power_on_flag(void)
{
    return bt_first_power_on_flag;
}

void bt_mcu_set_bt_warning_poweroff_flag(u8_t flag)
{
    bt_warning_poweroff_flag = flag;
  //  SYS_LOG_INF("bt_warning_poweroff_flag:%d", bt_warning_poweroff_flag);
}

uint8_t bt_mcu_get_bt_warning_poweroff_flag(void)
{
    return bt_warning_poweroff_flag;
}

void bt_mcu_set_bt_wake_up_flag(bool flag)
{
    bt_wake_up_flag = flag;
}

bool bt_mcu_get_bt_wake_up_flag(void)
{
    return bt_wake_up_flag;
}
/*
*   true; g1 = high; g2 =low;
*   false: g1 = low; g2 = high;
*/
uint8_t Delay_ON_G1G2_flag = 0;
uint8_t Delay_times = 0;

void  pd_manager_v_sys_g1_g2(uint8_t flag)
{
    static uint8_t vsys_power_flag = 0;
    struct device *gpio_dev;
    
 
    if(vsys_power_flag != flag)
    {
    
        vsys_power_flag = flag;
	    gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	    if (!gpio_dev) 
	    {
		    printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		    return;
	    }

        
        switch(flag)
        {
            case PD_V_BUS_G1_G2_INIT:
                gpio_pin_configure(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, GPIO_DIR_OUT | GPIO_POL_NORMAL);
                gpio_pin_configure(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, GPIO_DIR_OUT | GPIO_POL_NORMAL);
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 0);
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, 1);
                SYS_LOG_INF("[%d] g1 on, g2 off \n",  __LINE__);
				Delay_ON_G1G2_flag = 0;
                break;

            case PD_V_BUS_G1_LOW_G2_HIGH:
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, 0);
              //  k_sleep(1000);
                Delay_ON_G1G2_flag = 1;
			    Delay_times = 0;
               // gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 1);
                SYS_LOG_INF("[%d] g2 on, g1 off \n",  __LINE__);
                break;


            case PD_V_BUS_G1_G2_DEINIT:
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 0);
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, 0);
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_ONOFF_G2, 0);
                SYS_LOG_INF("[%d] g1 off, g2 off \n",  __LINE__);
                break;


            case PD_V_BUS_G1_HIGH_G2_LOW:
            default:
                gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 0);
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_ONOFF_G2, 0);

			    Delay_ON_G1G2_flag = 2;
				Delay_times = 0;
               // k_sleep(1000);
               // gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, 1);

                SYS_LOG_INF("[%d] g1 on, g2 off \n",  __LINE__);
                break;
        }
    }
}


int pd_bt_send_led_code(uint8_t type, int code)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    val.intval = code;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->set_property(wlt_pd_manager->dev, type, &val);

    return 0;
}


int pd_manager_send_cmd_code(uint8_t type, int code)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    val.intval = code;
    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;

    api->set_property(wlt_pd_manager->dev, type, &val);

    return 0;
}

void pd_manager_disable_charging(bool flag)
{
    // if(wlt_pd_manager->pd_sink_status_flag)
    {
        SYS_LOG_INF("[%d]; flag=%d", __LINE__, flag);

        wlt_pd_manager->pd_sink_dischg_flag = flag;

        if(flag)
        {
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_SINK_CHANGING_OFF, 0);
        }else{
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_SINK_CHANGING_ON, 0); 
        }
    }
    
}


void pd_manager_mcu_int_deal(void)
{
    if(bt_standy_mode_flag)
    {
        return;
    }

	if(!ReadODM())
	{
		mcu_mspm0l_int_deal();
	}
    else 
    {
		mcu_ls8a10049t_int_deal();
    }
    
    if(bt_mcu_get_bt_wake_up_flag())
    {
        bt_mcu_set_bt_wake_up_flag(0);
    }  
	
	
}

int pd_manager_check_water_alarm(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_TYPEC_WATER_WARNING_TRIGGER_CNT, &val);

    return val.intval;

}


#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
static void pa_manager_otg_phone_power_handle(void)
{
    static uint8_t ten_seconds_cnt = 0;
    if(pd_get_app_mode_state() == CHARGING_APP_MODE
    && power_manager_get_charge_status() == POWER_SUPPLY_STATUS_DISCHARGE){
        ten_seconds_cnt++;
        if(ten_seconds_cnt > 10)
        {
            printk("\n %s,otg phone power need shutdown dut  \n",__func__);
            ten_seconds_cnt = 0;
            pd_manager_deinit(0);
            mcu_ui_set_real_shutdown_dut(true);
            sys_pm_poweroff(); 
        }
    } 
    else
    {
        ten_seconds_cnt = 0;
    }   
}
#endif
void pd_managet_typec_high_temp_protect(void)
{
    static uint8_t five_second_cnt = 0;


   //  SYS_LOG_INF("[%d] bt_warning_poweroff_flag:%d, count:%d \n ", __LINE__, bt_warning_poweroff_flag, five_second_cnt);


    if(bt_warning_poweroff_flag == 2){
        if(five_second_cnt >= 5)
        {
            bt_warning_poweroff_flag = 0;
            five_second_cnt = 0;
            bt_warning_poweroff_handle_fn();
        }
        five_second_cnt++;
    }
    else{
        five_second_cnt = 0;
    }
}


int pd_manager_get_power_key_debounce(void)
{
    if(power_key_debounce_count == 0x00)
    {
        power_key_debounce_count = MAX_POWER_KEY_DEBOUNCE_TIME;
        return 0;
    }else{
        return 1;
    }
}

void pd_manager_power_key_time(void)
{
    if(power_key_debounce_count)
    {
        power_key_debounce_count--;
    }
}


void pd_manager_battery_low_check_otg(bool force_flag)
{
   
    if(wlt_pd_manager == NULL)
        return;

    if(wlt_pd_manager->bt_app_mode == BTMODE_APP_MODE)
    {
        if((!wlt_pd_manager->pd_sink_status_flag) || force_flag)
        {

            if(wlt_pd_manager->otg_off_flag)
            {
                if(!pd_manager_check_water_alarm())
                {
                    if(power_manager_get_battery_capacity() >= (BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1 + 1))
                    {
                        wlt_pd_manager->otg_off_flag = false;
                        pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 0);
                    }
                }                  
            }else{

                if(pd_manager_check_water_alarm())
                {
                    if(!wlt_pd_manager->otg_off_flag)
                    {
                        wlt_pd_manager->otg_off_flag = true;
                        pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 1);
                    }
                }else
                {
                    if(power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1) 
                    {

                        if(sys_check_standby_state())
                        {
                            SYS_LOG_INF("[%d] source volt < 15, power down! \n", __LINE__);
                            pd_manager_deinit(0);
                            sys_pm_poweroff();               


                        }
                        else{
                            
                            if(!wlt_pd_manager->otg_off_flag)
                            {
                                wlt_pd_manager->otg_off_flag = true;
                                pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 1);
                            }
                        }    

                    }else if(force_flag)
                    {
                        wlt_pd_manager->otg_off_flag = false;
                        pd_manager_send_cmd_code(PD_SUPPLY_PROP_OTG_OFF, 0);
                    } 
                }              
            }
        }

    }

    if(((wlt_pd_manager->pd_source_status_flag) && (wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE))          \
            || (!pd_get_sink_charging_state() && wlt_pd_manager->otg_off_flag && sys_check_standby_state()))
    {
        if(power_manager_get_battery_capacity() <= BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
        {
            SYS_LOG_INF("[%d] battery cap < 15, power down! \n", __LINE__);
            pd_manager_deinit(0);
            sys_pm_poweroff();       
        }
    }

}




/*
*
* @param flag=1 force refresh; flag=0, have changed refresh
*/

bool pd_set_source_refrest(void)
{

    // if(run_mode_is_demo())
    // {
    //     return false;
    // }
    
    if(run_mode_is_demo())
    {
        wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;

		pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_SSRC, 1);

    }else{
        // pd_manager_disable_charging(false);

        pd_manager_battery_low_check_otg(true);
        
        // k_sleep(500);
        pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_SSRC, 0);
    }

    return true;
}

extern bool music_get_tts_count(void);

bool pd_set_source_current(bool flag)
{
    if(wlt_pd_manager == NULL)
        return -1;

    if((wlt_pd_manager->pd_source_status_flag) || flag)
    {
         
        if(wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE)
        {
            SYS_LOG_INF("[%d] flag=%d; %d\n", __LINE__, flag, wlt_pd_manager->bt_app_mode);
            // wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;

            pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CHARGING_OTG, 0);
        }else{

            // if(flag || (wlt_pd_manager->app_music_flag  != (media_player_is_working() && music_get_tts_count())))
            // {
            //     SYS_LOG_INF("[%d] %d %d \n", __LINE__, media_player_is_working(), music_get_tts_count());
            //     wlt_pd_manager->app_music_flag = media_player_is_working() && music_get_tts_count();
            //     wlt_pd_manager->source_change_debunce_count = 0x00;
            //     if(wlt_pd_manager->app_music_flag)
            //     {
            //         SYS_LOG_INF("[%d] flag=%d; %d\n", __LINE__, flag, wlt_pd_manager->bt_app_mode);
            //         pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_500MA, 0);
            //     }else{
            //         SYS_LOG_INF("[%d] flag=%d; %d\n", __LINE__, flag, wlt_pd_manager->bt_app_mode);
            //         pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA, 0);
            //         // wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
            //     } 
            // }

            // wlt_pd_manager->source_change_debunce_count = 0x00;
            SYS_LOG_INF("[%d] flag=%d; %d\n", __LINE__, flag, wlt_pd_manager->bt_app_mode);
            if(wlt_pd_manager->pd_source_status_flag)
            {
               //  wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
            }

            if(run_mode_is_demo())
            {
                wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
            }

            pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA, 0);

        }
    }
    return true;
}

int pd_manager_get_volt_info(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SINK_VOLT_VAULE, &val);

    return val.intval;
}

void pd_manager_G1_G2_debounce_process(bool flag)
{
    static int debounce_count = 0;

    if(flag)
    {

        if((pd_manager_get_volt_info() > PD_BUS_POWER_9V_VOLT) && (!wlt_pd_manager->pd_sink_full_state))
        {
            
            if(debounce_count++ >= MAX_G1_G2_DECOUNCE_TIME)
            {
                debounce_count = MAX_G1_G2_DECOUNCE_TIME + 1;
                pd_manager_v_sys_g1_g2(PD_V_BUS_G1_LOW_G2_HIGH);
            }
            
        }else{
            pd_manager_v_sys_g1_g2(PD_V_BUS_G1_HIGH_G2_LOW);
            debounce_count = 0x00;
        }   


    }else{
        pd_manager_v_sys_g1_g2(PD_V_BUS_G1_HIGH_G2_LOW);
        debounce_count = 0x00;
    }

}

void pd_manager_source_change_debunce_process(void)
{

    if((wlt_pd_manager->source_change_debunce_count))
    {
        SYS_LOG_INF("[%d] count:%d\n", __LINE__, wlt_pd_manager->source_change_debunce_count);

        if(--wlt_pd_manager->source_change_debunce_count == 0x00)
        {
            if((wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE) || run_mode_is_demo())
            {
                if(!dc_power_in_status_read())                                   // totti chanege: sys_pm_get_power_5v_status
                {
                    wlt_pd_manager->source_change_debunce_count = 0x00;
                    SYS_LOG_INF("DC OUT!!! \n");
                    // pd_manager_deinit();
                    // sys_pm_poweroff();   
                    struct app_msg msg = {0};
    
                    msg.type = MSG_HOTPLUG_EVENT;
		            msg.cmd = 6;
		            msg.value = 2;
                    send_async_msg(APP_ID_MAIN, &msg);

                }
            }
        }
    }

}

bool pd_manager_get_source_change_state(void)
{

    return (wlt_pd_manager->source_change_debunce_count ? 1 : 0);

    // if(wlt_pd_manager->pd_source_status_flag)
    // {
    //     return (wlt_pd_manager->source_change_debunce_count ? 1 : 0);
    // }else{
    //     return 0;
    // }
}

void pd_manager_over_temp_protect(void)
{
    // static bool pd_otp_flag = 0;
    static bool temp_ten_flag = 0;
    static int debounce_count=0;

    // if(!wlt_pd_manager->pd_sink_status_flag)
    // {   
    //     return;
    // }

    if(wlt_pd_manager->pd_sink_dischg_flag)
    {
        if((power_manager_get_battery_temperature() <= 430) && (power_manager_get_battery_temperature() >= 20))
        {    
            if(debounce_count++ == 3){

               // wlt_pd_manager->pd_sink_dischg_flag = false;
                debounce_count = 0;
                
                pd_manager_disable_charging(false);
            }

        }else{
            debounce_count = 0;
        }

    }else{
    
        if((power_manager_get_battery_temperature() >= 440) || (power_manager_get_battery_temperature() <= 10))
        {
            if(debounce_count++ == 3)
            {
               // wlt_pd_manager->pd_sink_dischg_flag = true;
                debounce_count = 0;
                pd_manager_disable_charging(true);
            }
        }else{
            debounce_count = 0;
        }
    } 

    if(wlt_pd_manager->dc_sink_charge_count <= PD_CHARGE_TEN_MINITE_COUNT)   
    {
        wlt_pd_manager->dc_sink_charge_count++;
    }

    if(wlt_pd_manager->dc_sink_charge_count == PD_CHARGE_TEN_MINITE_COUNT)
    {
        if((power_manager_get_battery_temperature() <= 100) && (power_manager_get_battery_temperature() >= 10))
        {
            SYS_LOG_INF("[%d] set current 1500ma \n", __LINE__);
            temp_ten_flag = 1;
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_CURRENT_2400MA, 1);
        }else if((power_manager_get_battery_temperature() < 440) && (power_manager_get_battery_temperature() > 100))
        {
            SYS_LOG_INF("[%d] set current 2400ma \n", __LINE__);
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_CURRENT_2400MA, 0);
        }
    }

    if((power_manager_get_battery_temperature() <= 100) && (power_manager_get_battery_temperature() >= 10))
    {
        if(!temp_ten_flag)
        {
            temp_ten_flag = 1;
            SYS_LOG_INF("[%d] set current 1500ma \n", __LINE__);
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_CURRENT_2400MA, 1);
        }

    }else if((power_manager_get_battery_temperature() < 440) && (power_manager_get_battery_temperature() > 100))
    {
        if(temp_ten_flag)
        {
            temp_ten_flag = 0;
            SYS_LOG_INF("[%d] set current 2400ma \n", __LINE__);
            pd_manager_send_cmd_code(PD_SUPPLY_PROP_CURRENT_2400MA, 0);
        }        
    }
  
}

void pd_manager_send_disc(void)
{

    if(wlt_pd_manager == NULL)
        return;

    if(wlt_pd_manager->pd_source_status_flag)
    {
        pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_DISC, 0);
    }

}

void pd_manager_pd_wakeup()
{
     pd_manager_send_cmd_code(PD_SUPPLY_PROP_WAKEUP, 0);
}


void pd_set_app_mode_state(u8_t status)
{
    if(wlt_pd_manager == NULL)
        return;

    wlt_pd_manager->bt_app_mode = status;  

    SYS_LOG_INF("[%d] app_mode:%d\n", __LINE__, status);

    pd_set_source_current(true);
}

void wait_pd_init_finish(void)
{
    u8_t count=0;

    while(1)
    {
        k_sleep(5);
        // SYS_LOG_INF("finish_flag = [%d]\n", wlt_pd_manager->pd_manager_finish_flag);
        if(wlt_pd_manager->pd_manager_finish_flag)
	    {
	        SYS_LOG_INF("finish_flag = [%d], count:%d\n", wlt_pd_manager->pd_manager_finish_flag, count);
	        break;
        }
        if(count++ >= 72000)
        {
          	SYS_LOG_INF("wait PD init finish timer out; count:%d\n", count);
	        break;  
        }
    }
}
uint8_t pd_get_app_mode_state(void)
{
    if(wlt_pd_manager == NULL) 
        return 0;

    return wlt_pd_manager->bt_app_mode;  
}


bool pd_get_plug_present_state(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_PLUG_PRESENT, &val);

    return val.intval;
}


bool pd_get_sink_charging_state(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SINK_HIGH_Z_STATE, &val);

    return val.intval;

}

u32_t pd_get_sink_charge_volt(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SINK_VOLT_VAULE, &val);

    return val.intval;

}

u32_t pd_get_sink_charge_current(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SINK_CURRENT_VAULE, &val);

    return val.intval;

}
/* test get sink charge step!!! */
u32_t pd_test_get_sink_charge_step(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_TEST_GET_SINK_CHARGE_STEP, &val);

    return val.intval;

}

u32_t pd_get_source_volt(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SOURCE_VOLT_VAULE, &val);

    return val.intval;

}

u32_t pd_get_source_current(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_SOURCE_CURRENT_VAULE, &val);

    return val.intval;

}

u8_t pd_get_pd_version(void)
{
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return 0;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_PD_VERSION, &val);

    return val.intval;
}

// bool pd_get_unload_state(void)
// {
//     union pd_manager_supply_propval val;

//     if(wlt_pd_manager == NULL)
//         return 0;

//     const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
//     api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_UNLOAD_FINISHED, &val);

//     return val.intval;

// }



bool pd_get_sink_full_state(void)
{
    if(wlt_pd_manager == NULL)
        return 0;

    return (wlt_pd_manager->pd_sink_full_state == 1);  
}

void pd_supply_report(pd_manager_charge_event_t event, pd_manager_charge_event_para_t *para)
{
    struct app_msg msg = {0};


    if (wlt_pd_manager == NULL) {
        SYS_LOG_INF("[%d] wlt_pd_manager not init\n", __LINE__);
		return;
	}

    SYS_LOG_INF("[%d] event=%d, para = %d\n", __LINE__, event, para->pd_event_val);

    switch(event)
    {
        case PD_EVENT_INIT_OK:
           // wlt_pd_manager->pd_init_ok = 1;    
            break;

        case PD_EVENT_SOURCE_STATUS_CHG:
            wlt_pd_manager->pd_source_status_flag = false;
            wlt_pd_manager->pd_sink_status_flag = false; 
            wlt_pd_manager->pd_sink_source_chg_flag = true;
            wlt_pd_manager->pd_sink_full_state = false;
            if(para->pd_event_val != 0)
            {
                wlt_pd_manager->pd_source_status_flag = true;
            }
            wlt_pd_manager->pd_manager_low_cur_flag = false;
            break;

        case PD_EVENT_SINK_STATUS_CHG:
            wlt_pd_manager->pd_sink_source_chg_flag = true;
            wlt_pd_manager->pd_source_status_flag = false;
            wlt_pd_manager->pd_sink_status_flag = false;        
            if(para->pd_event_val != 0)
            {
                wlt_pd_manager->pd_sink_status_flag = true;
            }else
            {
               // pd_manager_send_cmd_code(PD_SUPPLY_PROP_CURRENT_2400MA, 2);
            }
            break;

        case PD_EVENT_SOURCE_OTG_CURRENT_LOW:
            if(para->pd_event_val)
            {
                if(wlt_pd_manager->pd_source_status_flag)
                {
                    printk("%s:%d PD_EVENT_SOURCE_OTG_CURRENT_LOW! \n", __func__, __LINE__);

                    if((media_player_is_working() && music_get_tts_count()))
                    {

                        printk("%s:%d Media is playing! \n", __func__, __LINE__);

                    }else if((wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE) || sys_check_standby_state())
                    {
                        printk("%s:%d source current < 80ma, power down! \n", __func__, __LINE__);
                        pd_manager_deinit(0);
    #ifdef OTG_PHONE_POWER_NEED_SHUTDOWN                    
                        mcu_ui_set_real_shutdown_dut(true);
    #endif                    
                        sys_pm_poweroff();       
                    }

                }
                wlt_pd_manager->pd_manager_low_cur_flag = true;    
              
            }else{
                wlt_pd_manager->pd_manager_low_cur_flag = false;  
            }
            
            break;

        case PD_EVENT_SOURCE_LEGACY_DET:

            if((wlt_pd_manager->bt_app_mode == BTMODE_APP_MODE) && wlt_pd_manager->pd_source_status_flag)
            {
                if(run_mode_is_demo())
                {
                    wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
                }
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA, 0);
            }
            break;    

        case PD_EVENT_SINK_FULL:
            if(para->pd_event_val)
            {
                pd_manager_v_sys_g1_g2(PD_V_BUS_G1_HIGH_G2_LOW);

                if((wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE) || sys_check_standby_state())
                {
                     pd_manager_send_cmd_code(PD_SUPPLY_PROP_IDLE, 0);       
                }
            }

            wlt_pd_manager->pd_sink_full_state = para->pd_event_val; 
            
            // if(wlt_pd_manager->pd_sink_full_state)
            // {
            //     msg.type = MSG_PD_BAT_SINK_FULL;
            //     msg.cmd = 6;
            //     msg.value = para->pd_event_val;
            //     send_async_msg(APP_ID_MAIN, &msg);
            // }

            break;

        case PD_EVENT_OTG_MOBILE_DET:
                
            msg.type = MSG_PD_OTG_MOBILE_EVENT;
            msg.cmd = 6;
            msg.value = 0;
            send_async_msg(APP_ID_MAIN, &msg);
                
            break;    

        default:
            break;
    }
}

static void pd_manager_time_hander(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    static uint8_t one_second_cnt = 0;
     struct device *gpio_dev;
	 
    if (wlt_pd_manager == NULL) {
        SYS_LOG_INF("[%d] wlt_pd_manager not init\n", __LINE__);
		return;
	}
	 gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if(Delay_ON_G1G2_flag)
	{
	   Delay_times ++;
	  if(Delay_times >= 9)
      {
        if(Delay_ON_G1G2_flag == 1)
      	{
      	    SYS_LOG_INF("[%d] G2 ON\n", __LINE__);
           gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G2, 1);
	    }
	  else if(Delay_ON_G1G2_flag == 2)
	  	{
	  	  SYS_LOG_INF("[%d] G1 ON\n", __LINE__);
           gpio_pin_write(gpio_dev, POWER_SUPLAY_CONTORL_PIN_G1, 1);
	    }
	     Delay_ON_G1G2_flag = 0;
	     Delay_times = 0;
	  }
	}
    if(wlt_pd_manager->pd_sink_source_chg_flag)
    {
        SYS_LOG_INF("[%d]\n", __LINE__);
        if(wlt_pd_manager->pd_sink_status_flag)
        {
            if(wlt_pd_manager->bt_app_mode == CHARGING_APP_MODE)
            {
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_VOLT_PROIRITY, 0);
            }else{
                pd_manager_send_cmd_code(PD_SUPPLY_PROP_VOLT_PROIRITY, 1);
            }
            wlt_pd_manager->dc_sink_charge_count = 0;
            
        }else if(wlt_pd_manager->pd_source_status_flag)
        {
            if((wlt_pd_manager->bt_app_mode != CHARGING_APP_MODE) && (!wlt_pd_manager->source_change_debunce_count))
            {
                // pd_set_source_current(true);
            }  
        }else{
            pd_manager_G1_G2_debounce_process(false);
            // pd_manager_disable_charging(false);
        }
        wlt_pd_manager->pd_sink_source_chg_flag = 0;
    }


    if(wlt_pd_manager->pd_sink_status_flag)
    {
        pd_manager_over_temp_protect();
        pd_manager_G1_G2_debounce_process(true);

    }else if(wlt_pd_manager->pd_source_status_flag)
    {          
        if(wlt_pd_manager->bt_app_mode != CHARGING_APP_MODE)
        {
        //    pd_set_source_current(false);
        } 

    }else{
        wlt_pd_manager->dc_sink_charge_count = 0;
       
    }

    pd_manager_source_change_debunce_process();

    if(!run_mode_is_demo())
    {
        pd_manager_battery_low_check_otg(false);
    }
    pd_manager_power_key_time();
    
    if(one_second_cnt++ < 10)
    {
        return;
    }
    one_second_cnt = 0;

    //pd_manger_poweron_filte_battery_led_handle();
    pd_managet_typec_high_temp_protect();
 

#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
    pa_manager_otg_phone_power_handle();
#endif    
    SYS_LOG_INF("[%d] sink charging:%d ,full:%d \n",  __LINE__, pd_get_sink_charging_state(),wlt_pd_manager->pd_sink_full_state);


}

#include <adc.h>
#include <misc/byteorder.h>


struct odm_detect_info {
	struct device *adc_dev;
	struct adc_seq_table seq_tbl;
	struct adc_seq_entry seq_entrys;
	u8_t adcval[4];
};



uint8_t wlt_ReadODM(void)

{
    u32_t value;
    uint8_t hw_info;
#ifndef HW_3NODS_VERSION_3
#define ODM_PIN		44 // high to 3nod ,low to tonly.
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
    gpio_pin_configure(gpio_dev, ODM_PIN, GPIO_DIR_IN | GPIO_POL_NORMAL);
    gpio_pin_read(gpio_dev, ODM_PIN, &value);
	if(value == 1)
    {
        hw_info = HW_3NOD_BOARD;
    }
    else{
        hw_info = HW_TONGLI_BOARD;
    }

    return hw_info;
#else
#define ODM_PIN		WIO0 // <0.5v 3nod, >2.7v tonly, 1.3~1.7 ggec; 
    struct odm_detect_info odm;  
    int adc_val; 
	/* LRADC1: wio0  */
	value = sys_read32(WIO0_CTL);
	value = (value & (~(0x0000000F))) | (1 << 3);
	sys_write32(value, WIO0_CTL);


	odm.adc_dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_NAME);
	if (!odm.adc_dev) {
		SYS_LOG_ERR("cannot found ADC device %s\n", CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_NAME);
		return 0;
	}

	odm.seq_entrys.sampling_delay = 0;
	odm.seq_entrys.buffer = (u8_t *)&odm.adcval;
	odm.seq_entrys.buffer_length = sizeof(odm.adcval);
	odm.seq_entrys.channel_id = ADC_ID_CH1;

	odm.seq_tbl.entries = &odm.seq_entrys;
	odm.seq_tbl.num_entries = 1;

	adc_enable(odm.adc_dev);  
	/* wait 10ms */
	//k_busy_wait(20000);

	adc_read(odm.adc_dev, &odm.seq_tbl);
	adc_val = sys_get_le16(odm.seq_entrys.buffer);

   // printk("\n %s,%d , odm : %x !!!\n",__func__,__LINE__,adc_val);
	
    if(adc_val < 0xde)
    {
        hw_info = HW_3NOD_BOARD;
    }
    else if(adc_val < 0x29a)
    {
        hw_info = HW_GUOGUANG_BOARD;
    }
    else{
        hw_info = HW_TONGLI_BOARD;
    }

    return hw_info;    
    //return 1;
#endif

}


uint8_t ReadODM(void)
{
    static u8_t odm_value = 0x00;
    static u8_t odm_first_read = 0;

    if(odm_first_read)
    {
        return odm_value;
    }

    odm_first_read = true;
    odm_value = wlt_ReadODM();

    if(odm_value != wlt_ReadODM())
    {
        printf("%s,%d; ODM first read is Error! \n", __func__, __LINE__);
        k_sleep(50);
        odm_value = wlt_ReadODM();
    }

    if(odm_value == HW_3NOD_BOARD)
    {
        printf("%s,%d; ODM is WorldElite \n", __func__, __LINE__);
    }else if(odm_value == HW_GUOGUANG_BOARD)
    {
        printf("%s,%d; ODM is GuoGuang \n", __func__, __LINE__);
    }else{
        printf("%s,%d; ODM is Tonly \n", __func__, __LINE__);
    }
    return odm_value;
    
}

void pd_init_and_disable_charge(bool flag)
{

    if(!ReadODM()){

        pd_tps65992_init_and_disable_charge(flag);

    }
    else{

    }
}

bool pd_manager_check_mobile(void)
{
    
    union pd_manager_supply_propval val;

    if(wlt_pd_manager == NULL)
        return -1;

    const struct pd_manager_supply_driver_api *api = wlt_pd_manager->dev->driver_api;
    api->get_property(wlt_pd_manager->dev, PD_SUPPLY_PROP_OTG_MOBILE, &val);

    return val.intval;

}

void pd_manager_init(bool flag)
{
    if(wlt_pd_manager != NULL)
    {
        if(!flag)
        {
            wlt_pd_manager->bt_app_mode = BTMODE_APP_MODE; 
            SYS_LOG_INF("[%d] app_mode:%d \n", __LINE__, wlt_pd_manager->bt_app_mode);
        }
        SYS_LOG_INF("[%d] had inited yet \n", __LINE__);
        return;
    }

    struct device *mcu_dev = NULL;
    const struct mcu_manager_supply_driver_api *mcu_api = NULL;
    wlt_pd_manager = &global_pd_manager;
	memset(wlt_pd_manager, 0, sizeof(struct wlt_pd_manager_info));
 
    if(flag)
    {
        wlt_pd_manager->bt_app_mode = CHARGING_APP_MODE;  
    }

//extern void pd_tps65992_poweron(void);  
//   pd_tps65992_poweron();
//   battery_read_iic_value();
//   battery_power_supply_enable(); 

//#ifdef CONFIG_C_LOGIC_MCU_MSPM0L
    const struct pd_manager_supply_driver_api *api= NULL;
    pd_iic_queue_init(&pd_circles_queue);
    battery_iic_init();

if(!ReadODM())

    {
        pd_ti65992_init();   
        wlt_pd_manager->dev = wlt_device_get_pd_ti65992();
        api = wlt_pd_manager->dev->driver_api;
        api->register_pd_notify(wlt_pd_manager->dev, pd_supply_report);

    extern int mcu_mspm0l_init(void);
        mcu_mspm0l_init();
        mcu_dev = wlt_device_get_mcu_mspm0l();
        mcu_api = mcu_dev->driver_api;
        mcu_api->register_mcu_notify(mcu_dev, mcu_supply_report);
        wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
        wlt_pd_manager->otg_off_flag = true;
    }
    //#elif CONFIG_C_LOGIC_MCU_LS8A10049T
    else
    {
        extern int logic_mcu_LS8A10049T_init(void);  
        logic_mcu_LS8A10049T_init();	
        mcu_dev = wlt_device_get_mcu_ls8a10049t();
        mcu_api = mcu_dev->driver_api;
        mcu_api->register_mcu_notify(mcu_dev, mcu_supply_report);    
        extern void logic_mcu_ls8a10049t_int_first_handle(void);
        logic_mcu_ls8a10049t_int_first_handle();

        pd_mps52002_init(); 
        wlt_pd_manager->dev = wlt_device_get_pd_mps52002();
        api = wlt_pd_manager->dev->driver_api;
        api->register_pd_notify(wlt_pd_manager->dev, pd_supply_report);
        wlt_pd_manager->source_change_debunce_count = MAX_SOURCE_CHANGE_TIME;
        k_sleep(50);
		// pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA, 0);
    }
//#endif   
    api->register_bat_notify(wlt_pd_manager->dev, battery_read_iic_value);
    api->register_mcu_notify(wlt_pd_manager->dev, pd_manager_mcu_int_deal);
	//pd_manager_send_cmd_code(PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA, 0);

#ifdef CONFIG_BATTERY_SECURITY
    // extern unsigned char is_authenticated(void);
    //  if(!is_authenticated())
    //  {
    //      wlt_pd_manager->battery_security_flag = true;
    //  }
   	   
#endif
    thread_timer_init(&wlt_pd_manager->timer, pd_manager_time_hander, NULL);
    
    battery_power_supply_enable(); 
    //wlt_led_timer_init();
    extern bool  wlt_mcu_ui_process_init(void);
    wlt_mcu_ui_process_init();


#ifdef CONFIG_POWER_MANAGER
	//power_manager_init();
    power_manager_early_init();
#endif  

    api->enable(wlt_pd_manager->dev);
    thread_timer_start(&wlt_pd_manager->timer, 0, PD_RUN_TIME_PERIOD);

	wlt_pd_manager->pd_manager_finish_flag = 1;
}

int pd_manager_get_source_status(void)
{
    if(wlt_pd_manager == NULL)
    {
        return 0 ;
    }

    if(wlt_pd_manager->pd_source_status_flag)
    {
       if(wlt_pd_manager->pd_manager_low_cur_flag) 
       {
            return 2;
       }else{
            return 1;
       }
    }else{
        return 0;
    }

//    return wlt_pd_manager->pd_source_status_flag && !wlt_pd_manager->pd_manager_low_cur_flag;
    
}

int pd_manager_get_sink_status_flag(void)
{
    if(wlt_pd_manager == NULL)
    {
        return 0 ;
    }

    return wlt_pd_manager->pd_sink_status_flag;
    
}

extern void external_dsp_ats3615_deinit(void);

int pd_manager_deinit(int value)
{

    SYS_LOG_INF("[%d]", __LINE__);

    if(wlt_pd_manager == NULL)
    {
        return 0;
    }

    wlt_pd_manager->source_change_debunce_count = 0x00;

    thread_timer_stop(&wlt_pd_manager->timer);
    pd_manager_send_cmd_code(PD_SUPPLY_PROP_STANDBY, 0);                // stop pd timer;
    k_sleep(50);

    pd_manager_v_sys_g1_g2(PD_V_BUS_G1_G2_DEINIT);
    pd_manager_send_cmd_code(PD_SUPPLY_PROP_POWERDOWN, value);
    k_sleep(120);
    bt_mcu_send_pw_cmd_powerdown();

    return 0;
}

void  sys_3v3_power_on(void)
{
    struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(gpio_dev, 16, GPIO_DIR_OUT);
	gpio_pin_write(gpio_dev, 16, 1);

}


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

int pd_iic_queue_init(PD_CIRCLES_QUEUE_TYPE *Q)
{
	Q->front = 0;
    Q->rear = 0;
    os_mutex_init(&pd_iic_data_lock);
	return 0;
}


int pd_iic_data_isfull(PD_CIRCLES_QUEUE_TYPE *Q)
{
    return (Q->rear+1)%MAXSIZE == Q->front ? 1 : 0;
}

int pd_iic_queue_isempty(PD_CIRCLES_QUEUE_TYPE *Q)
{
     return (Q->rear == Q->front) ? 1 : 0;
}


int pd_iic_push_queue(pd_manager_iic_send_para_t type, u8_t data)
{
    os_mutex_lock(&pd_iic_data_lock, OS_FOREVER);    
    if(pd_iic_data_isfull(&pd_circles_queue))
	{
		SYS_LOG_INF("pd_circles_queue is full\n");
        os_mutex_unlock(&pd_iic_data_lock);  
		return 100001;
	}

    SYS_LOG_INF("type:%d,data:%d \n",(u8_t)type, data);
     
    pd_circles_queue.rear = (pd_circles_queue.rear+1) % MAXSIZE;
    pd_circles_queue.data[pd_circles_queue.rear] = ((u8_t)type<<8) | data;

    SYS_LOG_INF("rear:%d,front:%d \n",pd_circles_queue.rear, pd_circles_queue.front);

    os_mutex_unlock(&pd_iic_data_lock);  
	
	return 0;
}


int pd_iid_pop_queue(u8_t *type, u8_t *data)
{

	if(pd_iic_queue_isempty(&pd_circles_queue))
	{
		// printf("pd_circles_queue is empty\n");
		return 100002;
	}
	
   // os_mutex_lock(&pd_iic_data_lock, OS_FOREVER);
    pd_circles_queue.front = (pd_circles_queue.front+1) % MAXSIZE;
	*type = pd_circles_queue.data[pd_circles_queue.front] >> 8; 
    *data = pd_circles_queue.data[pd_circles_queue.front];
   // os_mutex_unlock(&pd_iic_data_lock);  
	return 0;
}

int pd_manager_get_volt_cur_value(int16 *volt_value, int16 *cur_value)
{
    *volt_value = pd_get_sink_charge_volt();
    *cur_value = pd_get_sink_charge_current();
	
	return 0;
}

bool wlt_pd_manager_is_init(void)
{
    if(wlt_pd_manager)
    {
        return true;
    }
    return false;
}

/* for factory test */
int pd_manager_test_set_sink_charge_current(u8_t step)
{
	pd_manager_send_cmd_code(PD_SUPPLY_PROP_TEST_SINK_CHARAGE_CURRENT, step);
	
	return 0;
}
/* for factory test */
int pd_manager_test_get_sink_charge_current(u8_t *sink_charge_step)
{
	*sink_charge_step = (u8_t )pd_test_get_sink_charge_step();
	
	return 0;
}














