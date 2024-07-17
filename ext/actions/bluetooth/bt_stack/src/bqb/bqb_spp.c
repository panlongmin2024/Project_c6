/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb spp test.
 */
#include <acts_bluetooth/host_interface.h>

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "bqb_spp.h"

static void bqb_spp_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_spp_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static void bqb_spp_connect_fail_cb(struct bt_conn *conn, uint8_t spp_id);
static void bqb_spp_connected_cb(struct bt_conn *conn, uint8_t spp_id);
static void bqb_spp_disconnected_cb(struct bt_conn *conn, uint8_t spp_id);
static void bqb_spp_recv_cb(struct bt_conn *conn, uint8_t spp_id, uint8_t *data, uint16_t len);
static void bqb_spp_passkey_display_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_spp_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey);

typedef struct bqb_spp_context {
    struct bt_conn *conn;
    uint8_t spp_id;
} bqb_spp_context_t;

static const struct bt_conn_auth_cb bqb_spp_auth_display_yesno_cbs = {
    .passkey_display = bqb_spp_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = bqb_spp_passkey_confirm_cb,
    .pincode_entry = NULL,
    .cancel = NULL,
    .pairing_confirm = NULL,
};

static const uint8_t bqb_spp_uuid[16] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00};

static bqb_spp_context_t g_bqb_spp_cntx = {0};

static struct bt_conn_cb bqb_spp_acl_conn_cbs = {
    .connected = bqb_spp_acl_connected_cb,
    .disconnected = bqb_spp_acl_disconnected_cb,
};

static struct bt_spp_app_cb bqb_spp_cbs = {
    .connect_failed = bqb_spp_connect_fail_cb,
    .connected = bqb_spp_connected_cb,
    .disconnected = bqb_spp_disconnected_cb,
    .recv = bqb_spp_recv_cb,
};

static void bqb_spp_passkey_display_cb(struct bt_conn *conn, unsigned int passkey)
{
    if (conn != g_bqb_spp_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_spp_cntx.conn, conn);
    }
}

static void bqb_spp_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey)
{
    if (conn != g_bqb_spp_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", g_bqb_spp_cntx.conn, conn);
        return;
    }
	int ret = bt_conn_auth_passkey_confirm(g_bqb_spp_cntx.conn);
    BT_INFO("passkey cfm ret: %d\n", ret);
}


static void bqb_spp_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    if (0 == err) {
        /* Spp connect */
        uint8_t connect_uuid[16];
        for (uint8_t i = 0; i < 16; i++) {
            connect_uuid[i] = bqb_spp_uuid[15 - i];
        }
        g_bqb_spp_cntx.spp_id = hostif_bt_spp_connect(g_bqb_spp_cntx.conn, connect_uuid);
        BT_INFO("bqb spp conn spp_id:%d\n", g_bqb_spp_cntx.spp_id);
    } else {
        BT_ERR("bqb spp connect acl fail, err:%d\n", err);
        g_bqb_spp_cntx.conn = NULL;
    }
}

static void bqb_spp_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("bqb spp acl disconnected, reason:%d\n", reason);
    g_bqb_spp_cntx.conn = NULL;
}

static void bqb_spp_connect_fail_cb(struct bt_conn *conn, uint8_t spp_id)
{
    BT_INFO("bqb spp connect fail: %p, %d\n", conn, spp_id);
}

static void bqb_spp_connected_cb(struct bt_conn *conn, uint8_t spp_id)
{
    BT_INFO("bqb spp connected cb: %p, %d\n", conn, spp_id);

    if (g_bqb_spp_cntx.spp_id == spp_id) {
        BT_INFO("Acive spp connect\n");
    } else if (g_bqb_spp_cntx.spp_id == 0) {
        g_bqb_spp_cntx.spp_id = spp_id;
        BT_INFO("Passive spp connect spp_id %d\n", spp_id);
    } else {
        BT_ERR("Can't find free spp channel\n");
    }
}

static void bqb_spp_disconnected_cb(struct bt_conn *conn, uint8_t spp_id)
{
    BT_INFO("bqb spp disconnected cb: conn:%p, spp_id: %d\n", conn, spp_id);

    if (g_bqb_spp_cntx.spp_id == spp_id) {
        g_bqb_spp_cntx.spp_id = 0;
        bqb_gap_disconnect_acl(g_bqb_spp_cntx.conn);
    } else {
        BT_ERR("Can't find spp spp_id\n");
    }
}

static void bqb_spp_recv_cb(struct bt_conn *conn, uint8_t spp_id, uint8_t *data, uint16_t len)
{
    BT_INFO("spp_id %d, len:%d\n", spp_id, len);
}

static void bqb_spp_start(void)
{
    hostif_bt_conn_cb_register(&bqb_spp_acl_conn_cbs);
    hostif_bt_spp_register_service((uint8_t *)bqb_spp_uuid);
    hostif_bt_spp_register_cb(&bqb_spp_cbs);
    hostif_bt_conn_auth_cb_register(&bqb_spp_auth_display_yesno_cbs);
	bqb_gap_write_scan_enable(BQB_GAP_BOTH_INQUIRY_PAGE_SCAN);
}

static void bqb_spp_stop(void)
{
    bqb_gap_write_scan_enable(BQB_GAP_NO_SCANS);
    hostif_bt_conn_auth_cb_register(NULL);
    hostif_bt_conn_cb_unregister(&bqb_spp_acl_conn_cbs);
    hostif_bt_spp_register_cb(NULL);
}

int bqb_spp_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        //SYS_LOG_INF("Used: bqb spp <action> [params]\n");
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "start")) {
        bqb_spp_start();
    } else if (!strcmp(cmd, "stop")) {
        bqb_spp_stop();
    } else if (!strcmp(cmd, "conn")) {
        cmd = argv[2]; /*Addr format: 12:34:56:78:9a*/
        g_bqb_spp_cntx.conn = bqb_gap_connect_br_acl(cmd);
    } else if (!strcmp(cmd, "disc")) {
        hostif_bt_spp_disconnect(g_bqb_spp_cntx.spp_id);
    } else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}

