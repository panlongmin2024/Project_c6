/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bqb gap test
 */

#include <os_common_api.h>


#define BQB_GAP_BT_DEFAULT_COD     0x240404 /* Rendering,Audio, Audio/Video, Wearable Headset Device */

#define BQB_GAP_INQUIRY_MODE_NONE      0x00
#define BQB_GAP_INQUIRY_MODE_GENERAL   0x01
#define BQB_GAP_INQUIRY_MODE_LIMITED   0x02
#define BQB_GAP_INQUIRY_MODE_INVALID   0xFF
typedef uint8_t bqb_gap_inquiry_mode_t;

#define BQB_GAP_DISC_MODE_NONE        0x00
#define BQB_GAP_DISC_MODE_GENERAL     0x01
#define BQB_GAP_DISC_MODE_LIMITED     0x02
#define BQB_GAP_DISC_MODE_INVALID     0xFF
typedef uint8_t bqb_gap_discovery_mode_t;

#define BQB_GAP_NO_SCANS                  0x00
#define BQB_GAP_INQUIRY_SCAN_ONLY         0x01
#define BQB_GAP_PAGE_SCAN_ONLY            0x02
#define BQB_GAP_BOTH_INQUIRY_PAGE_SCAN    0x03
#define BQB_GAP_SCAN_MODE_INVALID         0xFF
typedef uint8_t bqb_gap_scan_mode_t;

void bqb_gap_write_scan_enable(bqb_gap_scan_mode_t mode);
struct bt_conn *bqb_gap_connect_br_acl(char *str_addr);
int bqb_gap_disconnect_acl(struct bt_conn *conn);
void bqb_gap_auth_register(void);

void bqb_gap_clean_linkkey(void);
int bqb_gap_test_command_handler(int argc, char *argv[]);

