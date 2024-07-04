/*
 * Copyright (c) 2023 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief pts test.
 */

#include <zephyr.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include <acts_bluetooth/pts_test.h>
#include "hci_core.h"
#include "ecc.h"
#include "keys.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "smp.h"
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/gatt.h>
#include "gatt_internal.h"
#include "common_internal.h"

uint8_t bt_is_pts_test(void)
{
	return bt_internal_is_pts_test();
}

uint8_t bt_pts_clear_keys(void * bt_addr)
{
	return acts_bt_keys_clear_by_addr(bt_addr);
}

static uint8_t s_pts_test_flags = 0;

void pts_test_set_flag(pts_test_flags_e flags)
{
	s_pts_test_flags |= flags;
}

void pts_test_clear_flag(pts_test_flags_e flags)
{
	s_pts_test_flags &= ~flags;
}

uint8_t pts_test_is_flag(pts_test_flags_e flag)
{
#ifdef CONFIG_BT_PTS_TEST
	if (flag & s_pts_test_flags) {
		return 1;
	}
#endif
	return 0;
}

uint16_t pts_test_get_attr_by_uuid(uint16_t uuid)
{
#ifdef CONFIG_BT_PTS_TEST
	return bt_gatt_get_attr_by_uuid(uuid);
#else
	return 0;
#endif
}

