/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
 */
//#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <btservice_api.h>
#include <soc_dvfs.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>
#include <thread_timer.h>
#include "ctrl_interface.h"

static bd_address_t ota_dev_addr;

void bt_manager_ota_set_connected_dev(const struct bt_conn *conn)
{
	uint16_t hdl;
	struct bt_mgr_dev_info *dev_info;

	if (!conn) {
		return;
	}
	memset(&ota_dev_addr, 0, sizeof(bd_address_t));

	hdl = hostif_bt_conn_get_handle(conn);
	if (hdl) {
		dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
		if (dev_info) {
			SYS_LOG_INF("");
			memcpy(&ota_dev_addr, &dev_info->addr, sizeof(bd_address_t));
		}
	}
}

int bt_manager_ota_get_connected_dev(void *value, int value_len)
{
	if ((!value) || (!value_len)) {
		return -1;
	}

	return property_get(CFG_OTA_MAC, value, value_len);
}

void bt_manager_ota_save_connected_dev(void)
{
	SYS_LOG_INF("");
	property_set(CFG_OTA_MAC, (void *)&ota_dev_addr, sizeof(bd_address_t));
}

void bt_manager_ota_clr_connected_dev(void)
{
	SYS_LOG_INF("");
	memset(&ota_dev_addr, 0, sizeof(bd_address_t));
	property_set(CFG_OTA_MAC, (void *)&ota_dev_addr, sizeof(bd_address_t));
}
