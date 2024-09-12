/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief TI PD 65992 supply device driver interface
 */

#ifndef __INCLUDE_WLTMCU_MANAGER_SUPPLY_H__
#define __INCLUDE_WLTMCU_MANAGER_SUPPLY_H__

#include <stdint.h>
#include <device.h>

//#define OTG_PHONE_POWER_NEED_SHUTDOWN

#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL0        5
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL1        15
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL2        30
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL3        45
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL4        60
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL5        75
#define BATTERY_DISCHARGE_REMAIN_CAP_LEVEL6        100

typedef enum
{
    PD_V_BUS_G1_HIGH_G2_LOW,
    PD_V_BUS_G1_LOW_G2_HIGH,
    PD_V_BUS_G1_G2_INIT,
    PD_V_BUS_G1_G2_DEINIT,
} pd_v_bus_g1_g2_t;


typedef enum
{
    BTMODE_APP_MODE,
    CHARGING_APP_MODE,
    DEMO_APP_MODE,
  
} wlt_bt_app_mode_t;

typedef enum
{
    CHARGING_APP_NORMAL = 0,
    CHARGING_APP_EXIT,
    CHARGING_APP_INIT,
    CHARGING_APP_TTS_DONE,
    CHARGING_APP_OK,
    CHARGING_APP_POWER_DOWM,
} wlt_charge_app_state_t;

enum pd_manager_supply_property {
	/* Properties of type `int' */
    MCU_SUPPLY_RESP_POWER = 1,
    MCU_SUPPLY_RESP_DC_IN,

    LED_SUPPLY_PROP_AURACASE = 0x10,
    LED_SUPPLY_PROP_BT,
    LED_SUPPLY_PROP_BATERY,
    
	PD_SUPPLY_PROP_STATUS = 0x20,
	PD_SUPPLY_PROP_CURRENT_2400MA,
    PD_SUPPLY_PROP_SOURCE_CURRENT_500MA,
    PD_SUPPLY_PROP_SOURCE_CURRENT_1000MA,
    PD_SUPPLY_PROP_SOURCE_CHARGING_OTG,
	PD_SUPPLY_PROP_VOLT_PROIRITY,
    PD_SUPPLY_PROP_OTG_OFF,
    PD_SUPPLY_PROP_OTG_ON,
    PD_SUPPLY_PROP_SINK_CHANGING_ON,
    PD_SUPPLY_PROP_SINK_CHANGING_OFF,
    PD_SUPPLY_PROP_STANDBY,
    PD_SUPPLY_PROP_WAKEUP,
    PD_SUPPLY_PROP_SOURCE_CURRENT_VAULE,
    PD_SUPPLY_PROP_SOURCE_VOLT_VAULE,
    PD_SUPPLY_PROP_SINK_VOLT_VAULE,
    PD_SUPPLY_PROP_SINK_CURRENT_VAULE,
    PD_SUPPLY_PROP_SINK_HIGH_Z_STATE,
    PD_SUPPLY_PROP_SOURCE_SSRC,
    PD_SUPPLY_PROP_PLUG_PRESENT,
    PD_SUPPLY_PROP_POWERDOWN,
    PD_SUPPLY_PROP_ONOFF_AMP_PVDD,
    PD_SUPPLY_PROP_ONOFF_BGATE,
    PD_SUPPLY_PROP_UNLOAD_FINISHED,
    PD_SUPPLY_PROP_OTG_MOBILE,
    PD_SUPPLY_PROP_SOURCE_DISC,

    PD_SUPPLY_PROP_ONOFF_G2,

    PD_SUPPLY_PROP_PD_VERSION,
	PD_SUPPLY_PROP_TEST_SINK_CHARAGE_CURRENT,//for factory test
	PD_SUPPLY_PROP_TEST_GET_SINK_CHARGE_STEP,//for factory test
    PD_SUPPLY_PROP_TYPEC_WATER_WARNING_STATUS,//1:HAVE WATER WARNIG; 0: DONOT HAVE water WARNIG
    PD_SUPPLY_PROP_TYPEC_WATER_WARNING_TRIGGER_CNT,

    PD_SUPPLY_PROP_IDLE,


};

typedef enum
{
    WLT_FILTER_NOTHING = 0,
    WLT_FILTER_CHARGINE_POWERON,
    WLT_FILTER_CHARGINE_WARNING,
    WLT_FILTER_STANDBY_POWEROFF,
    WLT_FILTER_DISCHARGE_POWERON,
    WLT_FILTER_EIXT_STANDBY_POWEROFF,
  
} wlt_filter_led_t;

enum bt_mcu_pair_led_cmd_t{
    BT_LED_STATUS_OFF,
    BT_LED_STATUS_ON,
    BT_LED_STATUS_VERY_SLOW_FLASH,
    BT_LED_STATUS_SLOW_FLASH,
    BT_LED_STATUS_FLASH,
    BT_LED_STATUS_QUICK_FLASH

};

enum hw_factory_info_t{
    HW_3NOD_BOARD = 0,
    HW_TONGLI_BOARD,
    HW_GUOGUANG_BOARD,
};

enum hw_board_info_t{
    GGC_EV1_TONLI_EV3 = 0,
    GGC_EV2_TONLI_DV1,
};

#define ODM_ADC_MAPPINGS	\
	{HW_3NOD_BOARD,                0x0f},	\
	{HW_GUOGUANG_BOARD,              0x1F0},	\
	{HW_TONGLI_BOARD,                0x2E0},

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Distinguish between different manufacturers
 *
 *
 * @return 0 tongli , 1 3nod
 */
uint8_t ReadODM(void);

struct device *wlt_device_get_pd_ti65992(void);
int pd_ti65992_init();
struct device *wlt_device_get_pd_mps52002(void);
int pd_mps52002_init(void);
int pd_manager_send_cmd_code(uint8_t type, int code);
void pd_set_app_mode_state(u8_t status);
void pd_manager_send_disc(void);
void pd_manager_pd_wakeup(void);
bool pd_set_source_refrest(void);
void pd_manager_init(bool flag);
int pd_manager_deinit(int value);	
void pd_manager_disable_charging(bool flag);
bool pd_get_plug_present_state(void);
bool pd_set_plug_present_state(bool flag);
int pd_manager_get_source_status(void);
void pd_manager_v_sys_g1_g2(uint8_t flag);

uint8_t pd_manager_get_poweron_filte_battery_led(void);
void pd_manager_set_poweron_filte_battery_led(uint8_t flag);

void MPS_PB6_LOW(bool OnOFF);

void wait_pd_init_finish(void);
bool pd_get_sink_full_state(void);

uint8_t pd_get_app_mode_state(void);

bool pd_manager_get_source_change_state(void);

void pd_init_and_disable_charge(bool flag);
/**
 * @brief get PD charging status
 *
 *
 * @return 1 charging , 0 discharging
 */
bool pd_get_sink_charging_state(void);
u32_t pd_get_sink_charge_volt(void);
u32_t pd_get_sink_charge_current(void);
u32_t pd_get_source_volt(void);
u32_t pd_get_source_current(void);
u8_t pd_get_pd_version(void);


/**
 * @brief Set the status of BT light
 *
 * @param link_status status of led
 *
 * @return
 */
void bt_manager_sys_event_led_display(int link_status);
/**
 * @brief Set the status of auracast light
 *
 * @param status status of led
 *
 * @return
 */
void bt_manager_auracast_led_display(bool status);
/**
 * @brief Update the status of all lights
 *
 *
 * @return
 */
void bt_manager_update_led_display(void);
/**
 * @brief Control MCU to enter firmware update
 *
 *
 * @return
 */
void bt_manager_user_ctl_mcu_update(void);
/**
 * @brief Set the status of all lights
 *
 * @param status status of led
 *
 * @return
 */
void bt_ui_charging_warning_handle(uint8_t status);
/**
 * @brief Set all lights to stay on
 *
 *
 * @return
 */
void battery_charging_LED_on_all(void);

void bt_mcu_send_pw_cmd_exit_standy(void);
int bt_mcu_send_pw_cmd_standby(void);
bool bt_mcu_get_pw_cmd_exit_standy(void);
int bt_mcu_send_pw_cmd_charging(void);
bool wlt_mcu_ui_is_dc_in(void);
/**
 * @brief MCU mspm0l interrupt handling
 *
 *
 * @return
 */
int mcu_mspm0l_int_deal(void);
/**
 * @brief MCU ls8a10049t interrupt handling
 *
 *
 * @return
 */
int mcu_ls8a10049t_int_deal(void);

/**
 * @brief Enable bt led state
 *
 * @param enable 1:do not change state; 0: change state
 *
 * @return
 */
void mcu_ui_set_enable_bt_led_state(bool enable);

/**
 * @brief Enable auracast led state
 *
 * @param enable 1:do not change state; 0: change state
 *
 * @return
 */
void mcu_ui_set_enable_auracast_led_state(bool enable);

/**
 * @brief Enable battery led state
 *
 * @param enable 1:do not change state; 0: change state
 *
 * @return
 */
void mcu_ui_set_enable_bat_led_state(bool enable);

/**
 * @brief get enable bt led state
 *
 * @param 
 *
 * @return 0/1
 */
bool mcu_ui_get_enable_bt_led_state(void);

/**
 * @brief get enable auracast led state
 *
 * @param 
 *
 * @return 0/1
 */
bool mcu_ui_get_enable_auracast_led_state(void);

/**
 * @brief get enable battery led state
 *
 * @param 
 *
 * @return 0/1
 */
bool mcu_ui_get_enable_bat_led_state(void);
#ifdef OTG_PHONE_POWER_NEED_SHUTDOWN
/**
 * @brief Enable real shutdown dut
 *
 * @param enable 1:enable; 0: disable
 *
 * @return
 */
void mcu_ui_set_real_shutdown_dut(bool enable);
#endif

void mcu_ui_set_led_just_level(bool up);

#ifdef CONFIG_C_TEST_BATT_MACRO
enum{
    WLT_BATTERY_CAP_TEST = 0,
    WLT_BATTERY_TEMPERATURE_TEST,
    WLT_TYPC_TEMPERATURE_TEST,
    WLT_TYPC_WATER_TEST,
    WLT_TEST_ID_NUM,
};
void wlt_simulation_test_handle(bool flag);
uint8_t wlt_simulation_test_switch(void);
#endif

#ifdef CONFIG_ACTIONS_IMG_LOAD
void mcu_ui_power_hold_fn(void);
#endif

uint8_t mcu_ui_get_poweron_from_charge_warnning_status(void);
void mcu_ui_set_poweron_from_charge_warnning_status(uint8_t status);
typedef enum{

    PD_EVENT_SOURCE_REFREST = 1,
    PD_EVENT_SOURCE_BATTERY_DISPLAY,
    PD_EVENT_BT_LED_DISPLAY,
    PD_EVENT_AC_LED_DISPLAY,
    PD_EVENT_LED_LOCK,
    PD_EVENT_MUC_UPDATA,
    PD_EVENT_JUST_LED_LEVEL,

}pd_event;



typedef enum{

    PA_EVENT_AW_20V5_CTL = 1,
    

}pd_sync_event;
	
typedef enum{
    BATT_LED_NORMAL_ON = 1,
    BATT_LED_NORMAL_OFF,
    BATT_LED_CAHARGING,
    BATT_LED_EXT_CAHARGE,
    BATT_LED_ON_10S,
    BATT_LED_ON_2S,
    CHARGING_WARNING,
    REMOVE_CHARGING_WARNING,
    REBOOT_LED_STATUS,
    LOW_POWER_OFF_LED_STATUS,
    DOME_MODE_LED_STATUS,
    BATT_LED_RESET,
    BATT_PWR_LED_ON_0_5S,//PWROFF pwr&bat off 500ms
}batt_led_status;

#define BT_LED_STATE(STATE)    (STATE & 0XFF)
#define AC_LED_STATE(STATE)    ((STATE << 8) & 0XFFFF)
#define BAT_LED_STATE(STATE)    ((STATE << 16) & 0XFFFFFF)

#define BT_LED_STATE_MASK(STATE)    (STATE & 0XFF)
#define AC_LED_STATE_MASK(STATE)    ((STATE >> 8) & 0XFF)
#define BAT_LED_STATE_MASK(STATE)    ((STATE >> 16) & 0XFF)
void mcu_ui_set_led_enable_state(int state);
extern void pd_srv_sync_init(void);
extern int pd_srv_sync_exit(int value);
extern int pd_srv_sync_send_msg(int event, int value);
extern int pd_srv_event_notify(int event ,int value);

extern uint8_t Read_hw_ver(void);
extern bool logic_mcu_ls8a10049t_get_water_warning_status(void);
extern uint8_t mcu_get_water_charge_warnning_status(void);
#endif
