/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb did test.
 */
#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"

static struct device_id_info s_did_info = {
	.vendor_id = 0x03E0,
	.product_id = 0x0001,
	.id_source = 0x0001,
};


int bqb_did_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        BT_INFO("Used: bqb sdp xx\n");
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "start")) {
        hostif_bt_did_register_sdp(&s_did_info);
    } else if (!strcmp(cmd, "stop")) {

    } else {
        printk("invalid parameters\n");
    }

    return 0;
}

