/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief board init functions
 */

#include <init.h>
#include <gpio.h>
#include <soc.h>
#include "board.h"
#include <device.h>
#include <pwm.h>
#include <input_dev.h>
#include <audio_common.h>
#include "board_version.h"
#include <wltmcu_manager_supply.h>
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_BOARD

#ifdef CONFIG_BT_CONTROLER_BQB
extern int btdrv_get_bqb_mode(void);
#endif

#ifdef CONFIG_SOC_MAPPING_PSRAM
static const struct acts_pin_config board_pin_psram_config[] =
{
	{ 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
	{ 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
	{ 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
	{ BOARD_SPI1_PSRAM_CS_GPIO, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
	{13, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},	/* so3 */
	{14, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},	/* so2 */

#ifdef CONFIG_XSPI1_NOR_ACTS
	{BOARD_SPI1_NOR_CS_GPIO, 0 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP}, /* spi1 nor cs, pullup */
#endif
};
#endif
static const struct acts_pin_config board_pin_config[] = {
	/* uart0 */
	{2, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* UART0_RX */
	{3, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* UART0_TX */

	/* spi0, nor flash */
	{28, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SS */
	{29, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SCLK */
	{30, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MISO */
	{31, 3 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MOSI */

#if (CONFIG_XSPI_NOR_ACTS_IO_BUS_WIDTH == 4)
	{26, 4 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO2 */
	{27, 4 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO3 */
#endif

#ifdef CONFIG_I2C_SLAVE_ACTS
	{CONFIG_I2C_SLAVE_0_SCL_PIN,  	5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* I2C0_SCL */
	{CONFIG_I2C_SLAVE_0_SDA_PIN,  	5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* I2C0_SDA */
#endif

	{54, GPIO_CTL_AD_SELECT | GPIO_CTL_GPIO_INEN },	/* AUX2L */
	{55, GPIO_CTL_AD_SELECT | GPIO_CTL_GPIO_INEN },	/* AUX2R */

	/* i2s tx0 */
#ifdef CONFIG_AUDIO_OUT_I2STX0_SUPPORT
	// {40,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
	//{33,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	//{34, 0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	//{35,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* dout */
#endif

	/* i2s rx0 */
#ifdef CONFIG_AUDIO_IN_I2SRX0_SUPPORT
	{40,  0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
	{39,  0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	{6, 0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	{7,   0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* din */
#endif

	/* i2s rx1 */
#ifdef CONFIG_AUDIO_IN_I2SRX1_SUPPORT
	//BOARD_SPI1_NOR_CS_GPIO use GPIO32
	//{32,  0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
	{33,  0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	{34, 0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	{35,   0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* din */
#endif

#if 0
	/* aout */
	{50,  GPIO_CTL_AD_SELECT},  /* aoutl/aoutlp */
	{52,  GPIO_CTL_AD_SELECT},  /* aoutr/aoutrp */


	//直驱or  差分需要设置vro & vro_s
#if ((CONFIG_AUDIO_OUT_DD_MODE == 1) || (CONFIG_AUDIO_OUT_SE_DF == 1))
	{51,  GPIO_CTL_AD_SELECT},	/* vro */
	{53,  GPIO_CTL_AD_SELECT},	/* vros */
#endif
#endif
	/* external pa */
#ifdef CONFIG_BOARD_EXTERNAL_PA_ENABLE
	{21, 0},	 /* external pa ctl1 */
	{5, 0},	 /* external pa ctl2 */
#endif


#ifdef CONFIG_XSPI1_NOR_ACTS
	{11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{ 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{ 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{13, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},     /* so3 */
	{14, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},     /* so2 */
#endif

#ifdef CONFIG_SPI_1
/* 	{GPIO_SPI1_SS, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(5)},
	{GPIO_SPI1_CLK, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(5)},
	{GPIO_SPI1_MOSI, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(5)},
	{GPIO_SPI1_MISO, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(5)}, */
#endif

#ifdef CONFIG_I2C_ACTS //hardware i2c
	{1, 0x05| GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4) | GPIO_CTL_PULLUP}, //I2C_SDA
    {0, 0x05| GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4) | GPIO_CTL_PULLUP}, //I2C_SCL
#endif

#ifdef CONFIG_I2C_GPIO_0 //software i2c
	//
#endif
};

#ifdef CONFIG_MMC_0
static const struct acts_pin_config board_pin_mmc_config[] =
{
	/* sd0 */
	{17, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},			/* SD0_CLK */
#ifdef BOARD_SDCARD_USE_INTERNAL_PULLUP
	/* sd0, internal pull-up resistances in SoC */
	{16, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PULLUP | GPIO_CTL_PADDRV_LEVEL(1)}, /* SD0_CMD */
	{20, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PULLUP | GPIO_CTL_PADDRV_LEVEL(1)}, /* SD0_D0 */
#else
	/* sd0, external pull-up resistances on the board */
	{16, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SD0_CMD */
	{20, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SD0_D0 */
#endif
};
#endif

#ifdef CONFIG_BT_CONTROLER_BLE_BQB
static const struct acts_pin_config board_pin_config_bqb[] = {
#if (CONFIG_BT_BQB_UART_PORT == 1)
	/* use uart1 for BQB test */
	{21, 0xe | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_TX */
	{22, 0xe | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_RX */
#endif
};
#endif

static const audio_input_map_t board_audio_input_map[] =  {
	{AIN_LOGIC_SOURCE_LINEIN, AIN_SOURCE_AUXFD, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUXFD, AIN_SOURCE_AUXFD, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUX0, AIN_SOURCE_AUX0, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUX1, AIN_SOURCE_AUX1, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_MIC0, AIN_SOURCE_ASEMIC_AUX2, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_MIC1, AIN_SOURCE_ASEMIC, INPUTSRC_ONLY_L},
	{AIN_LOGIC_SOURCE_FM, AIN_SOURCE_AUX2, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_DMIC, AIN_SOURCE_DMIC, INPUTSRC_L_R},
};

int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(board_audio_input_map); i++) {
		if (logical_dev == board_audio_input_map[i].logical_dev) {
			*physical_dev = board_audio_input_map[i].physical_dev;
			*track_flag = board_audio_input_map[i].track_flag;
			break;
		}
	}

	if (i == ARRAY_SIZE(board_audio_input_map)) {
		printk("can not find out audio dev %d\n", logical_dev);
		return -ENOENT;
	}

	return 0;
}

#ifdef BOARD_SDCARD_POWER_EN_GPIO

#define SD_CARD_POWER_RESET_MS	80

#define SD0_CMD_PIN		16
#define SD0_D0_PIN		20
#define SD0_CLK_PIN     17

static int pinmux_sd0_cmd, pinmux_sd0_d0, pinmux_sd0_clk;
static int sd_card_reset_ms;

static void board_mmc0_pullup_disable(void)
{
	struct device *sd_gpio_dev;

	sd_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!sd_gpio_dev)
		return;

	/* backup origin pinmux config */
	acts_pinmux_get(SD0_CMD_PIN, &pinmux_sd0_cmd);
	acts_pinmux_get(SD0_D0_PIN, &pinmux_sd0_d0);
	acts_pinmux_get(SD0_CLK_PIN, &pinmux_sd0_clk);

	/* sd_cmd pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_CMD_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_CMD_PIN, 0);

	/* sd_d0 pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_D0_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_D0_PIN, 0);

	/* sd_clk pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_CLK_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_CLK_PIN, 0);
}

static void board_mmc0_pullup_enable(void)
{
	/* restore origin pullup pinmux config */
	acts_pinmux_set(SD0_CMD_PIN, pinmux_sd0_cmd);
	acts_pinmux_set(SD0_D0_PIN, pinmux_sd0_d0);
	acts_pinmux_set(SD0_CLK_PIN, pinmux_sd0_clk);
}

static int board_mmc_power_gpio_reset(struct device *power_gpio_dev, int power_gpio)
{
	gpio_pin_configure(power_gpio_dev, power_gpio,
			   GPIO_DIR_OUT);

	/* 0: power on, 1: power off */
	/* card vcc power off */
	gpio_pin_write(power_gpio_dev, power_gpio, 1);

	/* disable mmc0 pull-up to avoid leakage */
	board_mmc0_pullup_disable();

	k_sleep(sd_card_reset_ms);

	/* card vcc power on */
	gpio_pin_write(power_gpio_dev, power_gpio, 0);

	k_sleep(10);

	/* restore mmc0 pull-up */
	board_mmc0_pullup_enable();

	return 0;
}
#endif	/* BOARD_SDCARD_POWER_EN_GPIO */

int board_mmc_power_reset(int mmc_id, u8_t cnt)
{
#ifdef BOARD_SDCARD_POWER_EN_GPIO

	struct device *power_gpio_dev;

	if (mmc_id != 0)
		return 0;

	power_gpio_dev = device_get_binding(BOARD_SDCARD_POWER_EN_GPIO_NAME);
	if (!power_gpio_dev)
		return -EINVAL;

	sd_card_reset_ms = cnt * SD_CARD_POWER_RESET_MS;
	if (sd_card_reset_ms <= 0)
		sd_card_reset_ms = SD_CARD_POWER_RESET_MS;

	board_mmc_power_gpio_reset(power_gpio_dev, BOARD_SDCARD_POWER_EN_GPIO);

#if defined(BOARD_SDCARD_DETECT_GPIO) && (BOARD_SDCARD_DETECT_GPIO == BOARD_SDCARD_POWER_EN_GPIO)
	/* switch gpio function to input for detecting card plugin */
	gpio_pin_configure(power_gpio_dev, BOARD_SDCARD_DETECT_GPIO, GPIO_DIR_IN);
#endif

#endif	/* BOARD_SDCARD_POWER_EN_GPIO */

	return 0;
}

void board_mmc_function_reset(int mmc_id, u8_t is_lowpower_mode)
{
	if(mmc_id == 0){
#ifdef CONFIG_MMC_0
		//disable sdmmc0 cmd&data for reduce power resume
		if(is_lowpower_mode){
			u32_t i;
			for(i = 0; i < ARRAY_SIZE(board_pin_mmc_config); i++){
				sys_write32(0x1000, GPIO_CTL(board_pin_mmc_config[i].pin_num));
			}
		}else{
			acts_pinmux_setup_pins(board_pin_mmc_config, ARRAY_SIZE(board_pin_mmc_config));
		}
#endif
	}
}

#define EXTERN_PA_CTL1_PIN  21
#define EXTERN_PA_CTL2_PIN  5
int board_extern_pa_class_select(u8_t pa_class)
{
	struct device *pa_gpio_dev;

	pa_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!pa_gpio_dev)
		return -1;

	if (pa_class == EXT_PA_CLASS_AB) {
		SYS_LOG_INF("open external PA class AB");
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 1);

		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 0);
	} else if (pa_class == EXT_PA_CLASS_D) {
		// classD mode, pa_ctl1=1,	pa_ctl2=1
		SYS_LOG_INF("open external PA class D");
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 1);

		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 1);
	} else {
		SYS_LOG_ERR("invalid pa class:%d", pa_class);
		return -EINVAL;
	}

	return 0;
}

void board_extern_pa_ctl(uint8_t mode)
{
#ifdef CONFIG_BT_CONTROLER_BQB
	if (btdrv_get_bqb_mode() == 0) {
#endif
		struct device *pa_gpio_dev;

		pa_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
		if (!pa_gpio_dev)
			return;

		if(mode == 0) {
			//PA shutoff ,  pa_ctl1=0, pa_ctl2=0
			SYS_LOG_INF("close external PA");
			gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
			gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 0);

			gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
			gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 0);

		} else if(mode == 1) {
			// open
#if (CONFIG_EXTERN_PA_CLASS == 0)
			board_extern_pa_class_select(EXT_PA_CLASS_AB);
#else
			board_extern_pa_class_select(EXT_PA_CLASS_D);
#endif
		} else {
			SYS_LOG_ERR("invalid PA working mode:%d", mode);
		}
#ifdef CONFIG_BT_CONTROLER_BQB
	}
#endif
}
#ifdef CONFIG_SOC_MAPPING_PSRAM
void board_init_psram_pins(void)
{
	acts_pinmux_setup_pins(board_pin_psram_config, ARRAY_SIZE(board_pin_psram_config));
#ifdef CONFIG_XSPI1_NOR_ACTS
    sys_write32(sys_read32(0xc00900A4) | (1 << 6), 0xc00900A4);
	sys_write32(1 << (40 - 32), 0xc009010C);
#endif
}
#endif

static int board_early_init(struct device *arg)
{
	ARG_UNUSED(arg);
	struct device *pa_gpio_dev;
	int value = 0;

#if 0
	/* LRADC1: wio0  */
	value = sys_read32(WIO0_CTL);
	value = (value & (~(0x0000000F))) | (1 << 3);
	sys_write32(value, WIO0_CTL);
#endif

	/* bandgap has filter resistor  */
	value = sys_read32(BDG_CTL);
	value = (value | (1 << 6));
	sys_write32(value, BDG_CTL);

#ifdef CONFIG_PWM_ACTS
	//acts_pinmux_setup_pins(board_led_pin_config, ARRAY_SIZE(board_led_pin_config));
#endif
	pa_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	gpio_pin_configure(pa_gpio_dev, GPIO_SPI1_SS, GPIO_DIR_OUT);
	gpio_pin_write(pa_gpio_dev, GPIO_SPI1_SS, 0);
	acts_pinmux_setup_pins(board_pin_config, ARRAY_SIZE(board_pin_config));

#ifdef CONFIG_MMC_0
	acts_pinmux_setup_pins(board_pin_mmc_config, ARRAY_SIZE(board_pin_mmc_config));
#endif

	return 0;
}

void board_jtag_init(void)
{
#ifdef CONFIG_CPU0_EJTAG_ENABLE
	soc_debug_enable_jtag(SOC_JTAG_CPU0, CONFIG_CPU0_EJTAG_GROUP);
#else
	soc_debug_disable_jtag(SOC_JTAG_CPU0);
#endif

#ifdef CONFIG_DSP_EJTAG_ENABLE
	soc_debug_enable_jtag(SOC_JTAG_DSP, CONFIG_DSP_EJTAG_GROUP);
#else
	soc_debug_disable_jtag(SOC_JTAG_DSP);
#endif
}

#ifdef CHECK_ADFU_BY_ADCKEY

static int adfukey_press_cnt;
static int adfukey_invalid;
static struct input_value adfukey_last_val;

static void board_key_input_notify(struct device *dev, struct input_value *val)
{
	/* any adc key is pressed? */
	if (val->code == KEY_RESERVED) {
		adfukey_invalid = 1;
		return;
	}

	adfukey_press_cnt++;

	/* save last adfu key value */
	if (adfukey_press_cnt == 1) {
		adfukey_last_val = *val;
		return;
	}

	/* key code must be same with last code and status is pressed */
	if (adfukey_last_val.type != val->type ||
		adfukey_last_val.code != val->code ||
		val->code == 0) {
		adfukey_invalid = 1;
	}
}

static void check_adfu_key(void)
{
	struct device *adckey_dev;

	/* check adfu */
	adckey_dev = device_get_binding(CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME);
	if (!adckey_dev) {
		printk("%s: error \n", __func__);
	}

	input_dev_enable(adckey_dev);
	input_dev_register_notify(adckey_dev, board_key_input_notify);

	/* wait adfu key */
	k_sleep(80);

	if (adfukey_press_cnt > 0 && !adfukey_invalid) {
		/* check again */
		k_sleep(120);

		if (adfukey_press_cnt > 1 && !adfukey_invalid) {
			SYS_LOG_INF("start to reboot and enter ADFU...");
			/* adfu key is pressed, goto adfu! */
			sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
		}
	}

	input_dev_unregister_notify(adckey_dev, board_key_input_notify);
	input_dev_disable(adckey_dev);
}
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
extern bool mcu_read_dc_status(void);
//返回dc_power_in插入还是拔出，1插入，0拔出
bool dc_power_in_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;
	
	gpio_pin_read(gpio_dev, DC_POWER_IN_PIN, &val);

	// val = mcu_read_dc_status();

  	if(ReadODM())

   {
    #ifdef CONFIG_TOLY_EV
	return (val == 0);
	  #else
     return (val == 1);
	  #endif
   }
 else
 {
	   #ifdef CONFIG_3NODS_PRO_EV_HW_2
		return (val == 0);
	  #else
		return (val == 1);
	   #endif
 }
}

bool key_water_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;
	gpio_pin_read(gpio_dev, KEY_WATER_PIN, &val);

	return (val == 1);
}
static int wait_adfu_flag = 0;
int check_is_wait_adfu(void)
{
	return wait_adfu_flag;
}
void check_adfu_gpio_key(void)
{
	int cnt = 0;
	bool key_bt_status;
	bool key_vol_up;
	bool dc_power_in_status;

	SYS_LOG_INF("check adfu gpio key ok ?\n");

	do 
	{
		key_bt_status = key_bt_status_read();
		key_vol_up = key_vol_up_status_read();
		dc_power_in_status = dc_power_in_status_read();

		SYS_LOG_INF("key_bt down:%d, key_vol_up down:%d, dc_power_in insert:%d\n",
			key_bt_status, key_vol_up, dc_power_in_status);

		if (key_bt_status == 0 || key_vol_up == 0)
		{
			break;
		}

		cnt ++;

		if (cnt < 2)
		{
			k_busy_wait(20*1000);//20ms
			continue;
		}

#if 0
		for (cnt=0; cnt<100; cnt++)//100*20ms=2s
		{
#ifdef CONFIG_WATCHDOG
			watchdog_clear();
#endif
			k_busy_wait(20000);//20ms
		}
#endif
		if(dc_power_in_status){
			sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
		}
		else{
			wait_adfu_flag = 1;
			break;
		}	
	}
	while(1);
}

#define AMP_AVDD_PW_ON		45 //AMP AVDD, high to enable.
//void io_expend_aw9523b_ctl_20v5_set(uint8_t onoff);
void mcu_ui_set_dsp_dcdc(uint8_t onoff);


void extern_dsp_3615_io_enable(int enable)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	if(enable){

		gpio_pin_configure(gpio_dev, DSP_POWER_EN_PIN, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, DSP_POWER_EN_PIN, 1);

		gpio_pin_configure(gpio_dev, DSP_EN_PIN, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, DSP_EN_PIN, 1);

		gpio_pin_configure(gpio_dev, GPIO_DSP_SATTUS, GPIO_DIR_IN | GPIO_PUD_PULL_UP);
		//gpio_pin_configure(gpio_dev, GPIO_DSP_SATTUS, GPIO_DIR_OUT);
		//gpio_pin_write(gpio_dev, GPIO_DSP_SATTUS, 1);

		acts_pinmux_set(GPIO_SPI1_SS, 5 | (0x1 << 5) | GPIO_CTL_PADDRV_LEVEL(5));
		acts_pinmux_set(GPIO_SPI1_CLK, 5 | (0x1 << 5) | GPIO_CTL_PADDRV_LEVEL(5));
		acts_pinmux_set(GPIO_SPI1_MOSI, 5 | (0x1 << 5) | GPIO_CTL_PADDRV_LEVEL(5));
		acts_pinmux_set(GPIO_SPI1_MISO, 5 | (0x1 << 5) | GPIO_CTL_PADDRV_LEVEL(5));

		acts_pinmux_set(GPIO_I2S_BCLK, 0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4));
		acts_pinmux_set(GPIO_I2S_LRCLK, 0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4));
		acts_pinmux_set(GPIO_I2S_TX, 0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4));
		

    if(ReadODM())
	  {	
	   	mcu_ui_set_dsp_dcdc(1);
		gpio_pin_configure(gpio_dev, AMP_AVDD_PW_ON, GPIO_DIR_OUT | GPIO_POL_NORMAL);
		gpio_pin_write(gpio_dev, AMP_AVDD_PW_ON, 1);		
      }		

	}else{
		gpio_pin_configure(gpio_dev, DSP_EN_PIN, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, DSP_EN_PIN, 0);

		//gpio_pin_configure(gpio_dev, DSP_POWER_EN_PIN, GPIO_DIR_OUT);
		// gpio_pin_write(gpio_dev, DSP_POWER_EN_PIN, 0);		
       if(ReadODM())
		{
		 gpio_pin_configure(gpio_dev, AMP_AVDD_PW_ON, GPIO_DIR_OUT | GPIO_POL_NORMAL);
		 gpio_pin_write(gpio_dev, AMP_AVDD_PW_ON, 0);
		 mcu_ui_set_dsp_dcdc(0);
	    }
		gpio_pin_configure(gpio_dev, GPIO_SPI1_SS, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_SPI1_SS, 0);

		gpio_pin_configure(gpio_dev, GPIO_SPI1_CLK, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_SPI1_CLK, 0);

		gpio_pin_configure(gpio_dev, GPIO_SPI1_MOSI, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_SPI1_MOSI, 0);

		gpio_pin_configure(gpio_dev, GPIO_SPI1_MISO, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_SPI1_MISO, 0);

		gpio_pin_configure(gpio_dev, GPIO_DSP_SATTUS, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_DSP_SATTUS, 0);

		gpio_pin_configure(gpio_dev, GPIO_I2S_BCLK, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_I2S_BCLK, 0);

		gpio_pin_configure(gpio_dev, GPIO_I2S_LRCLK, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_I2S_LRCLK, 0);

		gpio_pin_configure(gpio_dev, GPIO_I2S_TX, GPIO_DIR_OUT);
		gpio_pin_write(gpio_dev, GPIO_I2S_TX, 0);
	}

}

void _ls8a10049t_power_hold_test(bool onoff)
{
	return;
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(gpio_dev, 22, GPIO_DIR_OUT);
	gpio_pin_write(gpio_dev, 22, onoff);
	printk("%s %d: gpio22: %d\n", __func__, __LINE__,onoff);
}
#endif

static int board_later_init(struct device *arg)
{
	ARG_UNUSED(arg);

	printk("%s %d: \n", __func__, __LINE__);

#ifdef CHECK_ADFU_BY_ADCKEY
	check_adfu_key();
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//check_adfu_gpio_key();
	_ls8a10049t_power_hold_test(0);
#endif

#ifndef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	//iic_test();
#ifdef CONFIG_ACTIONS_IMG_LOAD
    extern int run_test_image(void);
    run_test_image();
#endif
#endif

#ifdef CONFIG_BT_CONTROLER_BLE_BQB
	if(btdrv_get_bqb_mode() == 2) {
		acts_pinmux_setup_pins(board_pin_config_bqb, ARRAY_SIZE(board_pin_config_bqb));
	}
#endif

/* #ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE //temporary
	#define POWER_LED_PIN	6

	struct device *gpio_dev = NULL;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!gpio_dev) 
	{
		printk("[%s,%d] gpio_dev not found\n", __FUNCTION__, __LINE__);
		return -1;
	}
	gpio_pin_configure(gpio_dev, POWER_LED_PIN, GPIO_DIR_OUT);
	gpio_pin_write(gpio_dev, POWER_LED_PIN, 0);
#endif */

	return 0;
}

uint32_t libboard_version_get(void)
{
    return LIBBOARD_VERSION_NUMBER;
}


SYS_INIT(board_early_init, PRE_KERNEL_1, 5);

SYS_INIT(board_later_init, POST_KERNEL, 5);

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
static int dc_power_in_gpio_init(struct device *arg)
{
	SYS_LOG_INF("\n");

	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (gpio_dev == NULL)
	{
		SYS_LOG_ERR("gpio dev is null\n");
		return -1;
	}

	// gpio_pin_configure(gpio_dev, DC_POWER_IN_PIN, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
	gpio_pin_configure(gpio_dev, DC_POWER_IN_PIN, GPIO_DIR_IN );

	return 0;
}

SYS_INIT(dc_power_in_gpio_init, PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
#endif

#ifdef CONFIG_SPI_1

static int pinmux_spi_ss = 0;
static int pinmux_spi_clk = 0;
static int pinmux_spi_mosi = 0;
static int pinmux_spi_miso = 0;

void board_spi1_disable(void)
{
	struct device *dev;

	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!dev) {
		return;
	}

	// backup pinmux config 
	acts_pinmux_get(GPIO_SPI1_SS, &pinmux_spi_ss);
	acts_pinmux_get(GPIO_SPI1_CLK, &pinmux_spi_clk);
	acts_pinmux_get(GPIO_SPI1_MOSI, &pinmux_spi_mosi);
	acts_pinmux_get(GPIO_SPI1_MISO, &pinmux_spi_miso);

	//set PINMUX to GPIO 
	acts_pinmux_set(GPIO_SPI1_SS, 0);
	acts_pinmux_set(GPIO_SPI1_CLK, 0);
	acts_pinmux_set(GPIO_SPI1_MOSI, 0);
	acts_pinmux_set(GPIO_SPI1_MISO, 0);

}

void board_spi1_enable(void)
{
	//restore saved pinmux config.
	acts_pinmux_set(GPIO_SPI1_SS, pinmux_spi_ss);
	acts_pinmux_set(GPIO_SPI1_CLK, pinmux_spi_clk);
	acts_pinmux_set(GPIO_SPI1_MOSI, pinmux_spi_mosi);
	acts_pinmux_set(GPIO_SPI1_MISO, pinmux_spi_miso);
}

extern int dolphin_comm_init(void);
extern int dolphin_comm_deinit(void);

void board_spi_switch(void)
{
	static bool spi_disabled = false;

	if (spi_disabled) {
		SYS_LOG_INF("SPI1 enable\n");
		board_spi1_enable();
		k_busy_wait(20*1000);//20ms
		dolphin_comm_init();
	} else {
		SYS_LOG_INF("SPI1 disable\n");
		dolphin_comm_deinit();
		board_spi1_disable();
	}

	spi_disabled = !spi_disabled;
}
#endif
