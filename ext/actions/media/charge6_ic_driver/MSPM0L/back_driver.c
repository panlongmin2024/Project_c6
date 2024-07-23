#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <init.h>
#include <kernel.h>

#include <thread_timer.h>
#include <i2c.h>

#include <gpio.h>
#include <pinmux.h>
#include <sys_event.h>

#include "power_supply.h"
#include "power_manager.h"

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#include <msg_manager.h>
#include <soc_pm.h>
#include "app/charge_app/charge_app.h"
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

#define I2C_DEV_ADDR        0x48 //TODO
#define I2C_BAT_DEV_ADDR    0x55
#define I2C_PD_DEV_ADDR     0x21
// #define DEF_MCU_WATER_INT_PIN 0


enum bat_iic_address_t{
    BAT_TEMPERATURE_IIC_ADDR = 0x06,
    BAT_CAP_IIC_ADDR = 0x08,
    BAT_REMAIN_CHARGE_TIME_IIC_ADDR = 0x18,
    BAT_REMAIN_CAP_IIC_ADDR = 0x2C,

    
};

enum mcu_int_cmd_type_t{
    MCU_INT_TYPE_DC = 1,
    MCU_INT_TYPE_POWER_KEY,
    MCU_INT_TYPE_NTC,
    MCU_INT_TYPE_WATER,
   
};

enum mcu_int_cmd_power_t{
    MCU_INT_CMD_POWEROFF,
    MCU_INT_CMD_POWERON,
    MCU_INT_CMD_STANDBY,
    MCU_INT_CMD_CHARGING,

};

enum mcu_int_cmd_dc_t{
    MCU_INT_CMD_DC_OUT,
    MCU_INT_CMD_DC_IN,

};

enum mcu_int_cmd_water_t{
    MCU_INT_CMD_WATER_OUT,
    MCU_INT_CMD_WATER_IN,

};

enum mcu_int_iic_address_t{
    BT_MCU_ACK_IIC_ADDR,
    MCU_INT_CMD_IIC_ADDR,
    BT_MCU_BAT_LED0_IIC_ADDR = 0x10,
    BT_MCU_BAT_LED1_IIC_ADDR,
    BT_MCU_BAT_LED2_IIC_ADDR,
    BT_MCU_BAT_LED3_IIC_ADDR,
    BT_MCU_BAT_LED4_IIC_ADDR,
    BT_MCU_BAT_LED5_IIC_ADDR,
    BT_MCU_TWS_LED_IIC_ADDR = 0x16,
    BT_MCU_PAIR_LED_IIC_ADDR = 0x17,
    
};

enum ti_pd_reg_address_t{

    PD_REG_IIC_CMD_ADDR         = 0x08,
    PD_REG_IIC_DATA_ADDR        = 0x09,
    PD_REG_INT_EVENT_ADDR       = 0x14,
    PD_REG_STATUS_ADDR          = 0x1A,

    PD_REG_REV_SOURCE_ADDR      = 0x30,
    PD_REG_TRANS_SOURCE_ADDR    = 0x32,
    PD_REG_TRANS_SINK_ADDR      = 0x33,
    
};


enum bt_mcu_pair_led_cmd_t{
    BT_LED_STATUS_OFF,
    BT_LED_STATUS_ON,
    BT_LED_STATUS_VERY_SLOW_FLASH,
    BT_LED_STATUS_SLOW_FLASH,
    BT_LED_STATUS_FLASH,
    BT_LED_STATUS_QUICK_FLASH

};

#define PD_STATUS_REG_SINK_SOURCE_SHIFT_BIT     5
#define PD_STATUS_REG_SINK_SOURCE_MASK          0x01


#define PD_STATUS_REG_SINK_LEGACY_SHIFT_BIT            24
#define PD_STATUS_REG_SINK_LEGACY_MASK                 0x03


enum bt_runtime_status_map_t{
    BT_RUNTIME_STATUS_MUSIC_PLAY,
    BT_RUNTIME_STATUS_NORMAL,
    BT_RUNTIME_STATUS_POWEROFF,

};

enum pd_status_sink_legacy_map_t{
    PD_SINK_STATUS_PD,
    PD_SINK_STATUS_LEGACY,
};

enum pd_sink_source_map_t{
    PD_STATUS_SINK_IN,
    PD_STATUS_SOURCE_OUT,
};

typedef struct {
    /** bt_link_status */
	u8_t bt_link_status;

    /** BT send IIC to MCU led value */
	u8_t led_value;

} bt_mcu_pair_led_map_t;

typedef struct {
    u16_t    value;
    u8_t    priority;
} pd_ldo_sink_priority_map_t;


typedef struct {
    /** pd register address */
	u8_t addr;

    /** pd register value length */
	u8_t lenght;

} pd_reg_addr_len_map_t;

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0        5
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1        15
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2        30
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3        45
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4        60
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5        75
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL6        100

#define BATTERY_CHARGING_REMAIN_CAP_LEVEL0        15
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL1        23
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL2        45
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL3        60
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL4        75
#define BATTERY_CHARGING_REMAIN_CAP_LEVEL5        100

#define KELVIN_TEMPERATURE_BASE_VAL               2731
typedef struct {
    /** battery status */
	//uint8_t battery_status;
    /** battery remain cap */
	u16_t battery_remain_cap;

    int  battery_temperature;
    u16_t battery_remain_charge_time;

} battery_manage_info_t;
#endif

typedef struct {
    /** pd_link_status */
	bool    dc_detect_in_flag;
    /** _PD status */
	u16_t   dc_sink_charge_count;
    u32_t   pd_sink_source_flag:1;
    u32_t   pd_sink_legacy_flag:3;
    u32_t   app_music_flag:1;
    u32_t   otg_flag:1;

     /** _app music current status */
    u8_t    bt_app_mode;
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
    battery_manage_info_t global_bat_namager;
#endif    

} pd_manage_info_t;


#define MCU_RUN_TIME_PERIOD 100

#define MCU_ONE_SECOND_PERIOD           ((1000/MCU_RUN_TIME_PERIOD))
#define MCU_CHARGE_TEN_MINITE_COUNT     600
#define MCU_CHARGE_30_SECOND_COUNT      30


static const  bt_mcu_pair_led_map_t bt_mcu_pair_ledmap[] = {

    {SYS_EVENT_BT_UNLINKED,         BT_LED_STATUS_OFF },        
    {SYS_EVENT_BT_WAIT_PAIR,        BT_LED_STATUS_SLOW_FLASH},
    {SYS_EVENT_ENTER_PAIR_MODE,     BT_LED_STATUS_QUICK_FLASH},
    {SYS_EVENT_BT_CONNECTED,        BT_LED_STATUS_ON},
    {SYS_EVENT_TWS_CONNECTED,       BT_LED_STATUS_ON},
    {SYS_EVENT_TWS_START_PAIR,      BT_LED_STATUS_FLASH},
};

static const pd_reg_addr_len_map_t  pd_reg_address_len_map[] = {
    {PD_REG_STATUS_ADDR,            5 }, 
    {PD_REG_IIC_CMD_ADDR,           4 }, 
    {PD_REG_IIC_DATA_ADDR,          4 }, 
    {PD_REG_INT_EVENT_ADDR,         4 },
    {PD_REG_REV_SOURCE_ADDR,        0xFF},
    {PD_REG_TRANS_SINK_ADDR,        0xFF},

};


// 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v;
// 12V-->15V-->9V--->20V-->5V

static const  pd_ldo_sink_priority_map_t pd_ldo_sink_prior_map[] = {

    {0x190,             1 },
    {0xb4,              2 }, 
    {0x12c,             3 }, 
    {0xf0,              4 },
};


#define GPIO_DEV_NAME                       CONFIG_GPIO_ACTS_DEV_NAME
#define GPIO_PIN_WATER_IN                   23  /* DIO8 */
#define GPIO_PIN_USB_D_LINE                 42
#define GPIO_PIN_PD_SOURCE                  4


static struct thread_timer      mcu_timer;
static pd_manage_info_t         global_pd_manager;
static pd_manage_info_t         *pd_manager = NULL;

void pd_tps65992_source_current_proccess(u8_t state);
void logic_mcu_input_event_proc(void);
int bt_mcu_send_pw_cmd_powerdown(void);

#ifdef DEF_MCU_WATER_INT_PIN

static struct device *gpio_dev = NULL;

static K_SEM_DEFINE(sem_request, 0, 1);
static struct k_thread my_thread_data_trace;
K_THREAD_STACK_DEFINE(my_stack_area_trace, 1024);
static struct gpio_callback gpio_cb_water_int;
void logic_mcu_input_event_proc(void);
/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/



static void callback(struct device *dev,
		     struct gpio_callback *gpio_cb, u32_t pins)
{

    printf("PIN_0x%x callback triggered: %d\n", pins, __LINE__);
    gpio_pin_disable_callback(dev, GPIO_PIN_WATER_IN);
    gpio_remove_callback(dev,gpio_cb);
    k_sem_give(&sem_request);

}


static int water_gpio_init(u32_t pin)
{


	gpio_dev = device_get_binding(GPIO_DEV_NAME);

    printk("Totti debug:%s:%d\n", __func__, __LINE__);

	if (!gpio_dev) {
		printk("Cannot get GPIO device\n");
		return false;
	}

	/* 2. Configure PIN_IN and set callback */
	if (gpio_pin_configure(gpio_dev, pin,
				GPIO_DIR_IN | GPIO_INT | GPIO_INT_DEBOUNCE |
				GPIO_INT_EDGE)) {

		printk("PIN_IN configure fail\n");
		return false;
	}

    gpio_cb_water_int.pin_group = pin/32;

	gpio_init_callback(&gpio_cb_water_int, callback, BIT(pin));
	if (gpio_add_callback(gpio_dev, &gpio_cb_water_int)) {
		printk("Set PIN_IN callback fail\n");
		return false;
	}

	gpio_pin_enable_callback(gpio_dev, pin);


    return true;
}

#endif

#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY

#if 0
/*
enum battery_status_info_t{
	BATTERY_STATUS_UNKNOWN = 0,
	BATTERY_STATUS_BAT_NOTEXIST,
	BATTERY_STATUS_DISCHARGE,
	BATTERY_STATUS_CHARGING,
	BATTERY_STATUS_FULL,
    BATTERY_STATUS_EXT_CHARGING,
};
*/
typedef struct {
    /** battery status */
	//uint8_t battery_status;
    /** battery remain cap */
	u16_t battery_remain_cap;

    int  battery_temperature;
    u16_t battery_remain_charge_time;

} battery_manage_info_t;
#endif

#define DISCHARGE_LED_DISPLAY_TIME_PERIOD               10*1000
//static battery_manage_info_t global_bat_namager;
static struct k_timer battery_led_timer;
static bool need_close_battery_led_flag = 0;

static void bt_manager_battery_led_display(uint8_t reg,uint8_t status)
{

    int len = 0;
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    if (iic_dev != NULL)
    {
            buf[0] = BT_LED_STATUS_OFF;
            if(status)
                buf[0] = status;
            i2c_burst_write(iic_dev, I2C_DEV_ADDR, reg, buf, 1);
            printk("zth debug:%s:%d; len= %d; buf: %d \n", __func__,__LINE__, len, buf[0]);
    }else{
        printk("zth debug: IIC device Error!\n");
    }

}

static void battery_discharge_remaincap_low_5(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_15(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_30(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_45(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_60(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_75(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_discharge_remaincap_low_100(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_ON);
}

static void battery_charging_remaincap_low_15(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_charging_remaincap_low_23(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_charging_remaincap_low_45(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_charging_remaincap_low_60(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_charging_remaincap_low_75(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_FLASH);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_charging_remaincap_low_100(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_ON);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_FLASH);
}

static void battery_charging_remaincap_is_full(void)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,BT_LED_STATUS_OFF);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,BT_LED_STATUS_OFF);
}

static void battery_led_timer_fn(struct k_timer *timer)
{
	 printk("ZTH DEBUD: %s:%d\n", __func__, __LINE__);
     //battery_discharge_remaincap_low_5();//死机
     need_close_battery_led_flag = 1;
     k_timer_stop(&battery_led_timer);

}

#if 1
void battery_status_remaincap_display_handle(uint8_t status, u16_t cap,bool key_flag)
{
    printf("zth debug remaincap_display_handle,status ,cap: %d ,0x%x \n", status,cap);
    switch(status)
    {
        case POWER_SUPPLY_STATUS_UNKNOWN:

        break;
        case POWER_SUPPLY_STATUS_BAT_NOTEXIST:

        break;
        case POWER_SUPPLY_STATUS_DISCHARGE:
        {
            if(key_flag){
                if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                    battery_discharge_remaincap_low_5();   
                }
                else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){
                    battery_discharge_remaincap_low_15();
                }
                else{
                    if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
                        battery_discharge_remaincap_low_30();
                    }
                    else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
                        battery_discharge_remaincap_low_45();
                    }
                    else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
                        battery_discharge_remaincap_low_60();
                    }
                    else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
                        battery_discharge_remaincap_low_75();
                    }
                    else{
                        battery_discharge_remaincap_low_100();
                    }
                    k_timer_stop(&battery_led_timer);
                    k_timer_start(&battery_led_timer, DISCHARGE_LED_DISPLAY_TIME_PERIOD,DISCHARGE_LED_DISPLAY_TIME_PERIOD);
                } 
            }     
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
            k_timer_stop(&battery_led_timer);
        }
        break;
        case POWER_SUPPLY_STATUS_FULL:
            battery_charging_remaincap_is_full();
        break;  
        case POWER_SUPPLY_STATUS_EXT_CHARGING:
        {
            if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                 battery_discharge_remaincap_low_5();   
            }
            else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){
                battery_discharge_remaincap_low_15();
            }
            else{
                if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
                    battery_discharge_remaincap_low_30();
                }
                else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
                    battery_discharge_remaincap_low_45();
                }
                else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
                    battery_discharge_remaincap_low_60();
                }
                else if(cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
                    battery_discharge_remaincap_low_75();
                }
                else{
                    battery_discharge_remaincap_low_100();
                }
            } 
            k_timer_stop(&battery_led_timer);
        }
        break;                               
        default:

        break;
    }

}
#else
void battery_status_remaincap_display_handle(bool key_flag)
{
    uint8_t status;
    
    status = power_manager_get_charge_status();

    if(sys_pm_get_power_5v_status() == 1)
    {
        status = POWER_SUPPLY_STATUS_EXT_CHARGING;
    }

    printf("zth debug remaincap_display_handle,status ,cap: %d ,0x%x \n", status,pd_manager->global_bat_namager.battery_remain_cap);
    switch(status)
    {
        case POWER_SUPPLY_STATUS_UNKNOWN:

        break;
        case POWER_SUPPLY_STATUS_BAT_NOTEXIST:

        break;
        case POWER_SUPPLY_STATUS_DISCHARGE:
        {
            if(key_flag){
                if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                    battery_discharge_remaincap_low_5();   
                }
                else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){
                    battery_discharge_remaincap_low_15();
                }
                else{
                    if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
                        battery_discharge_remaincap_low_30();
                    }
                    else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
                        battery_discharge_remaincap_low_45();
                    }
                    else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
                        battery_discharge_remaincap_low_60();
                    }
                    else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
                        battery_discharge_remaincap_low_75();
                    }
                    else{
                        battery_discharge_remaincap_low_100();
                    }
                    k_timer_stop(&battery_led_timer);
                    k_timer_start(&battery_led_timer, DISCHARGE_LED_DISPLAY_TIME_PERIOD,DISCHARGE_LED_DISPLAY_TIME_PERIOD);
                } 
            }     
        }
        break;


        case POWER_SUPPLY_STATUS_CHARGING:
        {
            if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL0){
                battery_charging_remaincap_low_15();
            }
            else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL1){
                battery_charging_remaincap_low_23();
            }
            else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL2){
                battery_charging_remaincap_low_45();
            }
            else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL3){
                battery_charging_remaincap_low_60();
            }
            else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_CHARGING_REMAIN_CAP_LEVEL4){
                battery_charging_remaincap_low_75();
            }
            else{
                battery_charging_remaincap_low_100();
            }
            k_timer_stop(&battery_led_timer);
        }
        break;
        case POWER_SUPPLY_STATUS_FULL:
            battery_charging_remaincap_is_full();
        break;  
        case POWER_SUPPLY_STATUS_EXT_CHARGING:
        {
            if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0){
                 battery_discharge_remaincap_low_5();   
            }
            else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1){
                battery_discharge_remaincap_low_15();
            }
            else{
                if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2){
                    battery_discharge_remaincap_low_30();
                }
                else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3){
                    battery_discharge_remaincap_low_45();
                }
                else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4){
                    battery_discharge_remaincap_low_60();
                }
                else if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5){
                    battery_discharge_remaincap_low_75();
                }
                else{
                    battery_discharge_remaincap_low_100();
                }
            } 
            k_timer_stop(&battery_led_timer);
        }
        break;                               
        default:

        break;
    }
}
#endif //#if 1
#endif
void bt_ui_charging_warning_handle(uint8_t status)
{
    bt_manager_battery_led_display(BT_MCU_BAT_LED0_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_BAT_LED1_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_BAT_LED2_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_BAT_LED3_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_BAT_LED4_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_BAT_LED5_IIC_ADDR,status);   
    bt_manager_battery_led_display(BT_MCU_TWS_LED_IIC_ADDR,status);
    bt_manager_battery_led_display(BT_MCU_PAIR_LED_IIC_ADDR,status);     
}

bool pd_source_pin_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
                                                 
	u32_t val=0;
 
    if(gpio_dev != NULL)
    {
        gpio_pin_read(gpio_dev, GPIO_PIN_PD_SOURCE, &val);
    }
	return (val == 1);
}


void pd_set_app_mode_state(u8_t status)
{
    if(!pd_manager)
        return;
    pd_manager->bt_app_mode = status;  

    SYS_LOG_INF("[%d] app mode:%d\n", status);

    // if(!pd_source_pin_read())
    //     return;

    if(!pd_manager->bt_app_mode)
    {
        pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);
    }else{
        pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);
    }

}

bool mcu_read_dc_status(void)
{
    return pd_manager-> dc_detect_in_flag;
}


static void battery_read_iic_value(void)
{
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_CAP_IIC_ADDR, buf, 2);
    //printf("Totti debug BAT value: 0x%x 0x%x\n", buf[1], buf[0]);

    power_manager_set_battery_vol(buf[1]<<8 | buf[0]);


#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY        
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_REMAIN_CAP_IIC_ADDR, buf, 2);
    //printf("zth debug BAT cap: 0x%x 0x%x\n", buf[1], buf[0]);   
    pd_manager->global_bat_namager.battery_remain_cap = (u16_t)(buf[1]<<8 | buf[0]);

    power_manager_set_battery_cap(pd_manager->global_bat_namager.battery_remain_cap);
    //printf("zth debug battery_remain_cap: 0x%x \n", global_bat_namager.battery_remain_cap);
    
    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_TEMPERATURE_IIC_ADDR, buf, 2);
    //printf("zth debug BAT temp: 0x%x 0x%x\n", buf[1], buf[0]);  
    pd_manager->global_bat_namager.battery_temperature = ((int)(buf[1]<<8 | buf[0])) - KELVIN_TEMPERATURE_BASE_VAL;
    power_manager_set_battery_temperature(pd_manager->global_bat_namager.battery_temperature);
    //printf("zth debug battery_temperature: 0x%d \n", pd_manager->global_bat_namager.battery_temperature);
    // i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, 0x0a, buf, 2);
    //printf("zth debug BAT status: 0x%x 0x%x\n", buf[1], buf[0]);   

    i2c_burst_read(iic_dev, I2C_BAT_DEV_ADDR, BAT_REMAIN_CHARGE_TIME_IIC_ADDR, buf, 2);
    //printf("zth debug BAT time: 0x%x 0x%x\n", buf[1], buf[0]); 
    pd_manager->global_bat_namager.battery_remain_charge_time = (u16_t)(buf[1]<<8 | buf[0]);  
    power_manager_set_battery_remain_charge_time(pd_manager->global_bat_namager.battery_remain_charge_time);
      
#endif    
}



#ifdef DEF_MCU_WATER_INT_PIN
static void  mcu_msg_proceess_thread(void *p1, void *p2, void *p3)
{

    // u8_t buf[4] = {0};
    // union dev_config config = {0};
    // struct device *iic_dev;

    while(1)
    { 
        k_sem_take(&sem_request, K_FOREVER);

         
        printk("Totti debug: %s:%d ok\n", __func__, __LINE__);
        // gpio_pin_enable_callback(gpio_dev, GPIO_PIN_WATER_IN);
        logic_mcu_input_event_proc();
        water_gpio_init(GPIO_PIN_WATER_IN);
            // iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
            // config.bits.speed = I2C_SPEED_STANDARD;
            // i2c_configure(iic_dev, config.raw);
            // buf[0] = (MCU_INT_TYPE_POWER_KEY <<4 | MCU_INT_CMD_POWEROFF);
            // i2c_burst_write(iic_dev, I2C_DEV_ADDR, BT_MCU_ACK_IIC_ADDR, buf, 1);
        
    }
}
#endif

void bt_manager_sys_event_led_display(int link_status)
{
    int len = 0;
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;


    len = sizeof(bt_mcu_pair_ledmap) / sizeof(bt_mcu_pair_led_map_t); 

    printk("Totti debug:%s:%d; len= %d; %d \n", __func__,__LINE__, len, link_status);

    for(int i=0; i< len; i ++)
    {
        if(link_status == bt_mcu_pair_ledmap[i].bt_link_status)
        {
            iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
            config.bits.speed = I2C_SPEED_STANDARD;
            i2c_configure(iic_dev, config.raw);
            if (iic_dev != NULL)
            {
                 buf[0] = bt_mcu_pair_ledmap[i].led_value;
                 i2c_burst_write(iic_dev, I2C_DEV_ADDR, BT_MCU_PAIR_LED_IIC_ADDR, buf, 1);
            }else{
                printk("Totti debug: IIC device Error!\n");
            }
            break;
        }
    }
}

void bt_manager_auracast_led_display(bool status)
{

    int len = 0;
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    
    if (iic_dev != NULL)
    {
            buf[0] = BT_LED_STATUS_OFF;
            if(status)
                buf[0] = BT_LED_STATUS_ON;
            i2c_burst_write(iic_dev, I2C_DEV_ADDR, BT_MCU_TWS_LED_IIC_ADDR, buf, 1);
            printk("Totti debug:%s:%d; len= %d; buf: %d \n", __func__,__LINE__, len, buf[0]);
    }else{
        printk("Totti debug: IIC device Error!\n");
    }

}

void bt_is_off_notify_mcu_to_poweroff(void)
{
    SYS_LOG_INF("[%d] \n", __LINE__);
    if(!sys_pm_get_power_5v_status())
    {
        bt_mcu_send_pw_cmd_powerdown();
    }

    bt_manager_sys_event_led_display(SYS_EVENT_BT_UNLINKED);
    // pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);

}


static  void battery_power_supply_enable(void)
{
	struct device *bat_dev;
    bat_dev = (struct device *)device_get_binding("battery");
    k_sleep(100);
	const struct power_supply_driver_api *api = bat_dev->driver_api;
	api->enable(bat_dev);
}


/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/

static void pd_tps65992_status_value(void)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x0F, buf, 5);
    printf("Totti debug PD version:0x%x 0x%x 0x%x 0x%x 0x%x\n",buf[0], buf[1], buf[2], buf[3], buf[4]);

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x40, buf, 5);
    printf("Totti debug PD status:0x%x 0x%x 0x%x 0x%x 0x%x\n",buf[0], buf[1], buf[2], buf[3], buf[4]);

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x3f, buf, 3);
    printf("Totti debug Power status:0x%x 0x%x 0x%x \n",buf[0], buf[1], buf[2]);

}

static int pd_tps65992_write_4cc(struct device * dev, char *str)
{
    u8_t buf[10] = {0};
    u8_t num_bytes;
    num_bytes = 4;


    printf("Totti debug write 4cc: %s\n", str);

    buf[0] = num_bytes;
    memcpy(&buf[1], str, num_bytes);
    i2c_burst_write(dev, I2C_PD_DEV_ADDR, PD_REG_IIC_CMD_ADDR, buf, num_bytes+1);

    i2c_burst_read(dev, I2C_PD_DEV_ADDR, PD_REG_IIC_CMD_ADDR, buf, num_bytes+1);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n",PD_REG_IIC_CMD_ADDR, buf[0], buf[1], buf[2], buf[3], buf[4]);

    return 0;

}

int bt_mcu_send_cmd_code(u8_t addr, u8_t cmd, u8_t value)
{
    u8_t buf[4] = {0}; 
    union dev_config config = {0};
    struct device *iic_dev;

    SYS_LOG_INF("[%d],addr=%d,cmd=%d,value=%d\n", __LINE__, addr, cmd, value);
    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    if(iic_dev != NULL)
    {
        config.bits.speed = I2C_SPEED_STANDARD;
        i2c_configure(iic_dev, config.raw);

        buf[0] = (cmd <<4 | value);
        i2c_burst_write(iic_dev, I2C_DEV_ADDR, addr, buf, 1);
        return 0;
    }
    return -1;

}

int bt_mcu_send_pw_cmd_standby(void)
{
    return bt_mcu_send_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_STANDBY);
}

int bt_mcu_send_pw_cmd_powerdown(void)
{
	printf("------> send poweroff %d\n",__LINE__);
    return bt_mcu_send_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_POWEROFF);
}

int bt_mcu_send_pw_cmd_poweron(void)
{
    return bt_mcu_send_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_POWERON);
}

int bt_mcu_send_pw_cmd_charging(void)
{
    return bt_mcu_send_cmd_code(BT_MCU_ACK_IIC_ADDR, MCU_INT_TYPE_POWER_KEY, MCU_INT_CMD_CHARGING);
}


static int pd_tps65992_read_reg_value(uint8_t addr, u8_t *buf)
{
    union dev_config config = {0};
    struct device *iic_dev;
    u8_t  num_bytes = 0;
    u8_t i=0;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    for(i=0; i<(sizeof(pd_reg_address_len_map) / sizeof(pd_reg_addr_len_map_t)); i++)
    {
        // printf("[%s %d]  pd address: 0x%x 0x%x\n", __func__, __LINE__, addr, pd_reg_address_len_map[i].addr);
        if(addr == pd_reg_address_len_map[i].addr)
        {
            num_bytes = pd_reg_address_len_map[i].lenght;
            break;
        }
    }

    if(i >= (sizeof(pd_reg_address_len_map) / sizeof(pd_reg_addr_len_map_t)))
    {
        printf("[%s %d] dont find the pd address: 0x%x\n", __func__, __LINE__, addr);
        return 0;
    }

    if(num_bytes != 0xFF)
    {
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, num_bytes);
    }else{

        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, 2);
        num_bytes = buf[0];
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, addr, buf, num_bytes);
    }

    
    printf("Totti debug reg 0x%x value:  ", addr);
    for(int i=0; i< num_bytes; i++)
    {
        printf("0x%x, ", buf[i]);
    }
    printf("\n");

    return num_bytes;

}

static bool pd_tps65992_get_status_reg()
{
    u8_t    buf[10] = {0};
    u32_t   reg_value = 0;
    u8_t    ret = false;
    u8_t    result = 0;
    static  u8_t source_debounce_cnt = 0;

    if(pd_tps65992_read_reg_value(PD_REG_STATUS_ADDR, buf)!=0)
    {
        reg_value = (buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];

        result = (reg_value >> PD_STATUS_REG_SINK_SOURCE_SHIFT_BIT) & PD_STATUS_REG_SINK_SOURCE_MASK;
        
        if(result != pd_manager->pd_sink_source_flag)
        {
            if(source_debounce_cnt < 2)
            {
                source_debounce_cnt++;
                
            }else{
                source_debounce_cnt = 0;
                pd_manager->pd_sink_source_flag = result;
                SYS_LOG_INF("[%d] source flag:%d", __LINE__,pd_manager->pd_sink_source_flag);
                ret = true;
            }

        }else{
            source_debounce_cnt = 0;
        }

        result = (reg_value >> PD_STATUS_REG_SINK_LEGACY_SHIFT_BIT) & PD_STATUS_REG_SINK_LEGACY_MASK; 
        if(result != pd_manager->pd_sink_legacy_flag)
        {
            pd_manager->pd_sink_legacy_flag = result;
            ret = true;
        }
    }

    return ret;

}

static void pd_legacy_sink_current_proccess(void)
{

    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x06;
    buf[1] = 0x6b;
    buf[2] = 0x03;
    buf[3] = 0x06;                                              // IINDPM
    buf[4] = 0x01;                                              // 0x012c = 3A current;
    buf[5] = 0x2c;

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 2);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x\n", 0x09  ,buf[0], buf[1]);

    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x06;                                              // IINDPM
    buf[3] = 0x02;                                              
  
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
    pd_tps65992_write_4cc(iic_dev, "I2Cr");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

}


static void pd_bq25798_adc_off(void)
{

    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x2E;                                              // IINDPM
    buf[4] = 0x30;                                              // 0x012c = 3A current;

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x2E  ,buf[0], buf[1], buf[2], buf[3]);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x11;                                              // IINDPM
    buf[4] = 0x04;                                              // 0x012c = 3A current;

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x11  ,buf[0], buf[1], buf[2], buf[3]);


}



void pd_tps65992_pd_24A_proccess(void)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    buf[0] = 0x06;
    buf[1] = 0x6b;
    buf[2] = 0x03;
    buf[3] = 0x03;                                              // IINDPM
    buf[4] = 0x00;                                              // 0x0F0 = 2.4A current;
    buf[5] = 0xF0;

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

}

void pd_tps65992_pd_otg_on(bool flag)
{
    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;
    u8_t temp_value;
   
    if(pd_manager->otg_flag == flag)
    {
        return;
    }

    if(1)
        return;
    pd_manager->otg_flag = flag;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    if(flag)
    {
        
    }else{
       
        pd_tps65992_write_4cc(iic_dev, "DISC");

        buf[0] = 0x04;
        buf[1] = 0x6b;
        buf[2] = 0x12;
        buf[3] = 0x01;                                             // IINDPM
                                                
        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        pd_tps65992_write_4cc(iic_dev, "I2Cr");

        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
        printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

        temp_value = buf[2] & 0xBF;

        buf[0] = 0x05;
        buf[1] = 0x6b;
        buf[2] = 0x02;
        buf[3] = 0x12;  
        buf[4] = temp_value; 

        i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        pd_tps65992_write_4cc(iic_dev, "I2Cw");
        i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);
        printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

    }


}

int16_t pd_bq25798_read_current()
{

    u8_t buf[10] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);


    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x0d;
    buf[3] = 0x01;                                             // IINDPM
                                                
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    pd_tps65992_write_4cc(iic_dev, "I2Cr");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti od reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x;\n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x35;
    buf[3] = 0x02;                                             // IINDPM
                                                
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    pd_tps65992_write_4cc(iic_dev, "I2Cr");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti volt reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x; %d\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], ((buf[2]<<8) | buf[3]));


    buf[0] = 0x04;
    buf[1] = 0x6b;
    buf[2] = 0x31;
    buf[3] = 0x02;                                             // IINDPM
                                                
    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    pd_tps65992_write_4cc(iic_dev, "I2Cr");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x \n", 0x09  ,buf[0], buf[1], buf[2], buf[3]);

    return ((buf[2]<<8) | buf[3]);

}


void pd_tps65992_source_current_proccess(u8_t state)
{

    u8_t    buf[100] = {0};
    u8_t    charge_current = 0;
        
    printf("[%s %d] status:%d \n", __func__, __LINE__, state);

    memset(buf, 0x00, sizeof(buf));

    switch(state){
        case BT_RUNTIME_STATUS_MUSIC_PLAY:
            buf[0] = 0x35;
            buf[1] = 1;
            buf[2] = 0x02;
            buf[4] = 0x32;                                              // 5v/500mA ,must be write first   
            buf[5] = 0x90;
            buf[6] = 0x01;
            buf[7] = 0x10;
            charge_current = 0x8c;
            break;

        case BT_RUNTIME_STATUS_NORMAL:
            buf[0] = 0x35;
            buf[1] = 1;
            buf[2] = 0x02;
            buf[4] = 0x64;                                              // 5v/500mA ,must be write first   
            buf[5] = 0x90;
            buf[6] = 0x01;
            buf[7] = 0x10;
            charge_current = 0x98;
            break;

        case BT_RUNTIME_STATUS_POWEROFF:       
            buf[0] = 0x35;
            buf[1] = 3;
            buf[2] = 0x2A;
            buf[4] = 0x2c;                                              // 5v/3A ,must be write first   
            buf[5] = 0x91;
            buf[6] = 0x01;
            buf[7] = 0x10;

            buf[8] = 0xc8;                                              // 9v/2A 
            buf[9] = 0xd0;
            buf[10] = 0x02;
            buf[11] = 0x00;

            buf[12] = 0x28;                                              // pps5v~11v 
            buf[13] = 0x32;
            buf[14] = 0xdc;
            buf[15] = 0xc0;
            charge_current = 0xCA;
            break;

        default:
            break;    
    }

    union  dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SOURCE_ADDR, buf, 0x35);	

    
    pd_tps65992_write_4cc(iic_dev, "SSrC"); 
    
    k_sleep(5);

    buf[0] = 0x05;
    buf[1] = 0x6b;
    buf[2] = 0x02;
    buf[3] = 0x0d;                                              // battery
    buf[4] = charge_current;                                    // 0.5 mv; 1A; 3A

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 5);	
    pd_tps65992_write_4cc(iic_dev, "I2Cw");

    i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
    printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]); 
    
}

/********************************************************************************************
 * 0:9     ; max current
 * 10:19   ; voltage
 * 20:21   ; peak currtent
 * 24      ; unchunked extended message support
 * 26      ; usb suspend supported
 * 30:31   ; supply type

 * 0:9     ; max current
 * 10:19   ; voltage
 * 20:21   ; Peak current
 * 22      ; EPR mode capable
 * 30:31   ; supply type
 * 
 * 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v;
 ********************************************************************************************/


static void pd_tps65992_pd_priority_proccess(void)
{

    u8_t    buf[100] = {0};
//    u32_t   reg_value = 0;
    u8_t    i,j;
    u32_t   ldo_value = 0;
    u32_t   ldo_temp_value = 0;
    // check pd state for 500 msecond
    u32_t   ldo_index = 0x00;
    u8_t    ldo_nums = 0;

    for(i=0; i<5; i++)
    {
        if(pd_tps65992_read_reg_value(PD_REG_INT_EVENT_ADDR, buf)!=0)
        {
            if(buf[2] & 0x40)                       // Source Capabalities Message Received 
            {
                break;
            }
        }
        k_sleep(100);
    }
    if(i >= 5){
        printf("[%s:%d] dont detect pd sink\n", __func__, __LINE__);
 //       return;                                     // not pd sink
    }
        
    if(pd_tps65992_read_reg_value(PD_REG_REV_SOURCE_ADDR, buf)==0)
    {
        printf("[%s:%d] read recieve source reg error \n", __func__, __LINE__);
        return;
    }

     ldo_nums = buf[1];
     ldo_temp_value = buf[2] | (buf[3]<<8) | (buf[4]<<16) | (buf[5]<<24);

    if(((ldo_nums== 0)) || (ldo_temp_value==0) )         // ldo number no equal 0
    {
        printf("[%s:%d] detect pd recieve source num is zero\n", __func__, __LINE__);
        return;
    }

    // 12V-->15V-->9V--->20V-->5V
    // 0xb4=9v; 0x64=5v; 0xf0=12v; 0x12c=15v; 0x190=20v; 
    for(i=0; i< ldo_nums; i++)
    {
        ldo_temp_value = buf[(i*4)+2] | (buf[(i*4)+3]<<8) | (buf[(i*4)+4]<<16) | (buf[(i*4)+5]<<24); 
        
        for(j=0; j< sizeof(pd_ldo_sink_prior_map)/sizeof(pd_ldo_sink_priority_map_t); j++)
        {
            if( ((ldo_temp_value >> 10) & 0x3ff) == pd_ldo_sink_prior_map[j].value)
            {
                if(ldo_index < pd_ldo_sink_prior_map[j].priority )
                {
                    ldo_index = pd_ldo_sink_prior_map[j].priority;
                    ldo_value = ldo_temp_value;
                }
            }
        }
    }

    memset(buf, 0x00, sizeof(buf));

    buf[0] = 0x35;
    buf[1] = 1;
    buf[2] = 0x2c;                                              // 5v/3A ,must be write first   
    buf[3] = 0x91;
    buf[4] = 0x01;
    buf[5] = 0x10;

    if(ldo_index)
    {
        buf[1] = 2;
        buf[6] = ldo_value & 0xff;                              // 5v/3A ,must be write first   
        buf[7] = (ldo_value>>8) & 0xff;
        buf[8] = (ldo_value>>16) & 0xff;
        buf[9] = (ldo_value>>24) & 0xff;

    }

    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, PD_REG_TRANS_SINK_ADDR, buf, 0x35);	

    if(pd_tps65992_read_reg_value(PD_REG_TRANS_SINK_ADDR, buf)==0)
    {
        printf("[%s:%d] read recieve source reg error \n", __func__, __LINE__);
        return;
    }

    pd_tps65992_write_4cc(iic_dev, "GSrC");

}


static void pd_tps65992_init(void)
{

    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_FAST;
    i2c_configure(iic_dev, config.raw);

    pd_tps65992_write_4cc(iic_dev, "DBfg");

    pd_tps65992_status_value();

}


// static void pd_read_bq25789_value_test(void)
// {

//     u8_t buf[10] = {0};
//     union dev_config config = {0};
//     struct device *iic_dev;

//     iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
//     config.bits.speed = I2C_SPEED_FAST;
//     i2c_configure(iic_dev, config.raw);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x06;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x06;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x06;
//     buf[4] = 0x00;
//     buf[5] = 0xF0;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 6);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4],  buf[5]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cw");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);


//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x03;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     buf[0] = 0x04;
//     buf[1] = 0x6b;
//     buf[2] = 0x06;
//     buf[3] = 0x02;

//     i2c_burst_write(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
	
//     memset(buf, 0x00, sizeof(buf));
//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

//     pd_tps65992_write_4cc(iic_dev, "I2Cr");

//     i2c_burst_read(iic_dev, I2C_PD_DEV_ADDR, 0x09, buf, 4);
//     printf("Totti debug reg 0x%x value: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", 0x09  ,buf[0], buf[1], buf[2], buf[3], buf[4]);

// }




/****************************************************************************************************************
 * Auther: Totti
 * Data: 2023/12/12
 * 
*****************************************************************************************************************/
extern bool media_player_is_working(void);
extern bool key_water_status_read(void);

static void mcu_pd_iic_time_hander(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    static bool pd_source_pin_last_flag = false;
    static u8_t mcu_one_secound_count = 0;
 
	if(key_water_status_read())
    {
        logic_mcu_input_event_proc();
    }

    if(mcu_one_secound_count++ < MCU_ONE_SECOND_PERIOD)
    {
        return;
    }
    mcu_one_secound_count = 0;
    battery_read_iic_value();
        
    if(pd_manager->dc_detect_in_flag)
    {

        if(pd_manager->dc_sink_charge_count <= MCU_CHARGE_TEN_MINITE_COUNT)
        {
            pd_manager->dc_sink_charge_count++;
        }
        
        if(pd_manager->dc_sink_charge_count < MCU_CHARGE_30_SECOND_COUNT)
        {
            
            if(pd_tps65992_get_status_reg())
            {
                if(pd_manager->pd_sink_legacy_flag == PD_SINK_STATUS_LEGACY)
                {
                    // procces legacy sink;
                    pd_legacy_sink_current_proccess();

                }else{
                    // pd 12-15-20-9-5 priority
                    pd_tps65992_pd_priority_proccess();
                }
            }
            
        }else if(pd_manager->dc_sink_charge_count == MCU_CHARGE_TEN_MINITE_COUNT)
        {
            if(pd_manager->pd_sink_legacy_flag == PD_SINK_STATUS_PD)
            {
                pd_tps65992_pd_24A_proccess();
            }
        }

    }else{
        // if(pd_source_pin_read())
        pd_tps65992_get_status_reg();
        if(pd_manager->pd_sink_source_flag == PD_STATUS_SOURCE_OUT)
        {
            if(!pd_source_pin_last_flag)
            {
                pd_source_pin_last_flag = true;
                // At the situation of music playing, pd current process;  
                if(!pd_manager->bt_app_mode)
                {
                    pd_manager->app_music_flag = media_player_is_working();
                    if(pd_manager->app_music_flag)
                    {
                        pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);
                    }else{
                        pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);       
                    }
                }else{
                    pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_POWEROFF);    
                }

                pd_manager->otg_flag = true;

            }else{
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY     
                if(pd_manager->global_bat_namager.battery_remain_cap < BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1)
                {
                    pd_tps65992_pd_otg_on(false);
                }  
                
                int16_t value = pd_bq25798_read_current();
                value = (value>= 0 ? value : -value);

                SYS_LOG_INF("[%d] current value:%d\n", __LINE__, value);

                if((value!=0) && (value < 50))
                {
                    pd_tps65992_pd_otg_on(false); 
                }

                if((!pd_manager->bt_app_mode) && pd_manager->otg_flag)
                {
                    bool play_flag=false;

                    play_flag = media_player_is_working();

                    SYS_LOG_INF("[%d] %d %d \n", __LINE__, play_flag, pd_manager->app_music_flag);

                    if(play_flag != pd_manager->app_music_flag)
                    {
                        SYS_LOG_INF("[%d] \n", __LINE__);

                        pd_manager->app_music_flag = play_flag;
                        if(pd_manager->app_music_flag)
                        {
                            pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_MUSIC_PLAY);
                        }else{
                            pd_tps65992_source_current_proccess(BT_RUNTIME_STATUS_NORMAL);       
                        }
                    }
                }
#endif
            }
        

        }else{

            pd_tps65992_pd_otg_on(true);
            pd_source_pin_last_flag = false;
        } 
    }
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY  
    if(need_close_battery_led_flag == 1){
        battery_discharge_remaincap_low_5();
        need_close_battery_led_flag = 0;
    }  
#endif      
}


int logic_mcu_mspm0l_init(void)
{
    
    printk("Totti debug: %s:%d ok\n", __func__, __LINE__);

    pd_manager = &global_pd_manager;

	memset(pd_manager, 0, sizeof(pd_manage_info_t));

    thread_timer_init(&mcu_timer, mcu_pd_iic_time_hander, NULL);
    thread_timer_start(&mcu_timer, MCU_RUN_TIME_PERIOD, MCU_RUN_TIME_PERIOD);

    struct device *gpio_dev;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		
	}else{
	    gpio_pin_configure(gpio_dev, GPIO_PIN_USB_D_LINE, GPIO_DIR_OUT | GPIO_POL_NORMAL | GPIO_PUD_PULL_UP);
	    gpio_pin_write(gpio_dev, GPIO_PIN_USB_D_LINE, 1);
        gpio_pin_configure(gpio_dev, GPIO_PIN_PD_SOURCE, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
    }

    pd_tps65992_init();

#ifdef DEF_MCU_WATER_INT_PIN
    water_gpio_init(GPIO_PIN_WATER_IN);
    memset(&my_thread_data_trace, 0, sizeof(struct k_thread));    
    k_thread_create(&my_thread_data_trace, (k_thread_stack_t)my_stack_area_trace,
        sizeof(my_stack_area_trace), (k_thread_entry_t)mcu_msg_proceess_thread,
        NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
#endif    


#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
   // global_bat_namager.battery_status = BATTERY_STATUS_DISCHARGE;
    need_close_battery_led_flag = 0;
    k_timer_init(&battery_led_timer, battery_led_timer_fn, NULL);
#endif
    return 0;
}



void logic_mcu_input_event_proc(void){

    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

    if (iic_dev != NULL)
    {
        i2c_burst_read(iic_dev, I2C_DEV_ADDR, MCU_INT_CMD_IIC_ADDR, buf, 1);
        printf("Totti debug IIC buf: 0x%x\n", buf[0]);

        uint8_t code = buf[0] & 0x0f;

        switch((buf[0]>>4) & 0x0f)
        {
            case MCU_INT_TYPE_POWER_KEY:
            #ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
                power_manager_battery_display_handle(1);
            #endif   
                if(code == MCU_INT_CMD_POWEROFF)
                {
                	printf("------> send poweroff %d\n",__LINE__);
                    if(sys_pm_get_power_5v_status())
                    {
                        printk("POWERDOWN --> charging!!!\n");
                        bt_mcu_send_pw_cmd_charging();
                        charge_app_enter_cmd();
                        bt_manager_sys_event_led_display(SYS_EVENT_BT_UNLINKED);
                        bt_manager_auracast_led_display(BT_LED_STATUS_OFF); 
                    }else{
                        printk("POWERDOWN!!! \n");
                        bt_mcu_send_pw_cmd_powerdown();
                        pd_bq25798_adc_off();
                        sys_event_notify(SYS_EVENT_POWER_OFF);
                    }
                    //sys_event_notify(SYS_EVENT_POWER_OFF);
                }else{
                    if(charge_app_exit_cmd() == 1){
                        struct app_msg msg = {0};
                        msg.type = MSG_EXIT_APP;
                        send_async_msg("main", &msg);
                    }
                    bt_mcu_send_pw_cmd_poweron();

                }
                battery_power_supply_enable();                
                
                break;

            case MCU_INT_TYPE_DC:
                if(code == MCU_INT_CMD_DC_IN)
                {
                    pd_manager->dc_detect_in_flag = true;
                    pd_manager->dc_sink_charge_count = 0x00;
                    pd_manager->pd_sink_legacy_flag = PD_SINK_STATUS_PD;
                    pd_manager->pd_sink_source_flag = PD_STATUS_SOURCE_OUT;
                    buf[0] = (MCU_INT_TYPE_DC <<4 | MCU_INT_CMD_DC_IN);
                }else{
                    pd_manager->dc_detect_in_flag = false;
                    pd_manager->dc_sink_charge_count = 0x00;
                    pd_manager->pd_sink_legacy_flag = PD_SINK_STATUS_PD;
                    pd_manager->pd_sink_source_flag = PD_STATUS_SOURCE_OUT;
                    buf[0] = (MCU_INT_TYPE_DC <<4 | MCU_INT_CMD_DC_OUT);
                }
                i2c_burst_write(iic_dev, I2C_DEV_ADDR, BT_MCU_ACK_IIC_ADDR, buf, 1);
                battery_power_supply_enable();
               
                break;

            case MCU_INT_TYPE_NTC:
                break;

            case MCU_INT_TYPE_WATER:
                if(code == MCU_INT_CMD_WATER_IN)
                {
                    if(sys_pm_get_power_5v_status())
                    {
                        //pd_tps65992_pd_otg_on(false);
                        sys_event_notify(SYS_EVENT_CHARGING_WARNING);
                    }
                        
                }
                else
                {
                    sys_event_notify(SYS_EVENT_REMOVE_CHARGING_WARNING);
                }
                break;


            default:
                break;
        }
    
    }else{
        printk("[%s] I2C device is null\n", __FUNCTION__);
    }

}
