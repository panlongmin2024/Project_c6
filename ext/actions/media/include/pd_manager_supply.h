/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TI PD 65992 supply device driver interface
 */

#ifndef __INCLUDE_PD_MANAGER_SUPPLY_H__
#define __INCLUDE_PD_MANAGER_SUPPLY_H__

#include <stdint.h>
#include <device.h>
#include <wltmcu_manager_supply.h>

/////////////////////////////////////////////////////////////////////

/*
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
*/


enum bat_iic_address_t{
    BAT_TEMPERATURE_IIC_ADDR = 0x06,
    BAT_CAP_IIC_ADDR = 0x08,
    BAT_STATUS_IIC_ADDR = 0x0a,
    BAT_CURRENT_IIC_ADDR = 0x0c,
    BAT_AVERAGE_CURRENT_IIC_ADDR = 0x14,
    BAT_REMAIN_CHARGE_TIME_IIC_ADDR = 0x18,
    BAT_REMAIN_CAP_IIC_ADDR = 0x2C,
    BAT_HEALTH_IIC_ADDR = 0x2E,

};

// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0        5
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1        15
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2        23
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3        45
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4        60
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5        75
// #define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL6        100

#define   HW_3NODS_VERSION_3                    1  

///////////////////////////////////////////////////////////////////


/*!
 * \brief 电池及充电事件定义
 */
typedef enum
{
    PD_EVENT_INIT_OK = 0x01,
    PD_EVENT_SINK_STATUS_CHG=0x20,
    PD_EVENT_SOURCE_STATUS_CHG,
    PD_EVENT_SOURCE_OTG_CURRENT_LOW,
    PD_EVENT_SINK_FULL,
    PD_EVENT_SOURCE_LEGACY_DET,
    PD_EVENT_OTG_MOBILE_DET,



} pd_manager_charge_event_t;

/*
typedef enum
{
    PD_V_BUS_G1_HIGH_G2_LOW,
    PD_V_BUS_G1_LOW_G2_HIGH,
    PD_V_BUS_G1_G2_INIT,
    PD_V_BUS_G1_G2_DEINIT,
} pd_v_bus_g1_g2_t;


typedef enum
{
    MCU_INT_TYPE_DC = 1,
    MCU_INT_TYPE_POWER_KEY,
    MCU_INT_TYPE_NTC,
    MCU_INT_TYPE_WATER,

} mcu_charge_event_t;
	
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

enum bt_mcu_pair_led_cmd_t{
    BT_LED_STATUS_OFF,
    BT_LED_STATUS_ON,
    BT_LED_STATUS_VERY_SLOW_FLASH,
    BT_LED_STATUS_SLOW_FLASH,
    BT_LED_STATUS_FLASH,
    BT_LED_STATUS_QUICK_FLASH

};
*/
	
typedef union {
	uint32_t mcu_event_val;
	uint32_t pd_event_val;

} pd_manager_charge_event_para_t;


union pd_manager_supply_propval {
	int intval;
	const char *strval;
};


typedef void (*pd_manager_callback_t)(pd_manager_charge_event_t event, pd_manager_charge_event_para_t *para);
//typedef void (*mcu_callback_t)(mcu_charge_event_t event, pd_manager_charge_event_para_t *para);
typedef void (*pd_manager_battery_t)(void);
typedef void (*pd_manager_mcu_callback_t)(void);

struct pd_manager_supply_driver_api {
	
    int (*get_property)(struct device *dev, enum pd_manager_supply_property psp,
			     union pd_manager_supply_propval *val);
	void (*set_property)(struct device *dev, enum pd_manager_supply_property psp,
			     union pd_manager_supply_propval *val);
//   void (*mcu_response)(struct device *dev, enum pd_manager_supply_property psp,
//			     union pd_manager_supply_propval *val);

	void (*register_pd_notify)(struct device *dev, pd_manager_callback_t cb);
    void (*register_mcu_notify)(struct device *dev, pd_manager_mcu_callback_t cb);

    void (*register_bat_notify)(struct device *dev, pd_manager_battery_t cb);

	void (*enable)(struct device *dev);
	void (*disable)(struct device *dev);	
};


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
void pd_tps65992_init_and_disable_charge(bool flag);


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
 
#define MAXSIZE 30
 
typedef uint16_t DataType;
 
typedef struct
{
	DataType data[MAXSIZE];
	int front;
	int rear;
}PD_CIRCLES_QUEUE_TYPE;
 
/*循环队列初始化*/
int pd_iic_queue_init(PD_CIRCLES_QUEUE_TYPE *Q);
 
/*入队*/
int pd_iic_push_queue(u8_t type, u8_t data);
 
/*队满？*/
int pd_iic_data_isfull(PD_CIRCLES_QUEUE_TYPE *Q);
 
/*出队*/
int pd_iid_pop_queue(u8_t *type, u8_t *data);
 
/*队空*/
int pd_iic_queue_isempty(PD_CIRCLES_QUEUE_TYPE *Q);



typedef union {
	uint8_t pd_iic_data;
} pd_manager_iic_data_para_t;
 
typedef enum {

    PD_IIC_TYPE_PROP_CURRENT_2400MA = 1,
    PD_IIC_TYPE_PROP_SOURCE_CURRENT_500MA,
    PD_IIC_TYPE_PROP_SOURCE_CURRENT_1000MA,
    PD_IIC_TYPE_PROP_SOURCE_CHARGING_OTG,
    PD_IIC_TYPE_PROP_VOLT_PROIRITY,
    PD_IIC_TYPE_PROP_STANDBY,
    PD_IIC_TYPE_PROP_POWERDOWN,
    PD_IIC_TYPE_PROP_OTG_OFF,
    PD_IIC_TYPE_PROP_SINK_CHANGING_ON, 
    PD_IIC_TYPE_PROP_SINK_CHANGING_OFF,
    PD_IIC_TYPE_PROP_SOURCE_SSRC,
    PD_IIC_TYPE_PROP_ONOFF_AMP_PVDD,
    PD_IIC_TYPE_PROP_ONOFF_BGATE,
    PD_IIC_TYPE_PROP_SOURCE_DISC,
    PD_IIC_TYPE_PROP_ONOFF_G2,
    PD_IIC_TYPE_PROP_IDLE,
    PD_IIC_TYPE_PROP_WAKEUP,

    MCU_IIC_TYPE_PROP_LED_0 = 100,
    MCU_IIC_TYPE_PROP_LED_1,
    MCU_IIC_TYPE_PROP_LED_2,
    MCU_IIC_TYPE_PROP_LED_3,
    MCU_IIC_TYPE_PROP_LED_4,
    MCU_IIC_TYPE_PROP_LED_5,
    MCU_IIC_TYPE_PROP_LED_BT,
    MCU_IIC_TYPE_PROP_LED_AURACAST,

    MCU_IIC_TYPE_PROP_DSP_DCDC,

	PD_IIC_TYPE_PROP_TEST_SINK_CHARGE_CURRENT,//for factory test
} pd_manager_iic_send_para_t;



#endif
