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

void bt_manager_save_letws_info(uint8_t role,bt_addr_le_t *addr);
void bt_manager_init_letws_info(void);

int bt_mamager_letws_connected(uint16_t handle);
void bt_mamager_letws_disconnected(uint16_t handle, uint8_t role, uint8_t reason);

#endif
