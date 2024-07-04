/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include <app_launch.h>
#include <soc_dvfs.h>
#include "app_ui.h"
#include <stream.h>
#include <thread_timer.h>
#include <property_manager.h>
#include <sys_manager.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <logic.h>
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif
#include "main_app.h"
#include "app_defines.h"
#include "app_common.h"

#ifdef CONFIG_BT_LEATWS
#include "system_le_audio.h"
#endif

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif
#include <ats_cmd/ats.h>

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static void main_app_init(void)
{
	main_app_view_init();

	system_app_launch_init();

	desktop_manager_enter();
}
extern bool run_mode_is_demo(void);
void main_msg_proc(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = { 0 };
	int result = 0;
	int exit = 0;
	int ingore_callback = 0;

	if (receive_msg(&msg, thread_timer_next_timeout())) {
		MSG_RECV_TIME_STAT_START();
		SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);
		if(!exit){
			switch (msg.type) {
#ifdef CONFIG_UI_MANAGER
		case MSG_UI_EVENT:
			ui_message_dispatch(msg.sender, msg.cmd, msg.value);
			break;
#endif
#ifdef CONFIG_INPUT_MANAGER
		case MSG_SR_INPUT:
			main_sr_input_event_handle(msg.ptr);
			break;
#endif

#ifdef CONFIG_PLAYTTS
		case MSG_TTS_EVENT:
			SYS_EVENT_INF(EVENT_MAIN_TTS,desktop_manager_get_plugin_id(),msg.value);
			desktop_manager_proc_app_msg(&msg);
			break;
#endif

		case MSG_INIT_APP:
			break;

		case MSG_BT_ENGINE_READY:
			main_app_init();
			break;

		case MSG_KEY_INPUT:
			main_key_event_handle(msg.value);
			break;

		case MSG_INPUT_EVENT:
			main_input_event_handle(&msg);
			break;
		case MSG_KEY_INPUT_ATS:
		     SYS_LOG_INF("ATS: msg.value:0x%X\n", msg.value);
				
		    struct _ats_usb_cdc_acm_thread_msg_t ats_msg = {0};
				ats_msg.type = 1;
				ats_msg.value = msg.value;
			k_msgq_put(get_ats_usb_cdc_acm_thread_msgq(), &ats_msg, K_NO_WAIT);
				break;

#ifdef CONFIG_HOTPLUG_MANAGER
		case MSG_HOTPLUG_EVENT:
			main_hotplug_event_handle(&msg);
			break;
#endif

		case MSG_SWITCH_APP:
			desktop_manager_app_switch(&msg);
			break;

		case MSG_VOLUME_CHANGED_EVENT:
			main_app_volume_show(&msg);
			break;

		//case MSG_START_APP:
		//	SYS_LOG_INF("start %s\n", (char *)msg.ptr);
		//	app_switch((char *)msg.ptr, msg.reserve, true);
		//	break;

			case MSG_EXIT_APP:
				SYS_LOG_INF("exit desktop\n");
				desktop_manager_exit();
				exit = 1;
				//app_manager_exit_app((char *)msg.ptr, true);
				break;



			default:
				desktop_manager_proc_app_msg(&msg);
				//SYS_LOG_WRN("event: %d\n", msg.type);
				break;
			}
		}

		if (msg.callback && !ingore_callback)
			msg.callback(&msg, result, NULL);

		MSG_RECV_TIME_STAT_STOP(msg.cmd, msg.type, msg.value);
	}

	thread_timer_handle_expired();
}



//#define CONFIG_ACT_EVENT_STRESS_TEST

#ifdef CONFIG_ACT_EVENT_STRESS_TEST
static void event_timer_test(struct thread_timer *timer, void* pdata)
{
	static int test_cnt;

	int i;

	for(i = 0; i < 10; i++){
		SYS_EVENT_INF(22, test_cnt++);
	}
}
#endif

void main_app(void)
{

#ifdef CONFIG_ACT_EVENT_STRESS_TEST
	struct thread_timer event_timer;

	thread_timer_init(&event_timer, event_timer_test, NULL);
	thread_timer_start(&event_timer, 0, 10);
#endif

#ifdef CONFIG_DATA_ANALY
	system_data_analy_init(1);
#endif

#ifdef CONFIG_TASK_WDT
	task_wdt_add(TASK_WDT_CHANNEL_MAIN_APP, 5000, NULL, NULL);
#endif

	while (1) {
		main_msg_proc(NULL, NULL, NULL);

#ifdef CONFIG_TASK_WDT
		task_wdt_feed(TASK_WDT_CHANNEL_MAIN_APP);
#endif
	}

}

extern char _main_stack[CONFIG_MAIN_STACK_SIZE];
APP_DEFINE(main, _main_stack, CONFIG_MAIN_STACK_SIZE,
	   APP_PRIORITY, FOREGROUND_APP, NULL, NULL, NULL, NULL, NULL);
