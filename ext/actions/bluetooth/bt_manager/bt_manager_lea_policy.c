/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This is the abstraction layer provided for Application layer for
 * Bluetooth le audio access policy.
 */

#include <bt_manager.h>
#include <mem_manager.h>
#include <thread_timer.h>
#include "bt_manager_lea_policy.h"
#include "bt_manager_inner.h"
#include "btservice_api.h"
#include <acts_bluetooth/addr.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/host_interface.h>
#include "ctrl_interface.h"

#include <hex_str.h>
#include <sys_event.h>
#include <app_manager.h>

/*bt_manager_get_pair_config()->pair_mode_duration_sec = 180s?*/
#define PAIRING_DURATION_MS         ((bt_manager_get_pair_config()->pair_mode_duration_sec) * (1000))
#define RECONNECTING_DURATION_MS    ((3) * (1000))//BT_RECONNECT_PHONE_TIMEOUT

typedef int (* lea_policy_state_event_handle_func)(btmgr_lea_policy_event_e event, void *param, uint8_t len);

/* state of le audio access policy*/
typedef enum {
	LEA_POLICY_STATE_WAITING = 0,
	LEA_POLICY_STATE_PAIRING,
	LEA_POLICY_STATE_RECONNECTING,
	LEA_POLICY_STATE_CONNECTED,
	LEA_POLICY_STATE_MAX_NUM,
} btmgr_lea_policy_state_e;

/* structure of le audio access policy*/
typedef struct {
	uint8_t max_dev_num:5;
	uint8_t btoff : 1;
	uint8_t policy_enable : 1;
	btmgr_lea_policy_state_e lea_policy_state;
	uint32_t option;
	lea_policy_state_event_handle_func *funcs;
	bt_addr_le_t last_lea_dev_info;
	struct thread_timer lea_policy_timer;
} btmgr_dev_access_policy_t;

typedef struct {
	uint32_t event;
	void *param;
	uint8_t len;
} lea_policy_event_param_t;

static int lea_policy_state_waiting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_pairing_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_connected_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);

static btmgr_dev_access_policy_t btmgr_lea_policy_ctx = {0};
static lea_policy_state_event_handle_func lea_policy_state_event_handler[LEA_POLICY_STATE_MAX_NUM] =
{
	lea_policy_state_waiting_handler,
	lea_policy_state_pairing_handler,
	lea_policy_state_reconnecting_handler,
	lea_policy_state_connected_handler,
};

static void timeout_handler (
					struct thread_timer *ttimer,
				    void *expiry_fn_arg)
{
	if (!btmgr_lea_policy_ctx.policy_enable) {
		return;
	}

	uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();

	if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
		thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
	}

	bt_manager_halt_ble();
#if defined(CONFIG_BT_PRIVACY)
	if (btmgr_lea_policy_ctx.lea_policy_state == LEA_POLICY_STATE_PAIRING) {
		hostif_bt_le_address_resolution_enable(1);
	}
#endif

	if (cur_lea_num < btmgr_lea_policy_ctx.max_dev_num) {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_WAITING;
		bt_manager_resume_ble();
		bt_manager_le_audio_adv_enable();
	} else {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
	}

	SYS_LOG_INF("state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
}

static void lea_policy_timer_start(int32_t time_duration)
{
	if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
		thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
	}
	thread_timer_init(&btmgr_lea_policy_ctx.lea_policy_timer, timeout_handler, NULL);
	thread_timer_start(&btmgr_lea_policy_ctx.lea_policy_timer, time_duration, 0);
	SYS_LOG_INF("time:%d\n", time_duration);
}

static void enter_pairing_mode(void)
{
#if defined(CONFIG_BT_PRIVACY)
	hostif_bt_le_address_resolution_enable(0);
#endif
	lea_policy_timer_start(PAIRING_DURATION_MS);
	btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_PAIRING;
	bt_manager_resume_ble();
	bt_manager_le_audio_adv_enable();
}

static int lea_policy_state_waiting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				/* switch state, and stop adv */
				btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
				bt_manager_halt_ble();
				break;
			}
			/*check if need update adv data*/
			struct bt_audio_config * cfg = btif_audio_get_cfg_param();
			if (cfg->target_announcement) {
				cfg->target_announcement = 0;
				bt_manager_le_update_adv_data(cfg);
				bt_manager_halt_ble();
				bt_manager_resume_ble();
				bt_manager_le_audio_adv_enable();
			}

			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}
			uint8_t reason = ((uint8_t *)param)[0];
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				break;
			} else {
				/* reconnect last device */
				bt_manager_halt_ble();
				lea_policy_timer_start(RECONNECTING_DURATION_MS);

				btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_RECONNECTING;

				bt_manager_resume_ble();
				bt_manager_le_audio_adv_enable();
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			/* update adv */
			bt_manager_halt_ble();
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num == 0) {
				/* do nothing*/
				return 0;
			}

			/* switch state, and stop adv */
			btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
			bt_manager_halt_ble();

			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			/* do nothing, keep waiting */
			break;
		}

		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_pairing_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			if (param) {
				memcpy(&btmgr_lea_policy_ctx.last_lea_dev_info, param, sizeof(bt_addr_le_t));
			}

			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();

			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				/* switch state, and stop pairing */
				btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
				bt_manager_halt_ble();

				if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
					thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
#if defined(CONFIG_BT_PRIVACY)
					hostif_bt_le_address_resolution_enable(1);
#endif
				}
				break;
			}
			/*check if need update adv data*/
			struct bt_audio_config * cfg = btif_audio_get_cfg_param();
			if (cfg->target_announcement) {
				cfg->target_announcement = 0;
				bt_manager_le_update_adv_data(cfg);
				bt_manager_halt_ble();
				bt_manager_resume_ble();
				bt_manager_le_audio_adv_enable();
			}

			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			/* update pairing */
			lea_policy_timer_start(PAIRING_DURATION_MS);

			break;
		}
		case LEA_POLICY_EVENT_EXIT_PAIRING:
		{
			/* exit pairing */
			/*check if need update adv data*/
			struct bt_audio_config * cfg = btif_audio_get_cfg_param();
			if (cfg->target_announcement) {
				cfg->target_announcement = 0;
				bt_manager_le_update_adv_data(cfg);
			}

			timeout_handler(NULL,NULL);
			break;
		}

		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num == 0) {
				/* do nothing*/
				return 0;
			}

			/* switch state, and stop pairing */
			btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
			bt_manager_halt_ble();

			if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
				thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
#if defined(CONFIG_BT_PRIVACY)
				hostif_bt_le_address_resolution_enable(1);
#endif
			}

			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			/* do nothing, keep pairing */
			break;
		}

		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}
	return 0;
}

static int lea_policy_state_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				/* switch state, and stop adv */
				btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
				if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
					thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
				}

				bt_manager_halt_ble();
			}

			/* reconnect last device */
			uint8_t expected_len = sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}
			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}

			if (!bt_addr_le_cmp((const bt_addr_le_t*)param, &btmgr_lea_policy_ctx.last_lea_dev_info)) {
				timeout_handler(NULL, NULL);
			}
			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			/* reconnect last device */
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}
			uint8_t reason = ((uint8_t *)param)[0];
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				break;
			} else {
				/* reconnect last device */
				bt_manager_halt_ble();
				lea_policy_timer_start(RECONNECTING_DURATION_MS);

				btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_RECONNECTING;
				bt_manager_resume_ble();
				bt_manager_le_audio_adv_enable();
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			/* update adv */
			bt_manager_halt_ble();
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num == 0) {
				/* do nothing*/
				return 0;
			}

			/* switch state, and stop pairing */
			btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
			if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
				thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
			}
			bt_manager_halt_ble();

			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			/* do nothing, keep reconnecting */
			break;
		}

		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_connected_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				break;
			}

			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (expected_len == len) {
				uint8_t reason = ((uint8_t *)param)[0];
				bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
				memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
				if (0x13 == reason
					|| 0x16 == reason) {
					/* enable adv */
					btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_WAITING;
				} else {
					/* reconnect last device */
					lea_policy_timer_start(RECONNECTING_DURATION_MS);
					btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_RECONNECTING;
				}
				bt_manager_resume_ble();
				bt_manager_le_audio_adv_enable();
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			/* update adv */
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_WAITING;

			bt_manager_resume_ble();
			bt_manager_le_audio_adv_enable();

			break;
		}

		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

void bt_manager_lea_policy_ctx_init(void)
{
	memset(&btmgr_lea_policy_ctx, 0, sizeof(btmgr_lea_policy_ctx));
	btmgr_lea_policy_ctx.policy_enable = 1;

	btmgr_lea_policy_ctx.funcs = lea_policy_state_event_handler;
	btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();

	bool ret = bt_le_get_last_paired_device(&btmgr_lea_policy_ctx.last_lea_dev_info);
	if (true == ret) {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_WAITING;
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(&btmgr_lea_policy_ctx.last_lea_dev_info, addr_str, sizeof(addr_str));
		SYS_LOG_INF("addr:%s\n", addr_str);
	} else {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_PAIRING;
	}

	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_BT_ON, NULL, 0);
}

static void lea_policy_bt_on_handler(void)
{
	if (btmgr_lea_policy_ctx.lea_policy_state == LEA_POLICY_STATE_PAIRING) {
#if defined(CONFIG_BT_PRIVACY)
		hostif_bt_le_address_resolution_enable(0);
#endif
		thread_timer_init(&btmgr_lea_policy_ctx.lea_policy_timer, timeout_handler, NULL);
		thread_timer_start(&btmgr_lea_policy_ctx.lea_policy_timer, PAIRING_DURATION_MS, 0);
	}
	SYS_LOG_INF("state:%d,max dev:%d\n",
	btmgr_lea_policy_ctx.lea_policy_state,
	btmgr_lea_policy_ctx.max_dev_num);
}

struct bt_le_adv_param * bt_manager_lea_policy_get_adv_param(uint8_t adv_type, struct bt_le_adv_param* param_buf)
{
	struct bt_le_adv_param *param = param_buf;
	uint32_t last_options = btmgr_lea_policy_ctx.option;
	btmgr_lea_policy_state_e lea_policy_state = btmgr_lea_policy_ctx.lea_policy_state;

	if (!param){
		SYS_LOG_ERR("param buff is null\n");
		return NULL;
	}

	memset(param_buf, 0, sizeof(struct bt_le_adv_param));

    //adv Common param
	param->id = BT_ID_DEFAULT;
	param->interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param->interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param->options = BT_LE_ADV_OPT_CONNECTABLE;

	if (ADV_TYPE_EXT == adv_type) {
		param->options |= (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_NO_2M);
		param->sid = BT_EXT_ADV_SID_UNICAST;
	}

	if (!btmgr_lea_policy_ctx.policy_enable) {
#ifdef CONFIG_BT_PRIVACY
		param->options |= BT_LE_ADV_OPT_ADDR_RPA;
#endif

		return param;
	}

    //adv param config by state
	switch (lea_policy_state) {
		case LEA_POLICY_STATE_PAIRING: /*Undirect connectable Host-gen rpa adv*/
		{
#ifdef CONFIG_BT_PRIVACY
			param->options |= BT_LE_ADV_OPT_ADDR_RPA;
#endif
		}
		break;

		case LEA_POLICY_STATE_RECONNECTING: /*Direct connectable Controller-gen rpa adv*/
		{
			if (ADV_TYPE_EXT != adv_type) {
				//SYS_LOG_ERR("reconnecting should send ext ADV\n");
				return NULL;
			}

			param->options |= (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY);
#ifdef CONFIG_BT_WHITELIST
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
#endif
#ifdef CONFIG_BT_PRIVACY
			param->options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
#endif
			param->peer = &btmgr_lea_policy_ctx.last_lea_dev_info;
		}
		break;

		default: /*Undirect connectable Controller-gen rpa adv*/
		{
			param->options |= BT_LE_ADV_OPT_USE_IDENTITY;
#ifdef CONFIG_BT_WHITELIST
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
#endif
#ifdef CONFIG_BT_PRIVACY
			param->options |= BT_LE_ADV_OPT_ADDR_RPA;
			param->peer = &btmgr_lea_policy_ctx.last_lea_dev_info;
#endif
		}
		break;
	}

	if (last_options != param->options) {
		SYS_LOG_INF("op chg:%x->%x\n", last_options, param->options);
		btmgr_lea_policy_ctx.option = param->options;
	}

	if (param->peer != NULL && (ADV_TYPE_EXT == adv_type)) {
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(param->peer, addr_str, sizeof(addr_str));
		SYS_LOG_INF("addr:%s\n", addr_str);
	}

	return param;
}

bool bt_manager_lea_is_reconnected_dev(uint8_t *addr)
{
	if (btmgr_lea_policy_ctx.lea_policy_state == LEA_POLICY_STATE_RECONNECTING
		&& addr
		&& !memcmp(addr, btmgr_lea_policy_ctx.last_lea_dev_info.a.val, 6)) {
		SYS_LOG_INF("reconnect dev\n");
		return true;
	}

	return false;
}

void bt_manager_lea_clear_paired_list(void)
{
	if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
		thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
	}
	bt_manager_halt_ble();
	hostif_bt_le_clear_list();
}

uint8_t bt_manager_lea_set_active_device(uint16_t handle)
{
	return ctrl_cis_set_active_device(handle);
}

static void bt_manager_lea_policy_event_cb(void *event_param)
{
	lea_policy_event_param_t *ptr = (lea_policy_event_param_t *)event_param;
	if (!btmgr_lea_policy_ctx.policy_enable) {
		if (ptr->param) {
			mem_free(ptr->param);
		}
		return;
	}

	SYS_LOG_INF("evt:%d,state:%d\n", ptr->event, btmgr_lea_policy_ctx.lea_policy_state);
	switch (ptr->event) {
		case LEA_POLICY_EVENT_BT_OFF:
		{
			/* Disable discoverable/connectable, stop pair, stop reconnect. */
			if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
				thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
			}
			bt_manager_halt_ble();
			bt_manager_ble_disconnect_all_link();
			btmgr_lea_policy_ctx.btoff = 1;
			break;
		}
		case LEA_POLICY_EVENT_BT_ON:
		{
			lea_policy_bt_on_handler();
			break;
		}

		default:
			if (btmgr_lea_policy_ctx.funcs && !btmgr_lea_policy_ctx.btoff) {
				btmgr_lea_policy_ctx.funcs[btmgr_lea_policy_ctx.lea_policy_state]
					(
						ptr->event,
						ptr->param,
						ptr->len
					);
			}
			break;
	}

	if (btmgr_lea_policy_ctx.max_dev_num == 1
		&& bt_manager_audio_get_leaudio_dev_num()
		&& !btmgr_lea_policy_ctx.btoff) {
		uint8_t connectable = 0;
		uint8_t bt_link_num = bt_manager_get_connected_dev_num();
		connectable = (bt_link_num == 0) ? 1 : 0;
		/* notify le audio to handle BT scan mode update */
		SYS_LOG_INF("bt connectable:%d\n", connectable);
		bt_manager_event_notify(BT_AUDIO_UPDATE_SCAN_MODE, &connectable, sizeof(connectable));
	}

	if (ptr->param) {
		mem_free(ptr->param);
	}

	SYS_LOG_INF("state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
}

void bt_manager_lea_policy_event_notify(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	if (!btmgr_lea_policy_ctx.policy_enable) {
		return;
	}
	if (event >= LEA_POLICY_EVENT_MAX_NUM) {
		SYS_LOG_ERR("evt:%d\n", event);
		return;
	}
	void *param_ptr = NULL;
	int ret = 0;

	if (len) {
		param_ptr = mem_malloc(len);
		if (param_ptr == NULL) {
			SYS_LOG_ERR("alloc failed\n");
			return;
		}
		memcpy(param_ptr, param, len);
	}

	lea_policy_event_param_t evt_param = {0};
	evt_param.event = event;
	evt_param.param = param_ptr;
	evt_param.len = len;

	ret = btif_audio_cb_event_notify(bt_manager_lea_policy_event_cb, &evt_param, sizeof(evt_param));
	if (ret && param_ptr) {
		SYS_LOG_ERR("notify send failed\n");
		mem_free(param_ptr);
	}

	SYS_LOG_DBG("evt:%d,ret:%d\n", event, ret);
}

void bt_manager_lea_policy_disable(uint8_t adv_enable)
{
	btmgr_lea_policy_ctx.policy_enable = 0;
	bt_manager_le_audio_adv_disable();

	if (adv_enable) {
#if defined(CONFIG_BT_PRIVACY)
		hostif_bt_le_address_resolution_enable(0);
#endif
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_PAIRING;
		bt_manager_le_audio_adv_enable();
	} else {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
	}
}

void bt_manager_lea_policy_enable(void)
{
	btmgr_lea_policy_ctx.policy_enable = 1;
	uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
	bt_manager_halt_ble();
#if defined(CONFIG_BT_PRIVACY)
	if (btmgr_lea_policy_ctx.lea_policy_state == LEA_POLICY_STATE_PAIRING) {
		hostif_bt_le_address_resolution_enable(1);
	}
#endif

	if (cur_lea_num < btmgr_lea_policy_ctx.max_dev_num) {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_WAITING;
		bt_manager_resume_ble();
		bt_manager_le_audio_adv_enable();
	} else {
		btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_CONNECTED;
	}

	SYS_LOG_INF("state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);

}


