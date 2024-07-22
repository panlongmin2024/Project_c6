/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt le audio access policy manager.
*/

#ifndef __BT_MANAGER_LEA_POLICY_H__
#define __BT_MANAGER_LEA_POLICY_H__

/**
 * @defgroup bt_manager_lea_policy_apis Bt Manager LE Audio policy APIs
 * @ingroup bluetooth_system_apis
 * @{
 */

typedef enum {
	LEA_POLICY_EVENT_BT_OFF = 0,
	LEA_POLICY_EVENT_BT_ON,
	LEA_POLICY_EVENT_CONNECT,
	LEA_POLICY_EVENT_DISCONNECT,
	LEA_POLICY_EVENT_PAIRING,
	LEA_POLICY_EVENT_EXIT_PAIRING,
	LEA_POLICY_EVENT_SWITCH_SINGLE_POINT,
	LEA_POLICY_EVENT_SWITCH_MULTI_POINT,

	LEA_POLICY_EVENT_MAX_NUM,
} btmgr_lea_policy_event_e;

typedef enum {
	ADV_TYPE_LEGENCY = 0,
	ADV_TYPE_EXT = 1,
} btmgr_lea_policy_adv_type_e;

/**
* @brief bt manager lea policy event notify
*
* Event notify for le audio access policy
*/
void bt_manager_lea_policy_event_notify(btmgr_lea_policy_event_e event, void *param, uint8_t len);

/**
* @brief bt manager lea policy event notify
*
* Adv param init for le audio access policy
*/
struct bt_le_adv_param * bt_manager_lea_policy_get_adv_param(uint8_t adv_type, struct bt_le_adv_param* param_buf);

bool bt_manager_lea_is_reconnected_dev(uint8_t *addr);

void bt_manager_lea_clear_paired_list(void);

uint8_t bt_manager_lea_set_active_device(uint16_t handle);

void bt_manager_lea_policy_disable(uint8_t adv_enable);

void bt_manager_lea_policy_enable(void);

void bt_manager_lea_policy_ctx_init(void);

#endif /*__BT_MANAGER_LEA_POLICY_H__*/

