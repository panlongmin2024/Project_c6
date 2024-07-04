/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb sdp test.
 */
#include <logging/sys_log.h>
#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"
#include "bqb_sdp.h"

int bqb_sdp_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        BT_INFO("Used: bqb sdp xx\n");
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "start")) {

    } else if (!strcmp(cmd, "stop")) {

    } else {
        printk("invalid parameters\n");
    }

    return 0;
}

