/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief rfcomm for bqb test.
 */

#include <acts_bluetooth/host_interface.h>
#include <acts_bluetooth/rfcomm.h>

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "bqb_rfcomm.h"

static void bqb_rfcomm_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_rfcomm_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static void bqb_rfcomm_dlc_connected_cb(struct bt_rfcomm_dlc *dlc);
static void bqb_rfcomm_dlc_disconnected_cb(struct bt_rfcomm_dlc *dlc);
static void bqb_rfcomm_dlc_recv_cb(struct bt_rfcomm_dlc *dlc, struct net_buf *buf);

#define BQB_RFCOMM_DEFAULT_CHANNEL_ID 0x02
#define BQB_RFCOMM_DEFAULT_MTU 127

struct bqb_rfcomm_context {
    struct bt_conn *conn;
    struct bt_rfcomm_dlc dlc;
};

static struct bt_conn_cb bqb_rfcomm_acl_conn_cbs = {
    .connected = bqb_rfcomm_acl_connected_cb,
    .disconnected = bqb_rfcomm_acl_disconnected_cb,
};

static struct bt_rfcomm_dlc_ops bqb_rfcomm_ops = {
    .connected = bqb_rfcomm_dlc_connected_cb,
    .disconnected = bqb_rfcomm_dlc_disconnected_cb,
    .recv = bqb_rfcomm_dlc_recv_cb,
};

static struct bqb_rfcomm_context bqb_rfc_cntx = {0};

static void bqb_rfcomm_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    int ret;

    if (0 == err) {
        bqb_rfc_cntx.dlc.ops = &bqb_rfcomm_ops;
        bqb_rfc_cntx.dlc.mtu = BQB_RFCOMM_DEFAULT_MTU;
        ret = bt_rfcomm_dlc_connect(bqb_rfc_cntx.conn, &(bqb_rfc_cntx.dlc), BQB_RFCOMM_DEFAULT_CHANNEL_ID);
        BT_INFO("bqb rfcomm dlc conn ret:%d\n", ret);
    } else {
        BT_INFO("bqb rfcomm acl connected, err:%d\n", err);
    }
}

static void bqb_rfcomm_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    BT_INFO("bqb rfcomm acl disconnected, reason:%d\n", reason);
    bqb_rfc_cntx.conn = NULL;
}

static void bqb_rfcomm_dlc_connected_cb(struct bt_rfcomm_dlc *dlc)
{
    BT_INFO("bqb rfcomm dlc connected cb\n");
}

static void bqb_rfcomm_dlc_disconnected_cb(struct bt_rfcomm_dlc *dlc)
{
    BT_INFO("bqb rfcomm dlc disconnected cb\n");
    bqb_gap_disconnect_acl(bqb_rfc_cntx.conn);
}

static void bqb_rfcomm_dlc_recv_cb(struct bt_rfcomm_dlc *dlc, struct net_buf *buf)
{
    BT_INFO("bqb rfcomm dlc recv cb\n");
}

static void bqb_rfcomm_start(void)
{
    hostif_bt_conn_cb_register(&bqb_rfcomm_acl_conn_cbs);
}

static void bqb_rfcomm_stop(void)
{
    hostif_bt_conn_cb_unregister(&bqb_rfcomm_acl_conn_cbs);
}

int bqb_rfcomm_test_command_handler(int argc, char *argv[])
{
    char *cmd = NULL;

    if (argc < 2) {
        //SYS_LOG_INF("Used: bqb rfcomm <action> [params]\n");
        return -EINVAL;
    }

    cmd = argv[1];

    if (!strcmp(cmd, "start")) {
        bqb_rfcomm_start();
    } else if (!strcmp(cmd, "stop")) {
        bqb_rfcomm_stop();
    } else if (!strcmp(cmd, "conn")) {
        cmd = argv[2]; /*addr format: 12:34:56:78:9a*/
        bqb_rfc_cntx.conn= bqb_gap_connect_br_acl(cmd);
    } else if (!strcmp(cmd, "disc")) {
        bt_rfcomm_dlc_disconnect(&(bqb_rfc_cntx.dlc));
    } else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}

