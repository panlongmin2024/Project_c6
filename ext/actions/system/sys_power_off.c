/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system power off
 */

#include <os_common_api.h>
#include <app_manager.h>
#include <msg_manager.h>
#include <power_manager.h>
#include <property_manager.h>
#include <esd_manager.h>
#include <tts_manager.h>
#include <misc/util.h>
#include <string.h>
#include <soc.h>
#include <sys_event.h>
#include <sys_manager.h>
#include <sys_monitor.h>
#include <board.h>
#include <audio_hal.h>
#include <soc.h>

#ifdef CONFIG_INPUT_MANAGER
#include <input_manager.h>
#endif

#ifdef CONFIG_TTS_MANAGER
#include <tts_manager.h>
#endif

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#ifdef CONFIG_OTA_UNLOAD
#include <ota_unload.h>
#endif

#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "sys_monitor"
#include <logging/sys_log.h>
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include "common/nvram.h"
#include <wltmcu_manager_supply.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

extern void hm_ext_pa_deinit(void);
extern void external_dsp_ats3615_deinit(void);
extern int bt_mcu_send_pw_cmd_powerdown(void);
static void _system_power_callback(struct app_msg *msg, int result, void *reply)
{

	if(!msg->reserve){
#ifdef CONFIG_TASK_WDT
		task_wdt_exit();
#endif

#ifdef CONFIG_BT_MANAGER
		bt_manager_deinit();
#endif

		sys_monitor_stop();

#ifdef CONFIG_PROPERTY
		property_flush(NULL);
#endif
		system_deinit();

		k_sleep(100);//wait DSP output complete
		hm_ext_pa_deinit();
		external_dsp_ats3615_deinit();

#ifdef CONFIG_AUDIO
		hal_aout_close_pa(true);
#endif

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
	if (get_property_factory_reset_flag())
	{
		SYS_LOG_INF("property_clear_user_info\n");
		//property_factory_reset();
	}
#endif

		/** if usb charge not realy power off , goto card reader mode*/
	// if (sys_pm_get_power_5v_status() != 1) 				// logic ic changed
	{

		#ifdef BOARD_POWER_LOCK_GPIO
		board_power_lock_enable(false);
		#endif

		#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE //vcc VBATͬʱ����
		soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "poweroff");
		#ifdef CONFIG_LOGIC_MCU_LS8A10023T
		printk("[%s,%d] logic_mcu_ls8a10023t start\n", __FUNCTION__, __LINE__);
		
		struct device *logic_mcu_ls8a10023t_dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
		if (logic_mcu_ls8a10023t_dev == NULL)
		{
			printk("[%s,%d] logic_mcu_ls8a10023t_dev is null, error\n", __FUNCTION__, __LINE__);
			return;
		}
		
		logic_mcu_ls8a10023t_int_clear(logic_mcu_ls8a10023t_dev);
		logic_mcu_ls8a10023t_power_off_ready(logic_mcu_ls8a10023t_dev);

		printk("[%s,%d] logic_mcu_ls8a10023t end\n", __FUNCTION__, __LINE__);
		#endif
		printk("[%s,%d] send pd deinit message!\n", __FUNCTION__, __LINE__);
		pd_srv_sync_exit(0);
		bt_mcu_send_pw_cmd_powerdown();
		k_sleep(5*1000);
		//不会走到下面
		sys_pm_reboot(REBOOT_TYPE_NORMAL | REBOOT_REASON_NORMAL);
		#endif
		sys_pm_poweroff();
	} 
	}else{
		int reason = msg->value;

#ifdef CONFIG_TASK_WDT
		task_wdt_exit();
#endif

#ifdef CONFIG_TTS_MANAGER
		tts_manager_lock();
#endif

#ifdef CONFIG_BT_MANAGER
		bt_manager_deinit();
#endif

		sys_monitor_stop();

#ifdef CONFIG_PROPERTY
		property_flush(NULL);
#endif
		system_deinit();

		k_sleep(100);//wait DSP output complete
		hm_ext_pa_deinit();
		external_dsp_ats3615_deinit();
		
#ifdef CONFIG_OTA_UNLOAD
		printk("reboot reason %d\n", reason);
		if(reason == REBOOT_REASON_OTA_FINISHED){
			ota_unloading();
			os_sleep(500);
		}
#endif
        pd_srv_event_notify(PD_EVENT_SOURCE_BATTERY_DISPLAY,REBOOT_LED_STATUS);
		k_sleep(200);
		sys_pm_reboot(REBOOT_TYPE_NORMAL | reason);
	}
}

static void main_app_notify_system_exited(struct app_msg *recv_msg, int result, void *reply)
{
	struct app_msg msg = {0};

	msg.type = MSG_EXIT_APP;
	msg.value = recv_msg->value;
	msg.reserve = recv_msg->reserve;
	msg.callback = _system_power_callback;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);

}

static void system_exit_front_app(int reboot_type,int reboot_reason)
{
	struct app_msg msg = {0};

	msg.type = MSG_EXIT_APP;
	msg.value = reboot_reason;
	msg.reserve = reboot_type;
	msg.callback = main_app_notify_system_exited;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

}

void system_power_off(void)
{
#ifdef CONFIG_INPUT_MANAGER
	input_manager_lock();
#endif

	system_exit_front_app(0,0);
}

void system_power_reboot(int reason)
{
#ifdef CONFIG_INPUT_MANAGER
	input_manager_lock();
#endif

	system_exit_front_app(1,reason);
}

void system_power_get_reboot_reason(u16_t *reboot_type, u8_t *reason)
{
	union sys_pm_wakeup_src src = {0};

	if (!sys_pm_get_wakeup_source(&src)) {
		if (src.t.alarm) {
			*reboot_type = REBOOT_TYPE_ALARM;
			*reason = 0;
			goto exit;
		}

		if (src.t.reset) {
			*reboot_type = REBOOT_TYPE_HW_RESET;
			*reason = 0;
			goto exit;
		}

		if (src.t.onoff) {
			*reboot_type = REBOOT_TYPE_ONOFF_RESET;
			*reason = 0;
			goto exit;
		}

		if (!sys_pm_get_reboot_reason(reboot_type, reason)) {
			 	*reboot_type = REBOOT_TYPE_SF_RESET;
			 	goto exit;
		}
	}

exit:
	sys_pm_clear_wakeup_pending();
	SYS_LOG_ERR(" reboot_type: 0x%x, reason %d \n", *reboot_type, *reason);
	return;
}
