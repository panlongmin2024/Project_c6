/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app main
 */
#include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include "app_ui.h"
#include <bt_manager.h>
#include <hotplug_manager.h>
#include <input_manager.h>
#include <thread_timer.h>
#include <stream.h>
#include <property_manager.h>
#include <usb/usb_device.h>
#include <usb/class/usb_msc.h>
#include <soc.h>
#include <ui_manager.h>
#include <stub_hal.h>
#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#include "charger.h"
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
#include <run_mode/run_mode.h>
#include <wltmcu_manager_supply.h>
#include <power_manager.h>
#ifdef CONFIG_C_EXTERNAL_DSP_ATS3615
#include <external_dsp/ats3615/ats3615_head.h>
#endif
#ifdef CONFIG_C_AMP_TAS5828M
#include <driver/amp/tas5828m/tas5828m_head.h>
#endif
#endif

// #ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
// #endif

#if 0
//os_mutex charge_mutex;
static void _charge_event_nodify_callback(struct app_msg *msg, int result, void *reply)
{
	if (msg->sync_sem) {
		os_sem_give((os_sem *)msg->sync_sem);
	}
}

static void _charge_event_nodify(uint8_t type, uint8_t sync_mode)
{
	struct app_msg msg = {0};

	msg.type = type;
	msg.cmd = 0;
	if (sync_mode) {

		os_sem return_notify;

		os_sem_init(&return_notify, 0, 1);
		msg.callback = _charge_event_nodify_callback;
		msg.sync_sem = &return_notify;
		if (!send_async_msg(CONFIG_SYS_APP_NAME, &msg)) {
			return;
		}

		//os_mutex_unlock(&charge_mutex);
		if (os_sem_take(&return_notify, OS_FOREVER) != 0) {
			return;
		}
		//os_mutex_lock(&charge_mutex, OS_FOREVER);
	} else {
		if (!send_async_msg(CONFIG_SYS_APP_NAME, &msg)) {
			return;
		}
	}

}
#endif
extern int check_is_wait_adfu(void);
int charger_mode_check(void)
{
	struct app_msg msg = {0};
	//struct app_msg charger_mode_msg = {0};
	int result = 0;
	bool terminaltion = false;
#ifdef CONFIG_BT_CONTROLER_BQB
	extern bool main_system_is_enter_bqb(void);
#endif	
	if((run_mode_is_demo() == true) || (check_is_wait_adfu())
#ifdef CONFIG_BT_CONTROLER_BQB	
	|| (main_system_is_enter_bqb())
#endif	
	)
	{
/* 		charger_mode_msg.type = MSG_CHARGER_MODE;
		charger_mode_msg.cmd = 0;
		send_async_msg(CONFIG_SYS_APP_NAME, &charger_mode_msg); */
		led_manager_set_display(128,LED_ON,OS_FOREVER,NULL);
		// pd_set_app_mode_state(BTMODE_APP_MODE);
		return 0;
	}
#ifdef CONFIG_DATA_ANALY
	system_data_analy_init(0);
#endif

	charger_view_init();
	input_manager_unlock();
	
    pd_set_app_mode_state(CHARGING_APP_MODE);
	
	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			switch (msg.type) {
			case MSG_KEY_INPUT:

/* 				terminaltion = true;

				charger_mode_msg.type = MSG_CHARGER_MODE;
				charger_mode_msg.cmd = 0;

				send_async_msg(CONFIG_SYS_APP_NAME, &charger_mode_msg); */
				break;
		#ifdef CONFIG_UI_MANAGER
			case MSG_UI_EVENT:
				ui_message_dispatch(msg.sender, msg.cmd, msg.value);
				break;
		#endif
			case MSG_HOTPLUG_EVENT:
				if (msg.value == HOTPLUG_OUT) {
					switch (msg.cmd) {
						case HOTPLUG_CHARGER:
							if(!pd_manager_get_source_change_state())
							{

								terminaltion = true;

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
							//extern int pd_manager_deinit(void);
								pd_manager_deinit(0);
#endif							
								sys_pm_poweroff();
							}
							break;
						default :
							break;
					}
				}
				break;


			case MSG_EXIT_APP:
				if(power_manager_get_battery_capacity() > 5)
				{
					terminaltion = true;
					SYS_LOG_INF("%d MSG_EXIT_APP\n",__LINE__);
				}
				break;
			case MSG_POWER_KEY:
				{
					terminaltion = true;
					SYS_LOG_INF("%d MSG_POWER_KEY\n",__LINE__);
				}
				break;

			case MSG_PD_OTG_MOBILE_EVENT:
				terminaltion = true;
				SYS_LOG_INF("%d MSG_PD_OTG_MOBILE_EVENT\n",__LINE__);


				// struct device *logic_mcu_ls8a10023t_dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
				// if (logic_mcu_ls8a10023t_dev)
				// {
extern int logic_mcu_ls8a10023t_otg_mobile_det(void);
				logic_mcu_ls8a10023t_otg_mobile_det();
			
//				}
		
#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
				//extern int pd_manager_deinit(void);
				pd_manager_deinit(1);
#endif							
				sys_pm_poweroff();
				
				break;	

			default:
				SYS_LOG_ERR("error type: 0x%x! \n", msg.type);
				continue;
			}

			if (msg.callback != NULL)
				msg.callback(&msg, result, NULL);
		}
		thread_timer_handle_expired();
	}

 //   wait_pd_init_finish();

#ifdef CONFIG_BUILD_PROJECT_HM_DEMAND_CODE
    pd_set_app_mode_state(BTMODE_APP_MODE);
	// pd_set_source_refrest();
#endif

	charger_view_deinit();
/* 	msg.type = MSG_CHARGER_MODE;
	msg.cmd = 0;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg); */
	return 0;

}
