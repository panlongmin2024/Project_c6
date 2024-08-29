/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app launcher
 */

#include <os_common_api.h>
#include <string.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include <led_manager.h>
#include "app_defines.h"
#include <app_launch.h>
#include "audio_system.h"
#include "desktop_manager.h"
#include <bt_manager.h>

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_STUB_DEV_USB
#include <usb/class/usb_stub.h>
#endif

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#include "app_ui.h"
#include "run_mode.h"
#include <wltmcu_manager_supply.h>

#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "launcher"
#include <logging/sys_log.h>
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif


static struct k_mutex mutex;

static int def_desktop_id = DESKTOP_PLUGIN_ID_BR_MUSIC;

/*
 * app id switch list
 */
#define app_id_list {\
						APP_ID_DESKTOP \
					}


int system_app_launch_init(void)
{
    SYS_LOG_INF("++++++++++++def_desktop_id: %d+++++++\n", def_desktop_id);
	//TODO:linein or usound hotplug
	k_mutex_init(&mutex);
#ifdef CONFIG_HM_CHARGE_WARNNING_ACTION_FROM_X4
	if(mcu_ui_get_poweron_from_charge_warnning_status())
	{
		def_desktop_id = DESKTOP_PLUGIN_ID_CHARGER;
	}
#endif
	if (run_mode_is_demo())
	{
		def_desktop_id = DESKTOP_PLUGIN_ID_DEMO;
	}

	desktop_manager_init(def_desktop_id);
	desktop_manager_add(def_desktop_id);

#ifdef CONFIG_I2SRX_IN_APP
    desktop_manager_add(DESKTOP_PLUGIN_ID_I2SRX_IN);
#endif

	return 0;
}

bool system_app_launch_add(uint8_t plugin_id, uint8_t switch_plugin)
{
	SYS_LOG_INF("%d\n", plugin_id);
	struct app_msg  msg = {0};

	msg.type = MSG_SWITCH_APP;
	if(switch_plugin){
		msg.cmd = DESKTOP_SWITCH_CURR;
	}else{
		msg.cmd = DESKTOP_SWITCH_ADD_ONLY;
	}
	msg.value = plugin_id;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("%d failed\n", plugin_id);
		return false;
	}

	return true;
}

void system_app_launch_del(uint8_t plugin_id)
{

	SYS_LOG_INF("%d", plugin_id);
	struct app_msg	msg = {0};

	msg.type = MSG_SWITCH_APP;
	msg.cmd = DESKTOP_SWITCH_LAST;
	msg.value = plugin_id;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("%d failed\n", plugin_id);
		return;
	}

	return;

}

bool system_app_launch_switch(uint8_t old, uint8_t new)
{
	SYS_LOG_INF("%d->%d", old, new);

	struct app_msg	msg = {0};

	msg.type = MSG_SWITCH_APP;
	msg.cmd = DESKTOP_SWITCH_CURR_AND_DEL;
	msg.value = new|(old<<8);

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("failed\n");
		return false;
	}
	return true;
}

void system_app_switch(void)
{
	SYS_LOG_INF("\n");

	struct app_msg	msg = {0};

	msg.type = MSG_SWITCH_APP;
	msg.cmd = DESKTOP_SWITCH_NEXT;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("failed\n");
		return;
	}
	return;
}

uint32_t system_app_get_fault_app(void)
{
	return def_desktop_id;
}

#ifdef CONFIG_AURACAST

#if !defined(CONFIG_BT_LE_AUDIO)
#define BIS_CIS_RESTRICT_ENABLE
#endif

#if defined(BIS_CIS_RESTRICT_ENABLE)
static uint8_t bis_cis_restrict = 0;
#endif


extern void bt_manager_auracast_led_display(bool status);

// [0,1,2,3,4] 0-Normal, 1-BMS, 2-BMR, 3-stereo+primary, 4-stereo+secondary
static int g_auracast_mode = 0;
void system_app_set_auracast_mode(int mode)
{
	SYS_LOG_INF("mode %d->%d", g_auracast_mode, mode);

	if(g_auracast_mode == mode){
		return;
	}
	int last_mode = g_auracast_mode;
	k_mutex_lock(&mutex, K_FOREVER);
	g_auracast_mode = mode;
	if((g_auracast_mode == 0)&&(last_mode)){
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,0);
	}else if((last_mode == 0)&&(g_auracast_mode)){
		pd_srv_event_notify(PD_EVENT_AC_LED_DISPLAY,1);
	}
	k_mutex_unlock(&mutex);

	SYS_EVENT_INF(EVENT_MAIN_AURACAST_MODE, g_auracast_mode);

	if(mode == 0) {
#if defined(BIS_CIS_RESTRICT_ENABLE)
		if(bis_cis_restrict){
			bis_cis_restrict = 0;
			bt_manager_audio_le_resume_adv();
		}
#endif
#ifdef CONFIG_BT_LETWS
		bt_mamager_letws_reconnect();
		if(last_mode == 1 || last_mode == 2){
			sys_event_notify(SYS_EVENT_PLAY_EXIT_AURACAST_TTS);
		}
#else
		//sys_event_notify(SYS_EVENT_PLAY_EXIT_AURACAST_TTS);
#endif
	} else if(mode == 1 || mode == 2){
#if defined(BIS_CIS_RESTRICT_ENABLE)
		bis_cis_restrict = 1;
		bt_manager_audio_le_pause_adv();
#endif
		bt_manager_end_pair_mode();
#ifdef CONFIG_BT_LETWS
		bt_manager_letws_stop_pair_search();
		if(!last_mode || last_mode == 3
			|| last_mode == 4){
#else
		if(!last_mode){
#endif

			bt_manager_auto_reconnect_stop();
			//sys_event_notify(SYS_EVENT_PLAY_ENTER_AURACAST_TTS);
		}
	}else if(mode == 3 || mode == 4){
#if defined(BIS_CIS_RESTRICT_ENABLE)
		bis_cis_restrict = 1;
		bt_manager_audio_le_pause_adv();
#endif
	}

    if(mode == 1){
        bt_manager_set_user_visual(true,false,false,0);
    }
    else{
        bt_manager_set_user_visual(false,false,false,0);
    }

	if (mode == 0) {
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_MULTI_POINT, NULL, 0);
	} else {
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_SINGLE_POINT, NULL, 0);
	}

#ifdef CONFIG_BT_SELF_APP
	if(last_mode == 1 || last_mode == 2 || mode == 1 || mode == 2){
		selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_ROLE_UPDATE, 0, mode);
	}
	if(last_mode == 3 || last_mode == 4 || mode == 3 || mode == 4){
		selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_LASTING_STEREO_MODE_UPDATE, 0, mode);
	}
#endif
}

/*
return [0,1,2,3,4] : 0-normal, 1-bms, 2-bmr, 3-stereo+primary, 4-stereo+secondary
*/
uint8_t system_app_get_auracast_mode(void)
{
	return g_auracast_mode;
}

static void system_app_send_input_event(u8_t cmd)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = cmd;
	msg.value = 0;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void system_app_switch_auracast(bool auracast)
{
	if (auracast) {
		system_app_send_input_event(MSG_AURACAST_ENTER);
	} else {
		system_app_send_input_event(MSG_AURACAST_EXIT);
	}
}
#endif
