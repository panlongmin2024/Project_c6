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

#include "rfcomm_internal.h"

#include "bqb_utils.h"
#include "bqb_gap.h"
#include "bqb_rfcomm.h"

static void bqb_rfcomm_acl_connected_cb(struct bt_conn *conn, u8_t err);
static void bqb_rfcomm_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static void bqb_rfcomm_dlc_connected_cb(struct bt_rfcomm_dlc *dlc);
static void bqb_rfcomm_dlc_disconnected_cb(struct bt_rfcomm_dlc *dlc);
static void bqb_rfcomm_dlc_recv_cb(struct bt_rfcomm_dlc *dlc, struct net_buf *buf);
static void bqb_rfcomm_passkey_display_cb(struct bt_conn *conn, unsigned int passkey);
static void bqb_rfcomm_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey);


#define BQB_RFCOMM_DEFAULT_CHANNEL_ID 0x06
#define BQB_RFCOMM_DEFAULT_MTU 127

struct bqb_rfcomm_context {
    struct bt_conn *conn;
    struct bt_rfcomm_dlc dlc;
	bool conn_rfcomm;
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

static const struct bt_conn_auth_cb bqb_rfcomm_auth_display_yesno_cbs = {
    .passkey_display = bqb_rfcomm_passkey_display_cb,
    .passkey_entry = NULL,
    .passkey_confirm = bqb_rfcomm_passkey_confirm_cb,
    .pincode_entry = NULL,
    .cancel = NULL,
    .pairing_confirm = NULL,
};

static struct bqb_rfcomm_context bqb_rfc_cntx = {0};

static void bqb_rfcomm_passkey_display_cb(struct bt_conn *conn, unsigned int passkey)
{
    if (conn != bqb_rfc_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", bqb_rfc_cntx.conn, conn);
    }
}

static void bqb_rfcomm_passkey_confirm_cb(struct bt_conn *conn, unsigned int passkey)
{
    if (conn != bqb_rfc_cntx.conn) {
        BT_ERR("conn error, connected: %p, curr: %p\n", bqb_rfc_cntx.conn, conn);
        return;
    }
	int ret = bt_conn_auth_passkey_confirm(bqb_rfc_cntx.conn);
    BT_INFO("passkey cfm ret: %d\n", ret);
}

static int bqb_rfcomm_server_accept(struct bt_conn *conn, struct bt_rfcomm_dlc **dlc, uint8_t channel)
{
	BT_INFO("bqb rfcomm accept chl: %d\n", channel);
	*dlc = &bqb_rfc_cntx.dlc;
	bqb_rfc_cntx.dlc.ops = &bqb_rfcomm_ops;
    bqb_rfc_cntx.dlc.mtu = BQB_RFCOMM_DEFAULT_MTU;
	return 0;
}

static struct bt_rfcomm_server s_bqb_rfcomm_server =
{
	.channel = BQB_RFCOMM_DEFAULT_CHANNEL_ID,
	.accept = bqb_rfcomm_server_accept,
};

static void bqb_rfcomm_connect_channel()
{
	bqb_rfc_cntx.dlc.ops = &bqb_rfcomm_ops;
    bqb_rfc_cntx.dlc.mtu = BQB_RFCOMM_DEFAULT_MTU;
    int ret = bt_rfcomm_dlc_connect(bqb_rfc_cntx.conn, &(bqb_rfc_cntx.dlc), s_bqb_rfcomm_server.channel);
	BT_INFO("bqb rfcomm dlc conn ret:%d\n", ret);
}

static void bqb_rfcomm_acl_connected_cb(struct bt_conn *conn, u8_t err)
{
    if (0 == err) {
		bqb_rfc_cntx.conn = conn;
        BT_INFO("bqb rfcomm acl conn\n");
        if (bqb_rfc_cntx.conn_rfcomm) {
            bqb_rfcomm_connect_channel();
        }
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
}

static void bqb_rfcomm_dlc_recv_cb(struct bt_rfcomm_dlc *dlc, struct net_buf *buf)
{
    BT_INFO("bqb rfcomm dlc recv cb\n");
}

static void bqb_rfcomm_start(void)
{
    hostif_bt_conn_cb_register(&bqb_rfcomm_acl_conn_cbs);
    bt_rfcomm_server_register(&s_bqb_rfcomm_server);
    hostif_bt_conn_auth_cb_register(&bqb_rfcomm_auth_display_yesno_cbs);
    bqb_gap_write_scan_enable(BQB_GAP_BOTH_INQUIRY_PAGE_SCAN);
}

static void bqb_rfcomm_send_data(uint32_t len)
{
	struct net_buf * buf = bt_rfcomm_create_pdu(NULL);
	uint8_t hf_data[] = {0x0D, 0x0A, 0x41, 0x54, 0x44, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x0D, 0x0A};
	net_buf_add_mem(buf, hf_data, sizeof(hf_data));
	net_buf_add(buf, len - sizeof(hf_data));

	int ret = bt_rfcomm_dlc_send(&bqb_rfc_cntx.dlc, buf);
	BT_INFO("bqb rfcomm senddata ret: %d", ret);
}

static void bqb_rfcomm_stop(void)
{
    bqb_gap_write_scan_enable(0);
    hostif_bt_conn_auth_cb_register(NULL);
    hostif_bt_conn_cb_unregister(&bqb_rfcomm_acl_conn_cbs);
    memset(&bqb_rfc_cntx, 0, sizeof(bqb_rfc_cntx));
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
		if (bqb_rfc_cntx.conn) {
			bqb_rfcomm_connect_channel();
		} else {
        	bqb_rfc_cntx.conn= bqb_gap_connect_br_acl(cmd);
		}
    } else if (!strcmp(cmd, "disc")) {
        bt_rfcomm_dlc_disconnect(&(bqb_rfc_cntx.dlc));
    } else if (!strcmp(cmd, "setchl")) {
		s_bqb_rfcomm_server.channel = bqb_utils_atoi(argv[2]);
	} else if (!strcmp(cmd, "sddata")){
		bqb_rfcomm_send_data(bqb_utils_atoi(argv[2]));
	} else if (!strcmp(cmd, "auto_conn_chl")) {
		if (bqb_utils_atoi(argv[2])) {
			bqb_rfc_cntx.conn_rfcomm = true;
		} else {
			bqb_rfc_cntx.conn_rfcomm = false;
		}
	} else if (!strcmp(cmd, "disc_acl")) {
		bqb_gap_disconnect_acl(bqb_rfc_cntx.conn);
	} else if (!strcmp(cmd, "rls")) {
		bt_rfcomm_send_rls(&bqb_rfc_cntx.dlc, bqb_utils_atoi(argv[2]), bqb_utils_atoi(argv[3]));
	} else {
        printk("The cmd is not existed!\n");
    }

    return 0;
}

