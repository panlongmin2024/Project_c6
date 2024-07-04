/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Shell commands for bqb test.
 */

#include <os_common_api.h>
#include <shell/shell.h>

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "bqb_gap_le.h"
#include "bqb_l2cap.h"
#include "bqb_rfcomm.h"
#include "bqb_sdp.h"
#include "bqb_spp.h"
#include "bqb_smp.h"
#include "bqb_did.h"
#include "bqb_pbapc.h"
#include "bqb_gatt.h"
#include "bqb_hfp_ag.h"
#include "bqb_hfp_hf.h"
#include "bqb_a2dp.h"

#define BQB_TEST_SHELL_MODULE "bqb"

static const struct shell_cmd bqb_test_commands[] =
{
    {"gap",    bqb_gap_test_command_handler,    "Format:bqb gap <action> [params]"},
    {"l2cap",  bqb_l2cap_test_command_handler,  "Format:bqb l2cap <action> [params]"},
    {"sdp",    bqb_sdp_test_command_handler,    "Format:bqb sdp <action> [params]"},
    {"rfcomm", bqb_rfcomm_test_command_handler, "Format:bqb rfcomm <action> [params]"},
    {"spp",    bqb_spp_test_command_handler,    "Format:bqb spp <action> [params]"},
    {"did",    bqb_did_test_command_handler,    "Format:bqb did <action> [params]"},
    {"pbpac",  bqb_pbapc_test_command_handler,  "Format:bqb pbapc <action> [params]"},

    {"gaple", bqb_gap_le_test_command_handler, "Format:bqb gap_le <action> [params]"},
    {"smp",    bqb_smp_test_command_handler,    "Format:bqb smp <action> [params]"},
    {"gatt",   bqb_gatt_test_command_handler,   "Format:bqb gatt <action> [params]"},
    {"hfp_ag", bqb_hfp_ag_test_command_handler, "Format:bqb hfp_ag <action> [params]"},
    {"hfp_hf", bqb_hfp_hf_test_command_handler, "Format:bqb hfp_hf <action> [params]"},
    {"a2dp",   bqb_a2dp_test_command_handler,   "Format:bqb a2dp <action> [params]"},
    {"utl",    bqb_utils_command_handler,       "Format:bqb utl <action> [params]"},
    /*end*/
    {NULL, NULL, NULL}

};

SHELL_REGISTER(BQB_TEST_SHELL_MODULE, bqb_test_commands);

