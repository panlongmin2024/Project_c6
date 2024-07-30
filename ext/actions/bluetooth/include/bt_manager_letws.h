/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager le tws.
 */
#ifndef _BTMGR_LETWS_H_
#define _BTMGR_LETWS_H_

#include "bt_manager.h"

typedef struct
{
    uint8_t dev_role;
    bt_addr_le_t addr;
} bt_mgr_saved_letws_info_t;

typedef int (*bt_letws_vnd_rx_cb)(uint16_t handle,const uint8_t *buf,uint16_t len);

struct btmgr_letws_context_t {
	uint8_t tws_role:3;
	uint8_t temp_tws_role:3;
	uint8_t le_remote_addr_valid:1;
	uint16_t tws_handle;
	bt_addr_le_t remote_ble_addr;
	bt_mgr_saved_letws_info_t info;
	struct bt_le_ext_adv *ext_adv;
	bt_letws_vnd_rx_cb rx_cb;
	os_mutex letws_mutex;
};

struct btmgr_letws_context_t *btmgr_get_letws_context(void);
void bt_manager_save_letws_info(uint8_t role,bt_addr_le_t *addr);
void bt_manager_init_letws_info(bt_letws_vnd_rx_cb cb);
void bt_manager_clear_letws_info(void);
int bt_manager_letws_get_dev_role(void);
uint16_t bt_manager_letws_get_handle(void);

int bt_mamager_letws_connected(uint16_t handle);
void bt_mamager_letws_disconnected(uint16_t handle, uint8_t role, uint8_t reason);
int bt_mamager_set_remote_ble_addr(bt_addr_le_t *addr);
void bt_manager_letws_start_pair_search(uint8_t role,int time_out_s);
int bt_mamager_letws_disconnect(int reason);
int bt_mamager_letws_reconnect(void);
void bt_manager_letws_reset(void);

#endif
