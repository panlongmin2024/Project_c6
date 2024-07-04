/** @file
 *  @brief GATT TX Power Service
 */

/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/types.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/hci.h>

#define LOG_LEVEL CONFIG_BT_TPS_LOG_LEVEL
#define LOG_MODULE_NAME bt_tps
#include "common/log.h"

static ssize_t read_tx_power_level(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr, void *buf,
				   uint16_t len, uint16_t offset)
{
	struct bt_conn_le_tx_power tx_power_level = {0};

	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
#if 0
	int err;
	err = bt_conn_le_get_tx_power_level(conn, &tx_power_level);
	if (err) {
		SYS_LOG_ERR("Failed to read Tx Power Level over HCI: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
#else
	tx_power_level.current_level = 8;
#endif

	SYS_LOG_INF("TPS Tx Power Level read %d", tx_power_level.current_level);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &tx_power_level.current_level,
				 sizeof(tx_power_level.current_level));
}

static struct bt_gatt_attr tps_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_TPS),
	BT_GATT_CHARACTERISTIC(BT_UUID_TPS_TX_POWER_LEVEL, 
	               BT_GATT_CHRC_READ,
                   BT_GATT_PERM_READ, read_tx_power_level, NULL,NULL),
};

static struct bt_gatt_service tps_svc = BT_GATT_SERVICE(tps_attrs);

int bt_register_tps_svc(void)
{
	return bt_gatt_service_register(&tps_svc);
}

