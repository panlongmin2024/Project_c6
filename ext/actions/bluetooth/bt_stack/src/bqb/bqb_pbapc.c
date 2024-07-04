/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief pbapc for bqb pbapc test.
 */
#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"

int bqb_pbapc_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
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

