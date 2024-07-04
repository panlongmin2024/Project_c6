/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INC_BOARD_H
#define __INC_BOARD_H

#include <soc.h>

#ifdef CONFIG_MMC_0
#define BOARD_SDCARD_DETECT_GPIO_NAME		"GPIO_0"
#define BOARD_SDCARD_DETECT_GPIO		19	/* detect */
#define BOARD_SDCARD_USE_INTERNAL_PULLUP		/* use SoC internal pull-up resistance */
#define BOARD_SDCARD_POWER_EN_GPIO_NAME		"GPIO_0"
#define BOARD_SDCARD_POWER_EN_GPIO		19	/* detect gpio is also used as power en gpio */
#endif

#define BOARD_SPI1_PSRAM_CS_GPIO_NAME		"GPIO_0"
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
#define BOARD_SPI1_PSRAM_CS_GPIO		16	/* spi1_ss */
#endif

#define BOARD_SPI1_NOR_CS_GPIO_NAME		"GPIO_0"
#define BOARD_SPI1_NOR_CS_GPIO			32 /* spi1 nor cs gpio */

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
//dc_power_in io
#define DC_POWER_IN_PIN		7

#define THREE_L_HW_2
#endif

#define BOARD_ADCKEY_KEY_MAPPINGS	\
	{KEY_BT,                0x0f},	\
	{KEY_PREVIOUSSONG,      0x40},	\
	{KEY_NEXTSONG,          0x80},	\
	{KEY_MENU,              0x110},	\
	{KEY_TBD,               0x170},	\
	{KEY_COMBO_VOL,         0x1A8},	\
	{KEY_VOLUMEDOWN,        0x200},	\
	{KEY_VOLUMEUP,          0x2A0},

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
//key io
#define KEY_VOL_DOWN_PIN     		5 //vol-
#define KEY_VOL_UP_PIN     			21 // vol+  21
#define KEY_PLAY_PAUSE_PIN     		38 //Play/PAUSE
#define KEY_BT_PIN     				39 //BT

#define KEY_BROADCAST_PIN           40 //Auracast
#define KEY_WATER_PIN 				23	//water int

#endif


#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
#define BOARD_GPIOKEY_KEY_MAPPINGS              \
	{KEY_PAUSE_AND_RESUME, KEY_PLAY_PAUSE_PIN},         \
	{KEY_BT, KEY_BT_PIN},               \
	{KEY_VOLUMEDOWN, KEY_VOL_DOWN_PIN}, \
	{KEY_VOLUMEUP, KEY_VOL_UP_PIN}, 	\
	{KEY_BROADCAST, KEY_BROADCAST_PIN},	\
	{KEY_F1, KEY_WATER_PIN},
#endif
#ifdef CONFIG_WLT_MODIFY_BATTERY_DISPLAY  
//#define BOARD_BATTERY_CHARGING_STATUS_GPIO	15
#else
#define BOARD_BATTERY_CHARGING_STATUS_GPIO	15
#endif

#define BOARD_BATTERY_CAP_MAPPINGS      \
	{0, 3200},  \
	{5, 3300},  \
	{10, 3400}, \
	{20, 3550}, \
	{30, 3650}, \
	{40, 3750}, \
	{50, 3800}, \
	{60, 3850}, \
	{70, 3900}, \
	{80, 3950}, \
	{90, 4000}, \
	{100, 4050},


#define LINEIN_DETEC_ADVAL_RANGE \
        760,    \
        830


#define HOLDABLE_KEY {KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_NEXTSONG, KEY_PREVIOUSSONG, KEY_BT}
#define MUTIPLE_CLIK_KEY {KEY_PAUSE_AND_RESUME, KEY_TBD, KEY_NEXTSONG, KEY_PREVIOUSSONG}


/* led id, gpio , pwm id, active level*/
typedef struct {
    u8_t led_id;
    u8_t led_pin;
    u8_t led_pwm;
    u8_t active_level;
}led_map;


#ifndef CONFIG_SOC_MAPPING_PSRAM
#ifndef CONFIG_BT_CONTROLER_DEBUG_GPIO
#ifdef CONFIG_3NODS_PRO_EV_HW_2
#define BOARD_LED_MAP   \
    /*led_id , led_pin. led_pwm,  active_level    */\
	{   128,          6,          4,                  1},

#else
#define BOARD_LED_MAP   \
    /*led_id , led_pin. led_pwm,  active_level    */\
	{   128,          6,          4,                  0},\

#endif
#ifdef CONFIG_XSPI1_NOR_ACTS
#define BOARD_PWM_MAP   \
    {21,     6},\
    {21,     6},
#else
#define BOARD_PWM_MAP   \
    {6,     6},\
 
#endif
#endif
#endif

#ifdef CONFIG_AUDIO_OUT_SPDIFTX_SUPPORT
	#define BOARD_SPDIFTX_MAP {23,  0x5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},
#else
	#define BOARD_SPDIFTX_MAP
#endif

#ifdef CONFIG_AUDIO_IN_SPDIFRX_SUPPORT
	#define BOARD_SPDIFRX_MAP { 7,  0x5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},
#else
	#define BOARD_SPDIFRX_MAP
#endif

/*!
 * enum ain_logic_source_type_e
 * @brief Audio input logic devices for middleware and application.
 */
typedef enum {
	AIN_LOGIC_SOURCE_LINEIN = 0,
	AIN_LOGIC_SOURCE_ATT_AUXFD,
	AIN_LOGIC_SOURCE_ATT_AUX0,
	AIN_LOGIC_SOURCE_ATT_AUX1,
	AIN_LOGIC_SOURCE_MIC0,
	AIN_LOGIC_SOURCE_MIC1,
	AIN_LOGIC_SOURCE_FM,
	AIN_LOGIC_SOURCE_DMIC,
	AIN_LOGIC_SOURCE_USB,
} ain_logic_source_type_e;

extern void board_extern_pa_ctl(uint8_t mode);

extern int board_extern_pa_class_select(u8_t pa_class);

extern void board_init_psram_pins(void);

extern void board_mmc_function_reset(int mmc_id, u8_t is_lowpower_mode);

extern int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag);

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
extern bool key_vol_down_status_read(void);
extern bool key_vol_up_status_read(void);
extern bool key_play_pause_status_read(void);
extern bool key_bt_status_read(void);
extern bool key_broadcast_status_read(void);
extern bool dc_power_in_status_read(void);
extern bool key_water_status_read(void);

extern void extern_dsp_3615_io_enable(int enable);
#define GPIO_SPI1_SS 8
#define GPIO_SPI1_CLK 9
#define GPIO_SPI1_MOSI 10
#define GPIO_SPI1_MISO 11

#define GPIO_I2S_BCLK 33
#define GPIO_I2S_LRCLK 34
#define GPIO_I2S_TX 35

#define DSP_EN_PIN  32
#define DSP_POWER_EN_PIN 16
#define GPIO_DSP_SATTUS 12
#endif

#endif /* __INC_BOARD_H */
