/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/util.h>
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_monitor.h>
#include <sys_wakelock.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <input_manager.h>
#include <sys_manager.h>
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#include <bt_manager.h>
#include "main_app.h"
#include "broadcast.h"

#ifdef CONFIG_INPUT_MANAGER


void main_sr_input_event_handle(void *value)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_SR_INPUT;
	msg.value = *(int *)value;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void main_key_event_handle(u32_t key_event)
{
	/**input event means esd proecess finished*/
#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
#ifdef CONFIG_PLAYTTS
		tts_manager_unlock();
#endif
		esd_manager_reset_finished();
	}
#endif

	sys_wake_lock(WAKELOCK_INPUT);

#ifdef CONFIG_PLAYTTS
	if ((key_event & KEY_TYPE_DOUBLE_CLICK) == KEY_TYPE_DOUBLE_CLICK
	    || (key_event & KEY_TYPE_TRIPLE_CLICK) == KEY_TYPE_TRIPLE_CLICK
	    || (key_event & KEY_TYPE_SHORT_UP) == KEY_TYPE_SHORT_UP
	    || (key_event & KEY_TYPE_LONG_DOWN) == KEY_TYPE_LONG_DOWN) {
		//tts_manager_stop(NULL);
	}
#endif

	SYS_LOG_INF("Totti key 0x%x\n", key_event);

	/**drop fisrt key event when resume*/
	if (system_wakeup_time() > 600) {

#ifdef CONFIG_DATA_ANALY
		extern u8_t app_count_key_press_cnt(u32_t key_event);
		app_count_key_press_cnt(key_event);
#endif

#ifdef CONFIG_BT_LETWS
		uint8_t temp_role = bt_manager_letws_get_dev_role();
		if(temp_role == BTSRV_TWS_SLAVE){
			broadcast_tws_vnd_send_key(key_event);
		}else
#endif
		{
#ifdef CONFIG_UI_MANAGER
			ui_manager_dispatch_key_event(key_event);
#endif
		}
	} else {
		SYS_LOG_INF("drop key workup %d \n", system_wakeup_time());
	}

	sys_wake_unlock(WAKELOCK_INPUT);
}
#endif


