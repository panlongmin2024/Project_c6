/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb core.
 */

#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"
#include "bqb_gap.h"


static void bqb_core_int_reay(int err)
{
    if (err) {
        BT_ERR("bqb core init failed %d\n", err);
        return;
    }

    bqb_utils_read_public_addr(NULL);
    BT_INFO("bqb core init done.\n");
}

void bqb_core_init(void)
{
    hostif_bt_init_class(BQB_GAP_BT_DEFAULT_COD);
    //hostif_bt_init_device_id(did);
    hostif_bt_enable(bqb_core_int_reay);
}

void bqb_core_deinit(void)
{



}

