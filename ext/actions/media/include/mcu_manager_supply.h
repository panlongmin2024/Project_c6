/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TI PD 65992 supply device driver interface
 */

#ifndef __INCLUDE_MCU_MANAGER_SUPPLY_H__
#define __INCLUDE_MCU_MANAGER_SUPPLY_H__

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

typedef enum
{
    MCU_INT_TYPE_DC = 1,
    MCU_INT_TYPE_POWER_KEY,
    MCU_INT_TYPE_WATER,    
    MCU_INT_TYPE_NTC,
    MCU_INT_TYPE_HW_RESET,

} mcu_charge_event_t;
	
enum mcu_int_cmd_power_t{
    MCU_INT_CMD_POWEROFF,
    MCU_INT_CMD_POWERON,
    MCU_INT_CMD_STANDBY,
    MCU_INT_CMD_CHARGING,
    MCU_INT_CMD_OTG_MOBILE_ON,

};

enum mcu_int_cmd_dc_t{
    MCU_INT_CMD_DC_OUT,
    MCU_INT_CMD_DC_IN,

};

enum mcu_int_cmd_water_t{
    MCU_INT_CMD_WATER_OUT,
    MCU_INT_CMD_WATER_IN,

};

enum mcu_int_cmd_ntc_t{
    MCU_INT_CMD_TEMPERATURE_NORMAL =0,
    MCU_INT_CMD_TEMPERATURE_LOW,
    MCU_INT_CMD_TEMPERATURE_LOWER,
    MCU_INT_CMD_TEMPERATURE_HIGH,
    MCU_INT_CMD_TEMPERATURE_HIGHER,
    MCU_INT_CMD_TEMPERATURE_VERY_HIGH,

};

typedef enum
{
    MCU_FW_UPDATE_STOP = 0,
    MCU_FW_UPDATE_START,
    MCU_FW_UPDATEING,
    MCU_FW_UPDATE_SUCESS,    
} mcu_fw_upsate_status_t;

/// @brief ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
enum mcu_manager_supply_property {

    MCU_SUPPLY_PROP_LED_0 = 0,
    MCU_SUPPLY_PROP_LED_1,
    MCU_SUPPLY_PROP_LED_2,
    MCU_SUPPLY_PROP_LED_3,
    MCU_SUPPLY_PROP_LED_4,
    MCU_SUPPLY_PROP_LED_5,
    MCU_SUPPLY_PROP_LED_BT,
    MCU_SUPPLY_PROP_LED_PTY_BOOST,
    /// /*上面是灯的序号不能加东西，往下添加*////////////////////////////////
    MCU_SUPPLY_PROP_DSP_DCDC = 0x10,
    MCU_SUPPLY_PROP_UPDATE_FW,
    MCU_SUPPLY_PROP_RESET_MCU,
    MCU_SUPPLY_PROP_ENABLE_LED_BT,
    //MCU_SUPPLY_PROP_DISABLE_LED_BT,
    MCU_SUPPLY_PROP_ENABLE_LED_PTY_BOOST,
    //MCU_SUPPLY_PROP_DISABLE_LED_PTY_BOOST,
    MCU_SUPPLY_PROP_ENABLE_LED_BAT,
    //MCU_SUPPLY_PROP_BATTERY_LOW_RED_LED,
     MCU_SUPPLY_PROP_RED_LED_FLASH,
     MCU_SUPPLY_PROP_REAL_SHUTDOWN_DUT,
};

typedef union {
	uint32_t mcu_event_val;
	uint32_t pd_event_val;

} mcu_manager_charge_event_para_t;


union mcu_manager_supply_propval {
	int intval;
	const char *strval;
};

typedef void (*mcu_callback_t)(mcu_charge_event_t event, mcu_manager_charge_event_para_t *para);

struct mcu_manager_supply_driver_api {
	
    int (*get_property)(struct device *dev, enum mcu_manager_supply_property psp,
			     union mcu_manager_supply_propval *val);
	void (*set_property)(struct device *dev, enum mcu_manager_supply_property psp,
			     union mcu_manager_supply_propval *val);
    void (*mcu_response)(struct device *dev, enum mcu_manager_supply_property psp,
			     union mcu_manager_supply_propval *val);

    void (*register_mcu_notify)(struct device *dev, mcu_callback_t cb);
};

struct device *wlt_device_get_mcu_mspm0l(void);
struct device *wlt_device_get_mcu_ls8a10049t(void);
bool mcu_ls8a10049t_is_dc_in(void);
bool mcu_mspm0l_is_dc_in(void);
///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

#endif