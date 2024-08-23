/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file power manager interface
 */

#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__
#include "power_supply.h"

/**
 * @defgroup power_manager_apis App power Manager APIs
 * @ingroup system_apis
 * @{
 */

/** battary state enum */
enum bat_state_e {
	/** no battery detacted*/
	BAT_STATUS_UNKNOWN = 0,
	/** battery charging */
	BAT_STATUS_CHARGING,
	/** battery charged full */
	BAT_STATUS_FULL,
	/** battery no charge */
	BAT_STATUS_NORMAL,
};



/**
 * @brief get system battery capacity
 *
 *
 * @return battery capacity , The unit is the percentage
 */

int power_manager_get_battery_capacity(void);
int power_manager_get_battery_temperature(void);

#define POWER_MANAGER_BATTER_10_SECOUND	100						// 10 s
#define POWER_MANAGER_BATTER_2_SECOUND	20						// 2s
#define DEFAULT_NOPOWER_CAP_LEVEL	    				5


/**
 * @brief get system battery vol
 *
 *
 * @return battery vol , The unit is mv
 */

int power_manager_get_battery_vol(void);


/**
 * @brief get system battery vol
 *
 *
 * @return battery vol , The unit is mv
 */
int power_manager_set_battery_vol(u16_t vol);

/**
 * @brief get system charge status
 *
 *
 * @return charget status, POWER_SUPPLY_STATUS_DISCHARGE, POWER_SUPPLY_STATUS_CHARGING... see power_supply_status
 */

int power_manager_get_charge_status(void);


/**
 * @brief get system dc5v status
 *
 *
 * @return dc5v status, 1 dc5v exist  0 dc5v not exit.
 */

int power_manager_get_dc5v_status(void);

/**
 * @brief register system charge status changed callback
 *
 *
 */
int power_manager_init(void);
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
int power_manager_early_init(void);
#endif
/**
 * @} end defgroup power_manager_apis
 */

int power_manager_set_slave_battery_state(int capacity, int vol);
int power_manager_sync_slave_battery_state(void);

bool power_manager_get_battery_is_full(void);
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY
int power_manager_set_battery_cap(u16_t vol);
int power_manager_set_battery_remain_charge_time(u16_t vol);
int power_manager_set_battery_temperature(int vol);
void power_manager_battery_display_handle(int led_status ,bool key_flag);

#endif
#endif
