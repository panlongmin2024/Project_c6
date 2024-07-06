/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "recovery"

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <fw_version.h>
#include <ota_upgrade.h>
#include <ota_backend.h>
#include <ota_backend_temp_part.h>
#include <ota_backend_sdcard.h>
#include <soc_pm.h>
#include <soc.h>

#ifdef CONFIG_FS_MANAGER
#include <fs_manager.h>
#endif

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#include "sys_manager.h"
#include "flash.h"



#ifdef CONFIG_USB_HOTPLUG
extern int usb_hotplug_host_check(void);
#endif

static struct ota_upgrade_info *g_ota;

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
#define OTA_STORAGE_DEVICE_NAME		CONFIG_XSPI_NOR_ACTS_DEV_NAME
static struct ota_backend *backend_spinor;
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
#define OTA_STORAGE_DEVICE_NAME		CONFIG_XSPI_NOR_ACTS_DEV_NAME
static struct ota_backend *backend_sdcard;
#endif
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <thread_timer.h>
#include <i2c.h>
#include <adc.h>
#include <misc/byteorder.h>
//#include <wltmcu_manager_supply.h>
enum hw_factory_info_t{
    HW_3NOD_BOARD = 0,
    HW_TONGLI_BOARD,
    HW_GUOGUANG_BOARD,
};

enum mcu_int_iic_address_t{
    BT_MCU_ACK_IIC_ADDR,
    MCU_INT_CMD_IIC_ADDR,
    MCU_READ_INT_EVENT_IIC_ADDR = 0x04,
};

struct odm_detect_info {
	struct device *adc_dev;
	struct adc_seq_table seq_tbl;
	struct adc_seq_entry seq_entrys;
	u8_t adcval[4];
};

#define MSPMOL_I2C_ADDR        0x48
#define LS8A10049T_I2C_ADDR    		0x8		// 7bit I2C Addr

#define LS8A10049T_REG_7B			0x7B
typedef struct {
	u8_t bit0 : 1;			// poweron hold
	u8_t bit1 : 1;			// gpio1 output
	u8_t bit2 : 1;			// int_pending 
	u8_t bit3 : 1;			// watchdog
	u8_t bit4 : 1;			// gpio2 output
	u8_t bit5 : 1;
	u8_t bit6 : 1;			// poweroff
	u8_t bit7 : 1;			// poweron_key
} ls8a10049t_reg_7b_t;

static ls8a10049t_reg_7b_t reg_7b = { 0 };

static struct thread_timer mcu_timer;
extern bool key_water_status_read(void);
static uint8_t _ota_ReadODM(void)
{
    u32_t value;
    uint8_t hw_info;

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
	
    if(adc_val < 0x0f)
    {
        hw_info = HW_3NOD_BOARD;
    }
    else if(adc_val < 0x1ff)
    {
        hw_info = HW_GUOGUANG_BOARD;
    }
    else{
        hw_info = HW_TONGLI_BOARD;
    }

    return hw_info;    

}

static int _ota_mcu_mspm0l_poweron_hold(void)
{
    u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);

	if(i2c_burst_read(iic_dev, MSPMOL_I2C_ADDR, MCU_INT_CMD_IIC_ADDR, buf, 1)<0)
    {
        printk("[%s:%d] MCU --> IIC buf: Error!\n", __func__, __LINE__);
        return -EIO;
    }	
	printk("%s,MCU --> IIC buf: 0x%x\n", __func__,buf[0]);
	buf[0] = 0x21;
	i2c_burst_write(iic_dev, MSPMOL_I2C_ADDR, BT_MCU_ACK_IIC_ADDR, buf, 1);
	return 0;	
}
#if 0
static void _ota_ls8a10049t_poweron_hold(void)
{
    //u8_t buf[4] = {0};
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);	
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    SYS_LOG_INF("i2c_dev = %p\n", iic_dev);
   
	reg_7b.bit0 = 0;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	
	k_busy_wait(1000);
	reg_7b.bit0 = 1;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
    SYS_LOG_INF("HOLD write done\n");
}
#endif
static void _ota_ls8a10049t_watchdog_clear(void)
{
    union dev_config config = {0};
    struct device *iic_dev;

    iic_dev = device_get_binding(CONFIG_I2C_0_NAME);	
    config.bits.speed = I2C_SPEED_STANDARD;
    i2c_configure(iic_dev, config.raw);
    SYS_LOG_INF("i2c_dev = %p\n", iic_dev);

	reg_7b.bit3 = 0;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
	reg_7b.bit3 = 1;
	i2c_burst_write(iic_dev, LS8A10049T_I2C_ADDR, LS8A10049T_REG_7B, (u8_t *)&reg_7b, 1);
}

static void _ota_mcu_power_hold_fn(void)
{
    if(!_ota_ReadODM()){

        _ota_mcu_mspm0l_poweron_hold();
    }
    else{
        //_ota_ls8a10049t_poweron_hold();
		_ota_ls8a10049t_watchdog_clear();
    }
    printk("\n%s,power has hold !! \n",__func__);
}

void _ota_mcu_int_timer_hander(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if(!key_water_status_read())
	{
		_ota_mcu_power_hold_fn();
		printk("\n%s \n",__func__);
	}
}
#endif
static void ota_app_start(void)
{
	SYS_LOG_INF("ota app start");
	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, false);
}

static void ota_app_stop(void)
{
	SYS_LOG_INF("ota app stop");
	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, true);
}

static void ota_app_notify(int state, int old_state)
{
	SYS_LOG_INF("ota state: %d->%d", old_state, state);

	if (old_state != OTA_RUNNING && state == OTA_RUNNING) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_set_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "ota");
#endif
	} else if (old_state == OTA_RUNNING && state != OTA_RUNNING) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "ota");
#endif
	}
}

static void ota_app_backend_callback(struct ota_backend *backend, int cmd,
				     int state)
{
	int err;

	SYS_LOG_INF("backend %p state %d", backend, state);
	if (cmd == OTA_BACKEND_UPGRADE_STATE) {
		if (state == 1) {
			err = ota_upgrade_attach_backend(g_ota, backend);
			if (err) {
				SYS_LOG_INF("unable attach backend");
				return;
			}
			ota_app_start();
		} else {
			ota_upgrade_detach_backend(g_ota, backend);
			ota_app_stop();

		}
	}
}

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
int ota_app_init_spinor(void)
{
	struct ota_backend_temp_part_init_param temp_part_init_param;

	memset(&temp_part_init_param, 0x0,
	       sizeof(struct ota_backend_temp_part_init_param));
	temp_part_init_param.dev_name = CONFIG_OTA_TEMP_PART_DEV_NAME;

	backend_spinor =
	    ota_backend_temp_part_init(ota_app_backend_callback,
				       &temp_part_init_param);
	if (!backend_spinor) {
		SYS_LOG_ERR("failed to init temp part for ota");
		return -ENODEV;
	}

	return 0;
}
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
struct ota_backend_sdcard_init_param sdcard_init_param = {
	.fpath = "SD:ota.bin",
};

struct ota_backend_sdcard_init_param usb_init_param = {
	.fpath = "USB:ota.bin",
};

int ota_app_init_sdcard(void)
{
	backend_sdcard =
	    ota_backend_sdcard_init(ota_app_backend_callback,
				    &sdcard_init_param);
	if (!backend_sdcard) {
		backend_sdcard =
		    ota_backend_sdcard_init(ota_app_backend_callback,
					    &usb_init_param);
		if (!backend_sdcard) {
			SYS_LOG_INF("failed to init sdcard ota");
			return -ENODEV;
		}
	}

	printk("ota_app_init_sdcard ok \n");
	return 0;
}
#endif

int ota_app_init(void)
{
	struct ota_upgrade_param param;

	SYS_LOG_INF("ota app init");

	memset(&param, 0x0, sizeof(struct ota_upgrade_param));

	param.storage_name = OTA_STORAGE_DEVICE_NAME;
	param.notify = ota_app_notify;
	param.flag_use_recovery = 1;
	param.flag_use_recovery_app = 1;
	param.no_version_control = 1;

	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device)
		flash_write_protection_set(flash_device, false);

	g_ota = ota_upgrade_init(&param);
	if (!g_ota) {
		SYS_LOG_INF("ota app init error");
		if (flash_device)
			flash_write_protection_set(flash_device, true);
		return -1;
	}

	if (flash_device)
		flash_write_protection_set(flash_device, true);

	return 0;
}

extern void trace_init(void);
extern void system_library_version_dump(void);

static uint8_t __ota_bss ota_data_buf[0x1000];
void main(void)
{
	struct ota_upgrade_check_param param;
	soc_watchdog_stop();

#ifdef CONFIG_USB_HOTPLUG
	usb_hotplug_host_check();
#endif

	trace_init();

	system_library_version_dump();

	SYS_LOG_INF("ota recovery main");

	ota_app_init();

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
	int need_run = ota_upgrade_is_in_progress(g_ota);
	if (!need_run) {
		SYS_LOG_INF("skip ota recovery");
		goto exit;
	}

	SYS_LOG_INF("do ota recovery");

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
	ota_app_init_spinor();
#endif
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
#ifdef CONFIG_MUTIPLE_VOLUME_MANAGER
	fs_manager_init();
#endif

	if (ota_app_init_sdcard()) {
		SYS_LOG_INF("init sdcard failed");
		goto exit;
	}
#endif
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	thread_timer_init(&mcu_timer, _ota_mcu_int_timer_hander, NULL);
	//thread_timer_start(&mcu_timer, 20, 20);
#endif
	param.data_buf = ota_data_buf;
	param.data_buf_size = sizeof(ota_data_buf);

	if (ota_upgrade_check(g_ota, &param)) {
		while (true) {
			SYS_LOG_INF
			    ("ota upgrade failed, wait check ota file \n");
			k_sleep(1000);
		}
	}

 exit:
	SYS_LOG_INF("REBOOT_TYPE_GOTO_SYSTEM");
	k_sleep(50);
	sys_pm_reboot(REBOOT_TYPE_GOTO_SYSTEM | REBOOT_REASON_OTA_FINISHED);
}
